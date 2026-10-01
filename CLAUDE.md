# CLAUDE.md – Pebble-Kalender-App (Office 365)

## Projektziel
Pebble-Watchapp, die Office-365-Kalendertermine tageweise anzeigt (Tagesansicht, per Wischen blättern) und optional jeden Tag zu einer festen Uhrzeit ein Termin-Briefing auf der Uhr zeigt.

**Priorität:** Version 1 ist primär für den Eigengebrauch (Pebble Time 2, eigenes Microsoft-Konto). Eine Veröffentlichung im Pebble-App-Store ist später möglich – dann meldet sich jeder Nutzer mit seinem eigenen Microsoft-Konto an. Firmenkonten-Themen (s. „Firmenkonten & Einschränkungen“) werden erst danach angegangen.

**Grundprinzip: kein eigener Server.** Daten fließen ausschließlich Microsoft Graph → Handy (PebbleKit JS) → Uhr. Tokens bleiben am Handy des Nutzers.

## Git
- **Immer direkt auf `main` committen und pushen** (Wunsch des Projektinhabers). Keine Feature-Branches, keine Pull Requests, außer ausdrücklich verlangt.
- Vor jedem Commit die Tests laufen lassen (siehe „Architektur“).

## Plattform & Tooling
- Zielgerät: Pebble Time 2 (`emery`, 200×228, Farbe). Zusätzlich gebaut für `basalt` und `diorite`; Layout rechnet relativ zu den Layer-Bounds, nicht mit festen Pixeln.
- Pebble SDK: C (Uhr) + PebbleKit JS (Handy) + Clay (Einstellungsseite).
- Entwicklung unter Windows → Pebble SDK in WSL2 betreiben (`uv tool install pebble-tool`, dann `pebble sdk install latest`).
- Keine schweren JS-Abhängigkeiten außer Clay (`pebble-clay`).
- PebbleKit JS in ES5 schreiben (keine Arrow-Functions, kein `let`/`const`, keine Promises).

## Architektur
```
src/c/main.c         Einstieg, Wakeup, Inaktivitäts-Timer, Tage blättern, Touch-Gesten
src/c/events.c/.h    Cache (heute+morgen), Tagespuffer, Tagesansicht
src/c/comm.c/.h      AppMessage-Empfang/-Senden
src/c/settings.c/.h  Uhr-Settings (Briefing) + Wakeup-Planung
src/c/ui_main.c/.h   Hauptfenster: Statuszeile + MenuLayer
src/c/ui_detail.c/.h Detailansicht eines Termins
src/c/ui_login.c/.h  Anzeige des Device-Codes
src/c/protocol.h     Gemeinsame Konstanten (MSG_TYPE, STATUS_CODE, Längen)
src/pkjs/index.js    PebbleKit JS: Ablaufsteuerung, Auth, Graph, Senden an Uhr
src/pkjs/lib.js      Reine Logik (Zeiten, Filter, Kürzen, Fehlerklassifizierung) – mit Node testbar
src/pkjs/config.js   Clay-Konfiguration (wird dynamisch aufgebaut)
src/c/format.c/.h    Zeit-/Datumsformatierung (DE/EN)
src/c/i18n.c/.h      Texte der Uhr (Deutsch/Englisch), tr(STR_...)
src/c/theme.c/.h     Farben aus den Settings + abgeleitete Kontrastfarben
src/pkjs/i18n.js     Texte für Statusmeldungen und Einstellungsseite (Deutsch/Englisch)
test/lib.test.js     Node-Tests für lib.js + config.js
test/index.sim.js    Simulation von index.js (Pebble/XHR/localStorage gemockt)
test/c/run.sh        C-Kompilierung mit SDK-Flags (-std=c99 -Os -Werror, -fshort-enums wie ARM) gegen Stub-pebble.h + native Logiktests
```

Vor jedem Commit: `TZ=Europe/Vienna node test/lib.test.js && TZ=Europe/Vienna node test/index.sim.js && sh test/c/run.sh`. Der Stub (`test/c/stub/pebble.h`) muss bei neuen SDK-Aufrufen ergänzt werden – mit den echten Signaturen.

Zeitberechnung auf der Uhr nur über `time_make_local()` / `time_day_start()` (korrigiert DST-Versatz von `mktime`), nie `mktime` direkt.

## Authentifizierung (Microsoft Entra ID)
- App-Registrierung: **Multi-Tenant + persönliche Microsoft-Konten**, Public Client („Öffentliche Clientflows zulassen“ = Ja), kein Client Secret.
- Flow: **Device Code Flow**
  - `POST https://login.microsoftonline.com/{AUTHORITY}/oauth2/v2.0/devicecode`
  - `POST https://login.microsoftonline.com/{AUTHORITY}/oauth2/v2.0/token` (Polling im angegebenen Intervall, `slow_down` → +5 s)
  - `AUTHORITY` als Konstante: Standard `common`; für Eigengebrauch kann die eigene Tenant-ID eingetragen werden.
- Scopes: `https://graph.microsoft.com/Calendars.Read offline_access`
- Anzeige des Codes **auf der Uhr**: „microsoft.com/devicelogin – Code: XXXXXX“. Login startet automatisch beim App-Start, wenn kein Token vorhanden ist. Es gibt **keinen Menüpunkt** dafür; Kontowechsel = in den Einstellungen „Abmelden“, dann App neu öffnen. Während des Logins muss die App offen bleiben (PebbleKit JS läuft nur, solange die Watchapp läuft).
- Access Token + Refresh Token + Ablaufzeit in `localStorage` (PebbleKit JS). Erneuern, wenn weniger als 5 Minuten Restlaufzeit.
- `invalid_grant` / `interaction_required` → Tokens löschen, Uhr zeigt „Bitte App neu öffnen zum Anmelden.“ (beim nächsten Start startet der Login automatisch).
- Client-ID als Konstante in `src/pkjs/index.js`: `402716d7-8b06-443b-af50-c37348a787c4` (Public Client, kein Geheimnis). Steht dort `TODO`, zeigt die Uhr „Client-ID fehlt“.

## Microsoft Graph
- Kalenderliste: `GET /v1.0/me/calendars?$select=id,name,isDefaultCalendar` → in `localStorage` cachen (max. 1×/Tag automatisch erneuern, sonst beim Öffnen der Einstellungen).
- Termine: `GET /v1.0/me/calendars/{id}/calendarView?startDateTime=…&endDateTime=…&$select=id,subject,bodyPreview,start,end,isAllDay,location,isCancelled,showAs,responseStatus&$orderby=start/dateTime&$top=50`
  - Ohne Kalenderauswahl: `/v1.0/me/calendar/calendarView` (Standardkalender).
  - Header `Prefer: outlook.timezone="UTC"` immer mitsenden.
  - calendarView liefert Serientermine bereits aufgelöst – **keinen eigenen RRULE-Parser bauen**.
  - Mehrere ausgewählte Kalender: je Kalender abfragen, zusammenführen, nach Start sortieren, kürzen.
- Abfragezeitraum:
  - **Cache-Liste** (App-Start, Briefing): heute 00:00 bis übermorgen 00:00 (lokal). Morgen wird mitgecacht, damit das Briefing am nächsten Morgen auch offline Daten hat. Max. 12 pro Tag, insgesamt max. 16 (heute hat Vorrang).
  - **Tagesliste** (Blättern auf der Uhr): genau ein Tag, max. 12 Termine. Wird nur bei Bedarf geladen und auf der Uhr nur im RAM gehalten.
- Zeiten: Graph liefert UTC ohne `Z` → in JS in Epoch-Sekunden umrechnen und so an die Uhr schicken.
  - **Ganztägige Termine:** nur das Datum verwenden, nicht verschieben. Konvention: `EVT_START` = **lokale Mitternacht** des Startdatums, `EVT_END` = lokale Mitternacht des (exklusiven) Enddatums, beides in JS berechnet.
- Abgesagte (`isCancelled`) und optional abgelehnte Termine (`responseStatus.response == "declined"`) ausfiltern; ganztägige optional ausblenden.
- Pagination (`@odata.nextLink`) berücksichtigen.
- **Beschreibung (Outlook-Notiz):** `bodyPreview` (Klartext, max. 255 Zeichen) kommt mit der Terminliste. JS bereitet sie auf (`lib.cleanNotes`: Teams-/Einladungsblock ab `____`/„Microsoft Teams“ abschneiden, Leerzeilen zusammenfassen, Zeilenumbrüche behalten, max. 300 Zeichen / 400 Byte) und merkt sie sich mit einer 32-Bit-Kennung (`lib.eventKey`, FNV-1a der Graph-ID) in `localStorage` (`notes`, max. 80 Einträge). Fehlt sie dort, Fallback `GET /me/events/{id}?$select=bodyPreview`. Die Uhr speichert nur die Kennung (`EVT_KEY`), nicht den Text.
- Fehler: 401 → einmal Token erneuern und wiederholen; 403 → „Kein Zugriff auf Kalender“; sonst „Graph-Fehler <Status>“; Status 0/Timeout → „Keine Verbindung“.

## Einstellungen (Clay)
Clay-Config wird beim Öffnen (`showConfiguration`) **dynamisch** aus der gecachten Kalenderliste gebaut (`autoHandleEvents: false`, neue Clay-Instanz pro Öffnen, `clay-settings` vorher löschen – die Defaults kommen aus unseren eigenen Settings).

| Einstellung | Typ |
|---|---|
| Kalender | Checkbox-Gruppe (Default: Standardkalender) |
| Ganztägige Termine anzeigen | Toggle (Default an) |
| Abgelehnte Termine ausblenden | Toggle (Default an) |
| Vergangene Termine ausgrauen | Toggle (Default **aus** = alle Termine gleiche Farbe) |
| Touch-Bedienung | Toggle (Default an); aus = kein Wischen/Tippen, Tageswechsel nur per Tasten |
| Sprache | Auswahl: Automatisch (wie Handy, `navigator.language`) / Deutsch / English |
| Akzentfarbe | Farbwähler (Default `0055AA` Kobaltblau): Datumsleiste + ausgewählter Termin |
| Schriftfarbe | Farbwähler (Default `000000`) |
| Hintergrundfarbe | Farbwähler (Default `FFFFFF`) |
| Tägliches Briefing | Toggle (Default aus) |
| Briefing-Uhrzeit | Zeit (Default 07:00) |
| Briefing-Wochentage | Checkbox-Gruppe (Default Mo–Fr) |
| Abmelden | Toggle → Tokens löschen |

Clay füllt `meta.activeWatchInfo` nur beim `ready`-Event – bei der pro Öffnen neu erzeugten Instanz setzt `index.js` das selbst (sonst zeigt der Farbwähler nur Schwarz/Weiß). Keine `capabilities` in der Config verwenden (Clay kennt neuere Plattformen nicht).

Settings, die die Uhr betreffen (Briefing an/aus, Uhrzeit, Wochentage, Sprache, Farben), per AppMessage (`MSG_TYPE = 5`) an die Uhr schicken und dort persistieren. JS schickt sie zusätzlich zu Beginn jeder Synchronisierung.

## AppMessage-Protokoll
Termine **einzeln** senden (Message-Größenlimit), mit Warteschlange und ACK/NACK-Retry (bis 4 Versuche, wachsende Pause).

| Key | Typ | Inhalt |
|---|---|---|
| `MSG_TYPE` | int | Handy→Uhr: 1 = Termin, 2 = Ende der Liste, 3 = Status/Fehler, 4 = Login-Code, 5 = Settings, 6 = Beschreibung. Uhr→Handy: 10 = REQUEST (Daten anfordern), 12 = Beschreibung anfordern |
| `EVT_INDEX` / `EVT_COUNT` | int | Position / Gesamtzahl (`EVT_COUNT` auch bei MSG_TYPE 2) |
| `EVT_START` / `EVT_END` | int32 | Epoch-Sekunden (ganztägig: lokale Mitternacht, s. o.) |
| `EVT_ALLDAY` | int | 0/1 |
| `EVT_TITLE` | cstring | max. 80 Zeichen **und** max. 95 Byte UTF-8, in JS gekürzt (`...`) |
| `EVT_LOCATION` | cstring | max. 40 Zeichen **und** max. 47 Byte UTF-8 |
| `EVT_KEY` | int32 | Kennung des Termins (bei MSG_TYPE 1, 6, 12) |
| `NOTE_TEXT` | cstring | bei MSG_TYPE 6: aufbereitete Beschreibung (max. 400 Byte), leer = keine |
| `LIST_DAY` / `LIST_DAYS` | int | bei MSG_TYPE 2: lokale Mitternacht (Epoch) des ersten Tages / Anzahl Tage (2 = Cache, 1 = einzelner Tag) |
| `REQ_DAY` | int | bei MSG_TYPE 10: 0 = Cache neu laden, sonst lokale Mitternacht des gewünschten Tages (JS schickt sie als `LIST_DAY` unverändert zurück) |
| `STATUS_CODE` | int | 1 = Synchronisiere, 2 = Anmeldung nötig, 3 = Admin-Freigabe nötig, 4 = Richtlinie blockiert, 5 = Keine Verbindung, 6 = Sonstiger Fehler, 7 = Konfiguration fehlt |
| `STATUS_TEXT` | cstring | Fehler-/Statustext (deutsch, kurz) |
| `LOGIN_URL` / `LOGIN_CODE` | cstring | Device-Code-Anzeige |
| `SET_BRIEF_ENABLED` | int | 0/1 |
| `SET_BRIEF_HOUR` / `SET_BRIEF_MINUTE` | int | Briefing-Uhrzeit |
| `SET_BRIEF_DAYS` | int | Bitmaske, Bit 0 = Sonntag … Bit 6 = Samstag (wie `tm_wday`); Mo–Fr = 62 |
| `SET_LANG` | int | 0 = Deutsch, 1 = Englisch (in JS aus „Automatisch“ aufgelöst) |
| `SET_GRAY_PAST` | int | 1 = vergangene Termine grau, 0 = alle gleich (Default) |
| `SET_TOUCH` | int | 1 = Touch-Bedienung an, 0 = aus (Uhr speichert invertiert als `touch_off`, damit alte Settings „an“ bedeuten) |
| `SET_COLOR_ACCENT` / `SET_COLOR_TEXT` / `SET_COLOR_BG` | int | 0xRRGGBB, auf der Uhr per `GColorFromHEX()` |

- **Genau eine Synchronisierung pro App-Start:** JS synchronisiert beim `ready`-Event; die Uhr sendet beim Start nichts, sondern wartet (Statuszeile „Aktualisiere...“, Timeout) und zeigt bis dahin den Cache. Kein manuelles Aktualisieren, kein periodisches Nachladen.
- `MSG_TYPE = 10` sendet die Uhr beim Blättern zu einem Tag außerhalb des Caches (`REQ_DAY` = Tag) und wenn das Briefing bei offener App fällig wird (`REQ_DAY` = 0). Sonst lädt JS nur nach dem Speichern der Einstellungen neu.
- JS arbeitet Aufträge nacheinander ab: Cache-Anfragen während eines laufenden Cache-Syncs werden zusammengefasst; bei Tagesanfragen zählt nur die zuletzt angeforderte. Tagesanfragen senden keinen SYNCING-Status.
- Die Uhr übernimmt eine neue Liste nur, wenn bei `MSG_TYPE = 2` alle `EVT_COUNT` Termine angekommen sind – sonst bleibt der alte Cache.
- Jede Nachricht außer 4 schließt das Login-Fenster auf der Uhr.
- Texte für die Uhr kein „…“-Zeichen verwenden, sondern `...` (Font-Abdeckung).
- **Sprache:** Alle sichtbaren Texte zweisprachig – Uhr über `tr(STR_...)` in `i18n.c`, Handy über `i18n.t()` in `i18n.js`. Neue Texte immer in beiden Sprachen anlegen. Englisches Datum: „Thu Oct 1“, deutsches: „Do 01.10.“. Vor dem ersten Settings-Empfang nimmt die Uhr ihre Systemsprache (`i18n_get_system_locale()`).
- **Farben:** Schrift auf der Akzentfarbe, Grau für vergangene Termine und die Statuszeile werden aus Helligkeit von Akzent/Hintergrund automatisch gewählt (`theme.c`). Schwarz-Weiß-Uhren ignorieren die Farben.

## Watchapp (C)
- **Leiste oben:** immer das Datum des angezeigten Tages („Heute, Do 01.10.“, „Morgen, …“, „Sa 03.10.“). Darunter nur bei Bedarf eine schmale Statuszeile (Section-Header, sonst Höhe 0): „Aktualisiere...“, „Lade Tag...“, „Offline - Stand HH:MM“ oder Fehlertext. Im Normalfall kein „Stand“.
- `MenuLayer` darunter, eigene Zeilenzeichnung: jeder Termin als **Kasten mit Rahmen** (Farbe = Schriftfarbe, Radius 4; vergangene nur grau, wenn „Vergangene Termine ausgrauen“ an ist) und ~0,5 mm Luft dazwischen (`CARD_GAP` 4 px auf emery, 3 px sonst); Inhalt: Uhrzeit-Spanne (bzw. „Ganztägig“), Titel, Ort.
- **Keine Auswahl von Anfang an:** Erst der erste Tastendruck (hoch/runter/Select) blendet die Auswahl ein (gefüllter Kasten in Akzentfarbe), ohne sie zu bewegen. Beim Tageswechsel wieder aus. Die MenuLayer-Highlight-Farben sind deshalb = Normalfarben; die Hervorhebung zeichnet `prv_draw_event` selbst.
- **Tasten (alle Plattformen, eigene Click-Config):** hoch/runter = Auswahl, **lang drücken (400 ms) = vorheriger/nächster Tag**, Select = Details, Zurück = App beenden.
- **Tagesansicht:** immer genau ein Tag (max. 12 Termine). Termine, die vor dem Tag begonnen haben, erscheinen mit.
- **Touch (nur `PBL_TOUCH`, Pebble Time 2):**
  - Vertikales Scrollen, Tippen und „rechts wischen = zurück“ in der Detailansicht macht die **Touch-Navigation von PebbleOS** (`app_touch_navigation_enable(true)`): Inhalt folgt dem Finger 1:1, mit Schwung; Tippen auf einen Termin öffnet ihn. **Kein eigenes stufenweises Scrollen** (wurde als unangenehm empfunden). Funktioniert nur, wenn die Touch-Navigation in den Uhr-Einstellungen eingeschaltet ist.
  - Horizontal in der Tagesansicht wertet die App selbst aus (`touch_service_subscribe()`): links = nächster Tag, rechts = vorheriger Tag. Die Touch-Navigation meldet „rechts wischen“ zusätzlich als BACK-Taste → das Hauptfenster hat eine eigene Tastenbelegung (wie MenuLayer) und ignoriert BACK, solange der Finger liegt oder das Abheben < 600 ms her ist; echte Zurück-Taste beendet die App.
  - Ob eine Geste den Tag wechselt, entscheidet das Fenster beim **Aufsetzen** des Fingers (nur Tagesansicht). Das System verarbeitet Touch-Events vor dem App-Handler; „rechts wischen“ in der Detailansicht schließt diese schon vor unserem Liftoff – sonst würde zusätzlich ein Tag zurückgeblättert.
  - Blätterbereich: −60 bis +90 Tage. Ohne Touch (Einstellung aus oder andere Plattform): Tageswechsel per langem Tastendruck.
  - Einstellung „Touch-Bedienung“ aus → `touch_service_unsubscribe()` + `app_touch_navigation_enable(false)`.
  - Beim Tageswechsel gleitet die Liste herein (`ui_main_animate_day`: Frame-Animation des MenuLayers ab halber Breite, 220 ms, EaseOut; nächster Tag von rechts, vorheriger von links). Eine laufende Animation wird bei erneutem Wischen abgebrochen.
- Tage im Cache (heute/morgen) kommen sofort; andere Tage fordert die Uhr an („Lade Tag...“, Timeout 15 s; ist der Sendepuffer belegt, bis zu 8 Versuche im 250-ms-Abstand). Tageslisten werden nur übernommen, wenn sie zum gerade angezeigten Tag gehören (verspätete Antworten werden verworfen). Um Mitternacht springt die Ansicht mit, wenn „heute“ angezeigt war.
- Keine Aktions-Einträge (kein „Aktualisieren“/„Anmelden“) – bewusst entfernt.
- Select/Tippen auf einem Termin → Detailfenster (Datum, Zeit, voller Titel, Ort, **Beschreibung**). Die Beschreibung wird beim Öffnen per `MSG_TYPE 12` nachgeladen („Lade Beschreibung...“, Timeout 8 s, Sende-Retry wie beim Tag). Ohne Handy-Verbindung/Timeout: „Die Beschreibung benötigt die Nähe zum Handy.“; leer: „Keine Beschreibung“. Nur im RAM, nie im Persist-Cache. AppMessage-Inbox der Uhr: 640 Byte. Schrift dort bewusst klein: emery Titel GOTHIC_24_BOLD, Zeit GOTHIC_18_BOLD, Rest GOTHIC_18 (andere Plattformen eine Stufe kleiner).
- Minütlicher Tick → Ansicht neu berechnen (vergangene Termine, Tageswechsel).
- **Cache:** jeder Termin unter eigenem Persist-Key (256-Byte-Limit pro Key) + Anzahl + Zeitstempel des letzten Syncs + Cache-Version. Beim Start sofort Cache anzeigen, dann aktualisieren.
- **Speicherbudget:** Persistenter Speicher ist insgesamt ca. 4 KB pro App. Ein Termin belegt 160 Byte → **max. 16 Termine** im Cache (~2,6 KB) plus Settings. Nicht erhöhen, ohne nachzurechnen. Tagespuffer (12 Termine) nur im RAM.
- Leerer Zustand: „Keine Termine heute“ bzw. „Keine Termine“; bei Fehlern der Fehlertext; ohne Daten/Verbindung „Keine Verbindung zum Handy.“
- Timeout für eine Synchronisierung: 30 s (außer während des Logins), dann „Offline“.

## Tägliches Briefing (Wakeup-API)
- `wakeup_schedule()` immer nur für den **nächsten** Briefing-Zeitpunkt (berücksichtigt Wochentage); vorher `wakeup_cancel_all()`.
- Neu planen: bei jedem App-Start, nach jeder Settings-Änderung und bei jedem Wakeup.
- Kollision (`E_RANGE`): bis zu 5× je +1 Minute versuchen.
- Start mit `APP_LAUNCH_WAKEUP` (oder Wakeup bei offener App, dann Sprung auf heute): kurz vibrieren, frische Daten anfordern (Timeout 10 s), sonst Cache mit „Offline · Stand …“ anzeigen. App nach 60 s ohne Interaktion selbst schließen (nur beim Wakeup-Start).
- Abend-Caching ist über den Abfragezeitraum „heute + morgen“ abgedeckt.

## Firmenkonten & Einschränkungen
Für Privatkonten (outlook.com/hotmail) und kleine Firmen ohne eigene Richtlinien funktioniert der Login in der Regel. In Firmen-Tenants gibt es drei unabhängige Hürden:

1. **Einwilligung (Consent):** Nutzer-Einwilligung ist oft abgeschaltet oder auf „verifizierte Herausgeber + risikoarme Berechtigungen“ beschränkt; `Calendars.Read` zählt standardmäßig nicht als risikoarm. Neue Multi-Tenant-Apps ohne verifizierten Herausgeber dürfen seit 11/2020 nicht mehr von Nutzern freigegeben werden.
   → `AADSTS65001`, `AADSTS90094`, `AADSTS90095` → STATUS 3, „Deine Firmen-IT muss die App freigeben.“
2. **Conditional Access:** Device Code Flow kann per Richtlinie („Authentifizierungsflüsse“) gesperrt sein – Microsoft empfiehlt das wegen Phishing-Missbrauch. „Konformes Gerät“ ist im Device Code Flow in der Regel nicht erfüllbar; „Genehmigte Client-App“ und Standort-Regeln blockieren ebenfalls.
   → `AADSTS53000`, `AADSTS53001`, `AADSTS53002`, `AADSTS53003`, `AADSTS50097` → STATUS 4, „Firmenrichtlinie blockiert diese Anmeldung.“ Dafür gibt es keinen Workaround in der App.
3. **Token-Laufzeit:** „Anmeldehäufigkeit“-Richtlinien erzwingen regelmäßige Neuanmeldung (`interaction_required`/`invalid_grant`) → STATUS 2. Für das Briefing praktisch unbrauchbar.

| Kontotyp | Erwartung |
|---|---|
| Privates Microsoft-Konto | funktioniert |
| Kleinfirma ohne eigene Richtlinien | meist ok, abhängig von der Consent-Einstellung |
| Mittelstand mit IT-Dienstleister | unsicher |
| Konzern mit Intune / Conditional Access | scheitert fast sicher |

Positionierung bei Veröffentlichung: primär Privatkonten und kleine Firmen, nicht „Office 365 im Konzern“. Vor dem Store: Login mit einem fremden Test-Tenant (inkl. aktivierter Device-Code-Sperre) prüfen.

## Nicht-Ziele
- Kein eigener Server/Proxy, keine Timeline-Pins.
- Keine Schreibzugriffe auf den Kalender.
- Keine anderen Kalenderquellen (Google, CalDAV) in Version 1. Graph-spezifischer Code bleibt in `index.js`/`lib.js` gekapselt, damit später weitere Quellen andocken können.

## Reihenfolge der Umsetzung
1. ✅ Grundgerüst Watchapp + MenuLayer
2. ✅ Device Code Flow + Token-Handling in PebbleKit JS
3. ✅ Graph-Abfrage + AppMessage-Übertragung
4. ✅ Cache auf der Uhr
5. ✅ Clay-Einstellungen inkl. dynamischer Kalenderliste
6. ✅ Mehrere Kalender; Tagesansicht mit Blättern per Touch (ersetzt die Modi „Nur heute“/„Nächste N“)
7. ✅ Tägliches Briefing per Wakeup
8. ✅ Fehlerzustände (erste Fassung) – Feinschliff nach Test auf echter Hardware

Code ist geschrieben, aber noch **nicht mit dem echten SDK gebaut und nicht auf der Uhr getestet** (Build-Umgebung hatte keinen Zugriff auf den SDK-Server). C wurde gegen einen Stub-Header syntaktisch geprüft, JS-Logik per Node-Tests.

## Vor der Store-Veröffentlichung
- Publisher-Verifizierung in Entra ID (Microsoft-Partner-ID) – Voraussetzung, dass Nutzer in fremden Tenants überhaupt selbst zustimmen können.
- Store-Beschreibung: klarer Hinweis auf die Einschränkungen bei Firmenkonten (s. o.).
- Datenschutzhinweis: keine Datenverarbeitung durch den Entwickler, alles bleibt am Gerät.

## Offene Punkte
- App-Name (Arbeitstitel: „O365 Kalender“)
