# Calendar for Office 365 (Pebble)

A Pebble watchapp (built for the Pebble Time 2) that shows your Microsoft 365 / Outlook calendar day by day and can show a morning briefing on the watch. There is no server: Microsoft Graph → phone (PebbleKit JS) → watch. Your sign-in tokens never leave your phone.

Not an official Microsoft product. Works best with personal Microsoft accounts (outlook.com, hotmail) and small organizations – see [Work accounts](docs/help.md#work-and-school-accounts).

- **User help:** [docs/help.md](docs/help.md)
- **Privacy policy:** [docs/privacy.md](docs/privacy.md)
- **Store listing:** [store/listing.md](store/listing.md)
- Architecture, protocol and design decisions: [CLAUDE.md](CLAUDE.md)

## Features

- Day view with up to 12 events per day; today and tomorrow are cached for offline use
- Swipe left/right to change the day, smooth scrolling, tap to open (Pebble Time 2); long-press up/down on all watches
- Detail view with the event description from Outlook (loaded on demand)
- Optional daily briefing at a set time on chosen weekdays
- Optional: move your own events (1 hour / 1 day / 2 days / 1 week later, next Monday)
- Configurable colors, German and English (automatic by phone language)
- Closes itself after a few minutes without use (configurable) to save battery

## 1. Register the app in Microsoft Entra ID (once, about 5 minutes)

1. <https://entra.microsoft.com> → **Identity → Applications → App registrations → New registration**
2. Name: e.g. `Pebble Calendar` (keep "Pebble" in the name – users see it on the Microsoft sign-in page)
3. Supported account types:
   - Only you with a work/school account: **"Accounts in this organizational directory only"**
   - Everyone (including outlook.com): **"Accounts in any organizational directory and personal Microsoft accounts"**
4. Redirect URI: leave empty → **Register**
5. **Authentication → Advanced settings → "Allow public client flows" = Yes** → Save
6. **API permissions → Add a permission → Microsoft Graph → Delegated permissions → `Calendars.Read`** (`offline_access` is included automatically). For "Move events" also add **`Calendars.ReadWrite`** (only requested when the feature is turned on). If you are an admin of your own tenant, click **"Grant admin consent"**.
7. Copy **Overview → "Application (client) ID"** into [`src/pkjs/index.js`](src/pkjs/index.js):
   ```js
   var CLIENT_ID = 'your-client-id';
   var AUTHORITY = 'common';   // for "this organizational directory only": your tenant ID
   ```

## 2. Set up the Pebble SDK (WSL2 on Windows, or Linux/macOS)

```bash
curl -LsSf https://astral.sh/uv/install.sh | sh
uv tool install pebble-tool
pebble sdk install latest
```

## 3. Build and install

```bash
pebble build
# Emulator (Pebble Time 2):
pebble install --emulator emery
pebble emu-app-config --emulator emery   # settings page in the emulator
# Real watch: enable the developer connection in the Pebble app on your phone, then
pebble install --phone <phone-IP>
```

Alternatively copy `build/*.pbw` to your phone and open it with the Pebble app.

## 4. First sign-in

1. Open the app on the watch. Without a sign-in the watch shows an address and a code.
2. Open `microsoft.com/devicelogin` on a PC or phone, enter the code and sign in with your Microsoft account.
3. **Keep the app open on the watch meanwhile.** Your events appear afterwards.

Settings (calendars, language, colors, briefing, moving events) are in the Pebble app on your phone, in the watchapp's settings.

## Usage

- **Day view:** up to 12 events of one day. The top bar always shows the date of the displayed day; below it a status line appears only when needed ("Updating...", "Offline - as of HH:MM", errors).
- **Buttons:** up/down select an event (the first press only shows the selection), **long-press up/down = previous/next day**, Select = details, Back = exit.
- **Touch (Pebble Time 2, can be turned off in the settings):** swipe left = next day, right = previous day, up/down = smooth scrolling, tap = open. Today and tomorrow are available instantly (also offline); other days are loaded from the phone. Smooth scrolling and tapping need **touch navigation** enabled in the watch's system settings.
- **Event details:** include the description from Outlook, loaded from the phone when opened. Without the phone nearby the watch says so.
- **Moving events (optional):** turn on "Allow moving" in the settings (the watch shows a new sign-in code once, because write access is needed). In the detail view of your own event press Select → pick an option (e.g. "1 day later – Fri Oct 2 09:00") → confirm.
- **Refresh:** automatically once when the app opens.
- **Battery:** the app uses nothing when closed. While open, the touch sensor costs the most, so the app closes itself after 5 minutes without use by default ("Close automatically": never / 2 / 5 / 10 minutes).
- **Switch account:** "Sign out" in the settings, then reopen the app.
- **Briefing:** at the set time the app opens, vibrates, loads fresh data (otherwise the cache) and closes itself after 60 s without interaction.

## Error messages

| Message | Meaning |
|---|---|
| Client ID missing | Put your `CLIENT_ID` into `src/pkjs/index.js` |
| App registration invalid | "Allow public client flows" not enabled or wrong client ID |
| Please reopen the app to sign in | Token expired or revoked → close and reopen the app, sign-in starts automatically |
| Your IT admin must approve this app | Admin consent required (AADSTS65001/90094) |
| Company policy blocks this sign-in | Conditional Access blocks the device code flow; no workaround |
| Offline - as of HH:MM | Phone or internet not reachable; cached events are shown |

## Screenshots (demo mode)

```bash
sh tools/screenshots.sh            # emery basalt diorite flint
sh tools/screenshots.sh emery      # one platform
```

Builds a version with neutral English sample events (no sign-in, no network: `DEMO = true` in `src/pkjs/index.js`, data in `src/pkjs/demo.js`), installs it in the emulator and saves a screenshot after each step to `store/screenshots/`. Afterwards it switches demo mode off and builds the normal version again.

## Tests

```bash
TZ=Europe/Vienna node test/lib.test.js    # JS logic
TZ=Europe/Vienna node test/index.sim.js   # sign-in → Graph → watch flow (mocked)
sh test/c/run.sh                          # C compile with SDK flags (all platforms) + native watch logic
```

The C checks use a stub of `pebble.h`; they do not replace a real `pebble build`.
