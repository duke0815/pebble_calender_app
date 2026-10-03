# CLAUDE.md – Calendar for Office 365 (Pebble app)

## Project goal
Pebble watchapp that shows Office 365 calendar events day by day (day view, swipe to page) and can optionally show an event briefing on the watch every day at a fixed time.

**Priority:** Built primarily for the owner's own use (Pebble Time 2, own Microsoft account) and now being prepared for the Pebble app store – there, every user signs in with their own Microsoft account. Work-account topics (see "Work accounts & limitations") are secondary.

**Core principle: no own server.** Data flows only Microsoft Graph → phone (PebbleKit JS) → watch. Tokens stay on the user's phone.

**Language:** Project files (code comments, README, docs, store text, this file) are in English. The app itself is bilingual (German/English). The owner talks to Claude in German – reply in German.

## Git
- **Always commit and push directly to `main`** (owner's wish). No feature branches, no pull requests unless explicitly requested.
- Run the tests before every commit (see "Architecture").

## Platform & tooling
- Target device: Pebble Time 2 (`emery`, 200×228, color, touch). Also built for `basalt`, `diorite` and `flint` (Pebble 2 Duo, 144×168, black/white, no touch – like diorite); layout is computed relative to layer bounds, not fixed pixels. Platform-specific sizes only via `PBL_PLATFORM_EMERY` (everything else uses the smaller values).
- Pebble SDK: C (watch) + PebbleKit JS (phone) + Clay (settings page).
- Development on Windows → run the Pebble SDK in WSL2 (`uv tool install pebble-tool`, then `pebble sdk install latest`).
- No heavy JS dependencies besides Clay (`@rebble/clay`, the maintained successor of `pebble-clay` – the old 1.0.4 does not support `flint`).
- Write PebbleKit JS in ES5 (no arrow functions, no `let`/`const`, no Promises).

## Architecture
```
src/c/main.c         Entry point, wakeup, inactivity, paging days, touch gestures
src/c/events.c/.h    Cache (today+tomorrow), day buffer, day view
src/c/comm.c/.h      AppMessage receive/send
src/c/settings.c/.h  Watch settings (briefing, ...) + wakeup scheduling
src/c/idle.c/.h      Exit the app after a time without interaction
src/c/ui_main.c/.h   Main window: date bar, status line + MenuLayer
src/c/ui_detail.c/.h Event detail view
src/c/ui_login.c/.h  Device code display
src/c/ui_action.c/.h Actions via Select in the details: move / respond menu, confirmation, result
src/c/protocol.h     Shared constants (MSG_TYPE, STATUS_CODE, lengths)
src/c/format.c/.h    Time/date formatting (DE/EN)
src/c/i18n.c/.h      Watch texts (German/English), tr(STR_...)
src/c/theme.c/.h     Colors from the settings + derived contrast colors
src/pkjs/index.js    PebbleKit JS: control flow, auth, Graph, sending to the watch
src/pkjs/lib.js      Pure logic (times, filters, truncation, error classification) – testable with Node
src/pkjs/config.js   Clay configuration (built dynamically)
src/pkjs/i18n.js     Texts for status messages and the settings page (German/English)
src/pkjs/demo.js     Demo mode: fake Graph with neutral English sample events (for screenshots)
tools/screenshots.sh Builds the demo version, takes emulator screenshots, rebuilds the normal version
test/lib.test.js     Node tests for lib.js + config.js
test/index.sim.js    Simulation of index.js (Pebble/XHR/localStorage mocked)
test/c/run.sh        C compile with SDK flags (-std=c99 -Os -Werror, -fshort-enums like ARM) against a stub pebble.h + native logic tests
docs/help.md         User help (linked from the store)
docs/privacy.md      Privacy policy
store/listing.md     Store texts
store/screenshots/   Store screenshots (from tools/screenshots.sh)
store/assets/        Store icons (144, 48) and banner (720x320) – from tools/store_assets.py (Pillow)
```

Before every commit: `TZ=Europe/Vienna node test/lib.test.js && TZ=Europe/Vienna node test/index.sim.js && sh test/c/run.sh`. The stub (`test/c/stub/pebble.h`) must be extended for new SDK calls – with the real signatures.

Time calculations on the watch only via `time_make_local()` / `time_day_start()` (corrects the DST offset of `mktime`), never `mktime` directly.

## Authentication (Microsoft Entra ID)
- App registration: **multi-tenant + personal Microsoft accounts**, public client ("Allow public client flows" = Yes), no client secret.
- Flow: **device code flow**
  - `POST https://login.microsoftonline.com/{AUTHORITY}/oauth2/v2.0/devicecode`
  - `POST https://login.microsoftonline.com/{AUTHORITY}/oauth2/v2.0/token` (poll at the given interval, `slow_down` → +5 s)
  - `AUTHORITY` as a constant: default `common`; for own use the own tenant ID can be entered.
- Scopes: `https://graph.microsoft.com/Calendars.Read offline_access`; **only when "Allow moving" or "Respond to invitations" is on** (`lib.needsWriteAccess`) `Calendars.ReadWrite offline_access` (incremental consent). `auth.rw` records whether the token response contained `Calendars.ReadWrite`; if it is missing while write access is needed, the tokens are discarded and sign-in restarts (code on the watch). Refresh requests the scope the token has.
- The code is shown **on the watch**: "microsoft.com/devicelogin – Code: XXXXXX". Sign-in starts automatically on app start when there is no token. There is **no menu entry** for it; switching accounts = "Sign out" in the settings, then reopen the app. The app must stay open during sign-in (PebbleKit JS only runs while the watchapp runs).
- Access token + refresh token + expiry in `localStorage` (PebbleKit JS). Refresh when less than 5 minutes remain.
- `invalid_grant` / `interaction_required` → delete tokens, watch shows "Please reopen the app to sign in." (sign-in starts automatically on the next start).
- **Demo mode** (`var DEMO = false;` in `index.js`): `getToken` returns a dummy token and `request()` is answered by `demo.js` (calendars, calendarView, description, PATCH in memory) – the rest of the pipeline is the real one. Language "auto" becomes English, moving is on until settings are saved. Only `tools/screenshots.sh` switches it on for its build; a test checks that the committed code has `DEMO = false`. Keep the sample data neutral (no real names/companies).
- Client ID as a constant in `src/pkjs/index.js`: `402716d7-8b06-443b-af50-c37348a787c4` (public client, not a secret). If it says `TODO`, the watch shows "Client ID missing".

## Microsoft Graph
- Calendar list: `GET /v1.0/me/calendars?$select=id,name,isDefaultCalendar` → cache in `localStorage` (refresh automatically at most once a day, otherwise when the settings are opened).
- Events: `GET /v1.0/me/calendars/{id}/calendarView?startDateTime=…&endDateTime=…&$select=id,subject,bodyPreview,start,end,isAllDay,location,isCancelled,showAs,responseStatus,isOrganizer,organizer,attendees&$orderby=start/dateTime&$top=50`
  - Without a calendar selection: `/v1.0/me/calendar/calendarView` (default calendar).
  - Always send the header `Prefer: outlook.timezone="UTC"`.
  - calendarView already expands recurring events – **do not build an own RRULE parser**.
  - Several selected calendars: query each, merge, sort by start, truncate.
- Query range:
  - **Cache list** (app start, briefing): today 00:00 to the day after tomorrow 00:00 (local). Tomorrow is cached too so the briefing next morning has data even offline. Max. 12 per day, max. 16 in total (today has priority).
  - **Day list** (paging on the watch): exactly one day, max. 12 events. Loaded only on demand and kept on the watch only in RAM.
- Times: Graph returns UTC without `Z` → convert to epoch seconds in JS and send them like that to the watch.
  - **All-day events:** use only the date, do not shift. Convention: `EVT_START` = **local midnight** of the start date, `EVT_END` = local midnight of the (exclusive) end date, both computed in JS.
- Filter out cancelled (`isCancelled`) and optionally declined events (`responseStatus.response == "declined"`); optionally hide all-day events.
- Handle pagination (`@odata.nextLink`).
- **Description (Outlook note):** `bodyPreview` (plain text, max. 255 chars) comes with the event list. JS cleans it up (`lib.cleanNotes`: cut the Teams/invitation block starting at `____`/"Microsoft Teams", collapse blank lines, keep line breaks, max. 300 chars / 400 bytes) and stores it under a 32-bit key (`lib.eventKey`, FNV-1a of the Graph ID) in `localStorage` (`notes`, max. 80 entries). If it is missing there, fallback `GET /me/events/{id}?$select=bodyPreview`. The watch only stores the key (`EVT_KEY`), not the text.
- Errors: 401 → refresh the token once and retry; 403 → "No access to calendar"; otherwise "Graph error <status>"; status 0/timeout → "No connection".

## Settings (Clay)
The Clay config is built **dynamically** from the cached calendar list when opened (`showConfiguration`) (`autoHandleEvents: false`, a new Clay instance per opening, delete `clay-settings` first – the defaults come from our own settings).

| Setting | Type |
|---|---|
| Calendars | Checkbox group (default: default calendar) |
| Show all-day events | Toggle (default on) |
| Hide declined events | Toggle (default on) |
| Gray out past events | Toggle (default **off** = all events the same color) |
| Touch control | Toggle (default on); off = no swiping/tapping, change the day only with buttons |
| Close automatically | Select: never / 2 / 5 / 10 minutes without interaction (default **5**, `autoClose`). Saves battery (touch sensor). Paused during sign-in; a briefing launch always closes after 60 s |
| Language | Select: Automatic (like phone, `navigator.language`) / Deutsch / English |
| Accent color | Color picker (default `0055AA` cobalt blue): date bar + selected event |
| Text color | Color picker (default `000000`) |
| Background color | Color picker (default `FFFFFF`) |
| Daily briefing | Toggle (default off) |
| Briefing time | Time (default 07:00) |
| Briefing weekdays | Checkbox group (default Mon–Fri) |
| Allow moving | Toggle (default **off**); on → write access, one-time new sign-in |
| Moving allowed for | Select: `solo` = own events without attendees (default) / `own` = all own events (attendees get an update) |
| Move options | Checkbox group: 1 hour / 1 day / 2 days / 1 week later / next Monday (default 1 day + 1 week, bitmask `moveOptions`) |
| Respond to invitations | Toggle (default **off**, `respondEnabled`); on → write access (one-time new sign-in unless moving already has it) |
| Sign out | Toggle → delete tokens |

Clay fills `meta.activeWatchInfo` only on the `ready` event – for the instance created per opening, `index.js` sets it itself (otherwise the color picker only shows black/white). Do not use `capabilities` in the config (older Clay versions did not know newer platforms).

Settings that concern the watch (briefing on/off, time, weekdays, language, colors, touch, gray past, auto close, move options) are sent via AppMessage (`MSG_TYPE = 5`) and persisted on the watch. JS also sends them at the start of every sync.

The watch stores its settings as one struct under one persist key. **Append new fields at the end only**: `settings_load()` accepts older, shorter data (new fields keep their defaults).

## AppMessage protocol
Send events **one by one** (message size limit), with a queue and ACK/NACK retry (up to 4 attempts, growing pause).

| Key | Type | Content |
|---|---|---|
| `MSG_TYPE` | int | Phone→watch: 1 = event, 2 = end of list, 3 = status/error, 4 = login code, 5 = settings, 6 = description, 7 = action result (move or response). Watch→phone: 10 = REQUEST (request data), 12 = request description, 13 = move, 14 = respond |
| `EVT_INDEX` / `EVT_COUNT` | int | Position / total (`EVT_COUNT` also with MSG_TYPE 2) |
| `EVT_START` / `EVT_END` | int32 | Epoch seconds (all-day: local midnight, see above) |
| `EVT_ALLDAY` | int | 0/1 |
| `EVT_TITLE` | cstring | max. 80 chars **and** max. 95 bytes UTF-8, truncated in JS (`...`) |
| `EVT_LOCATION` | cstring | max. 40 chars **and** max. 47 bytes UTF-8 |
| `EVT_KEY` | int32 | Event key (with MSG_TYPE 1, 6, 12) |
| `EVT_FLAGS` | int | with MSG_TYPE 1: bit 0 = movable, bit 1 = not organizer, bit 2 = has attendees, bit 3 = moving on at the phone, bit 4 = respondable, bits 5-6 = own response 0 none / 1 accepted / 2 tentative / 3 declined, bit 7 = responding on at the phone (`lib.eventFlags`) |
| `MOVE_OPT` | int | with MSG_TYPE 13: 0 = +1 hour, 1 = +1 day, 2 = +2 days, 3 = +1 week, 4 = next Monday (same time; event on a Monday → Monday a week later) |
| `RESP_OPT` | int | with MSG_TYPE 14: 0 = accept, 1 = tentative, 2 = decline (`lib.RESPONSES`) |
| `MOVE_OK` | int | with MSG_TYPE 7: 1 = done (moved/responded), 0 = error (text in `STATUS_TEXT`) |
| `SET_MOVE_OPTS` | int | Bitmask of allowed options, 0 = moving off |
| `NOTE_TEXT` | cstring | with MSG_TYPE 6: cleaned description (max. 400 bytes), empty = none |
| `LIST_DAY` / `LIST_DAYS` | int | with MSG_TYPE 2: local midnight (epoch) of the first day / number of days (2 = cache, 1 = single day) |
| `REQ_DAY` | int | with MSG_TYPE 10: 0 = reload cache, otherwise local midnight of the requested day (JS returns it unchanged as `LIST_DAY`) |
| `STATUS_CODE` | int | 1 = syncing, 2 = sign-in needed, 3 = admin consent needed, 4 = policy blocks, 5 = no connection, 6 = other error, 7 = configuration missing |
| `STATUS_TEXT` | cstring | Error/status text (short) |
| `LOGIN_URL` / `LOGIN_CODE` | cstring | Device code display |
| `SET_BRIEF_ENABLED` | int | 0/1 |
| `SET_BRIEF_HOUR` / `SET_BRIEF_MINUTE` | int | Briefing time |
| `SET_BRIEF_DAYS` | int | Bitmask, bit 0 = Sunday … bit 6 = Saturday (like `tm_wday`); Mon–Fri = 62 |
| `SET_LANG` | int | 0 = German, 1 = English (resolved from "Automatic" in JS) |
| `SET_GRAY_PAST` | int | 1 = past events gray, 0 = all the same (default) |
| `SET_TOUCH` | int | 1 = touch control on, 0 = off (the watch stores it inverted as `touch_off` so old settings mean "on") |
| `SET_AUTO_CLOSE` | int | Minutes without interaction until the app exits, 0 = never (default 5) |
| `SET_COLOR_ACCENT` / `SET_COLOR_TEXT` / `SET_COLOR_BG` | int | 0xRRGGBB, on the watch via `GColorFromHEX()` |

- **Exactly one sync per app start:** JS syncs on the `ready` event; the watch sends nothing at start but waits (status line "Updating...", timeout) and shows the cache until then. No manual refresh, no periodic reloading.
- The watch sends `MSG_TYPE = 10` when paging to a day outside the cache (`REQ_DAY` = day) and when the briefing becomes due while the app is open (`REQ_DAY` = 0). Otherwise JS only reloads after the settings are saved.
- JS processes jobs one after another: cache requests during a running cache sync are merged; for day requests only the most recent one counts. Day requests send no SYNCING status.
- The watch only accepts a new list if all `EVT_COUNT` events have arrived at `MSG_TYPE = 2` – otherwise the old cache stays.
- Every message except 4 closes the login window on the watch.
- Texts for the watch must not use the "…" character but `...` (font coverage).
- **Language:** All visible texts bilingual – watch via `tr(STR_...)` in `i18n.c`, phone via `i18n.t()` in `i18n.js`. Always add new texts in both languages. English date: "Thu Oct 1", German: "Do 01.10.". Before the first settings arrive the watch uses its system language (`i18n_get_system_locale()`).
- **Colors:** Text on the accent color, gray for past events and the status line are chosen automatically from the brightness of accent/background (`theme.c`). Black-and-white watches ignore the colors.

## Watchapp (C)
- **Top bar:** always the date of the displayed day ("Today, Thu Oct 1", "Tomorrow, …", "Sat Oct 3"). Below it, only when needed, a narrow status line (section header, otherwise height 0): "Updating...", "Loading day...", "Offline - as of HH:MM" or an error text. Normally no "as of".
- `MenuLayer` below, own row drawing: each event as a **card with a border** (color = text color, radius 4; past events only gray when "Gray out past events" is on) with ~0.5 mm gap in between (`CARD_GAP` 4 px on emery, 3 px otherwise); content: time span (or "All day"), title, location.
- **No selection at first:** Only the first button press (up/down/Select) shows the selection (filled card in the accent color) without moving it. Hidden again on day change. The MenuLayer highlight colors are therefore = normal colors; `prv_draw_event` draws the highlight itself.
- **Buttons (all platforms, own click config):** up/down = selection, **long press (400 ms) = previous/next day**, Select = details, Back = exit the app.
- **Day view:** always exactly one day (max. 12 events). Events that started before the day are included.
- **Touch (only `PBL_TOUCH`, Pebble Time 2):**
  - Vertical scrolling, tapping and "swipe right = back" in the detail view are done by the **PebbleOS touch navigation** (`app_touch_navigation_enable(true)`): content follows the finger 1:1, with momentum; tapping an event opens it. **No own stepped scrolling** (felt unpleasant). Only works when touch navigation is enabled in the watch settings.
  - Horizontal gestures in the day view are evaluated by the app (`touch_service_subscribe()`): left = next day, right = previous day. Touch navigation additionally reports "swipe right" as a BACK press → the main window has its own click config (like MenuLayer) and ignores BACK while the finger is down or the liftoff was < 600 ms ago; the real Back button exits the app.
  - Whether a gesture changes the day is decided by the window at **touchdown** (day view only). The system handles touch events before the app handler; "swipe right" in the detail view closes it before our liftoff – otherwise it would also page one day back.
  - Paging range: −60 to +90 days. Without touch (setting off or other platform): change the day by long press.
  - Setting "Touch control" off → `touch_service_unsubscribe()` + `app_touch_navigation_enable(false)`.
  - On day change the list slides in (`ui_main_animate_day`: frame animation of the MenuLayer from half width, 220 ms, EaseOut; next day from the right, previous from the left). A running animation is aborted by another swipe.
- Days in the cache (today/tomorrow) appear instantly; other days are requested by the watch ("Loading day...", timeout 15 s; if the outbox is busy, up to 8 attempts 250 ms apart). Day lists are only accepted if they belong to the currently displayed day (late answers are discarded). At midnight the view follows if "today" was displayed.
- No action entries (no "Refresh"/"Sign in") – removed on purpose.
- Select/tap on an event → detail window (date, time, full title, location, **description**). The description is loaded when opened via `MSG_TYPE 12` ("Loading description...", timeout 8 s, send retry like for days). Without phone connection/timeout: "The description needs the phone nearby."; empty: "No description". Only in RAM, never in the persist cache. Watch AppMessage inbox: 640 bytes. Fonts there deliberately small: emery title GOTHIC_24_BOLD, time GOTHIC_18_BOLD, rest GOTHIC_18 (other platforms one step smaller).
- Minute tick → recompute the view (past events, day change).
- **Auto close (`idle.c`):** timer is restarted by every interaction (buttons in all windows, scrolling in the details, touch liftoff); when it fires the app exits (`window_stack_pop_all`). Timeout from `auto_close_min`, 60 s on a briefing launch, paused while the login code is shown.
- **Cache:** each event under its own persist key (256-byte limit per key) + count + timestamp of the last sync + cache version. Show the cache immediately at start, then update.
- **Storage budget:** Persistent storage is about 4 KB per app in total. One event takes 160 bytes → **max. 16 events** in the cache (~2.6 KB) plus settings. Do not increase without recalculating. Day buffer (12 events) only in RAM.
- Empty state: "No events today" or "No events"; on errors the error text; without data/connection "No connection to the phone."
- Timeout for a sync: 30 s (except during sign-in), then "Offline".

## Moving events (opt-in)
- Only when enabled in the settings. Movable (`lib.isMovable`): `isOrganizer` and – with "Moving allowed for" = `solo` – no attendees (rooms count as attendees). Never invitations from others.
- Attendees are counted without the organizer (`lib.countAttendees`: address = `organizer.emailAddress` or `status.response == 'organizer'` is ignored) – Outlook sometimes lists the organizer in `attendees`; otherwise nothing would be movable in `solo` mode (happened in practice).
- `EVT_FLAGS` bits for diagnostics: 1 = movable, 2 = not organizer, 4 = has attendees, 8 = moving on at the phone. With moving on, the detail view shows why an event cannot be moved (gray), or "settings missing on the watch".
- Watch: detail view shows "Select: Move" (accent color); Select → action menu (`ui_action`, section "Move"): list of the enabled options with target date/time (+1 hour not for all-day) → confirmation "Move to …?" (Select = yes, Back = no) → "Moving..." (timeout 12 s, send retry) → "Moved to …" + vibration, back to the day view after 1.5 s; errors stay visible (double vibration).
- Phone: checks permission again (index `notes` stores id/start/end/allDay/movable), then `PATCH /me/events/{id}` with new start/end (UTC; days keep the local time, also across DST; all-day: date + `T00:00:00`, `timeZone: UTC`). Then `sync(true)`; the watch discards the day buffer and reloads the displayed day.
- Recurring events: only the single occurrence is moved (Graph creates an exception).

## Responding to invitations (opt-in)
- Only when "Respond to invitations" is enabled. Respondable (`lib.isRespondable`): every event where the user is **not** the organizer. Own events are never respondable, invitations never movable – so the action menu has either the "Move" or the "Respond" section (the code supports both).
- With moving on but responding off, the details of an invitation say "Invitation: turn on responding in the settings" (bit 7) instead of only "cannot move".
- Own response from `responseStatus.response` → `EVT_FLAGS` bits 5-6. The detail view shows "Your response: Accepted" / "Not responded yet" and "Select: Respond".
- Watch: Select → action menu section "Respond": Accept / Tentative / Decline ("Current response" as subtitle) → confirmation "Accept?\nThe organizer will be notified." → "Sending response..." → "Accepted" etc.; same timeouts/retry/close behavior as moving, result via MSG_TYPE 7.
- Phone: checks permission again (`notes` index stores `respondable`), then `POST /me/events/{id}/{accept|tentativelyAccept|decline}` with `{"sendResponse": true}` (202). Then `sync(true)`; declined events disappear when "Hide declined events" is on.
- Recurring events: only the single occurrence is answered.

## Daily briefing (Wakeup API)
- `wakeup_schedule()` only ever for the **next** briefing time (respects weekdays); `wakeup_cancel_all()` first.
- Reschedule: on every app start, after every settings change and on every wakeup.
- Collision (`E_RANGE`): up to 5 attempts, +1 minute each.
- Launch with `APP_LAUNCH_WAKEUP` (or wakeup while the app is open, then jump to today): short vibration, request fresh data (timeout 10 s), otherwise show the cache with "Offline - as of …". The app closes itself after 60 s without interaction (wakeup launch only).
- Evening caching is covered by the query range "today + tomorrow".

## Work accounts & limitations
Sign-in usually works for personal accounts (outlook.com/hotmail) and small organizations without own policies. Organization tenants have three independent hurdles:

1. **Consent:** User consent is often disabled or limited to "verified publishers + low-risk permissions"; `Calendars.Read` is not low-risk by default. Since 11/2020 users can no longer consent to new multi-tenant apps without a verified publisher.
   → `AADSTS65001`, `AADSTS90094`, `AADSTS90095` → STATUS 3, "Your IT admin must approve this app."
2. **Conditional Access:** The device code flow can be blocked by policy ("authentication flows") – Microsoft recommends this because of phishing abuse. "Compliant device" usually cannot be satisfied in the device code flow; "approved client app" and location rules block as well.
   → `AADSTS53000`, `AADSTS53001`, `AADSTS53002`, `AADSTS53003`, `AADSTS50097` → STATUS 4, "Company policy blocks this sign-in." There is no workaround in the app.
3. **Token lifetime:** "Sign-in frequency" policies force regular re-sign-in (`interaction_required`/`invalid_grant`) → STATUS 2. Practically unusable for the briefing.

| Account type | Expectation |
|---|---|
| Personal Microsoft account | works |
| Small business without own policies | mostly ok, depends on the consent setting |
| Mid-size company with IT provider | uncertain |
| Enterprise with Intune / Conditional Access | almost certainly fails |

Positioning for publication: primarily personal accounts and small organizations, not "Office 365 in the enterprise". Before the store: test sign-in with a foreign test tenant (including an enabled device code block).

## Non-goals
- No own server/proxy, no timeline pins.
- No write access to the calendar – **only exceptions:** "Moving events" (opt-in, own events only) and "Responding to invitations" (opt-in). No creating, deleting, editing titles.
- No other calendar sources (Google, CalDAV) in version 1. Graph-specific code stays encapsulated in `index.js`/`lib.js` so other sources can be added later.

## Implementation order
1. ✅ Watchapp skeleton + MenuLayer
2. ✅ Device code flow + token handling in PebbleKit JS
3. ✅ Graph query + AppMessage transfer
4. ✅ Cache on the watch
5. ✅ Clay settings incl. dynamic calendar list
6. ✅ Several calendars; day view with paging by touch (replaces the "Today only"/"Next N" modes)
7. ✅ Daily briefing via wakeup
8. ✅ Error states (first version) – fine-tuning after testing on real hardware
9. ✅ Moving events, colors, English, auto close, flint, demo mode for screenshots
10. ✅ Action menu on Select + responding to invitations (built with the SDK, **not yet tested on the watch**)

Status: built by the owner with the real SDK and **tested in daily use on the Pebble Time 2** (display, touch, description, colors, language, moving own events with and without attendees work). The Claude environment can now reach `sdk.repebble.com` (allowed by the owner): `pebble sdk install latest`, `pebble build` and the emulator work there. One local fix is needed in a fresh container: the container has no IPv6, so pypkjs must bind IPv4 – in `pypkjs/runner/websocket.py` (pebble-tool's site-packages) replace `WSGIServer(("", self.port)` with `WSGIServer(("0.0.0.0", self.port)`; otherwise `pebble install --emulator` fails with "Connection refused".

## Before the store release
- Publisher verification in Entra ID (Microsoft partner ID) – a precondition for users in other tenants to consent themselves at all.
- Store description: clear note on the limitations with work accounts (see above) – `store/listing.md`.
- Privacy policy: no data processing by the developer, everything stays on the device – `docs/privacy.md`.
- Screenshots: ✅ `store/screenshots/` (20 PNGs, all platforms), regenerate with `sh tools/screenshots.sh` – fully automatic via `pebble emu-button`.

## App name
- **"Calendar for Office 365"** (`displayName` in `package.json`, heading of the settings page, README). "Office 365" is a Microsoft trademark – phrase it clearly as "for" in the store text (not an official Microsoft product).
