# CLAUDE.md – Pebble-Kalender-App (Office 365)

## Projektziel
Pebble-Watchapp, die Office-365-Kalendertermine anzeigt (heute bzw. die nächsten N Termine) und optional jeden Tag zu einer festen Uhrzeit ein Termin-Briefing auf der Uhr zeigt.

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
src/c/main.c         Einstieg, Launch-Reason, Wakeup-Start, Inaktivitäts-Timer
src/c/events.c/.h    Terminliste, Persist-Cache, Ansicht (Filter/Sections)
src/c/comm.c/.h      AppMessage-Empfang/-Senden
src/c/settings.c/.h  Uhr-Settings (Briefing, Modus, N) + Wakeup-Planung
src/c/ui_main.c/.h   Hauptfenster: Statuszeile + MenuLayer
src/c/ui_detail.c/.h Detailansicht eines Termins
src/c/ui_login.c/.h  Anzeige des Device-Codes
src/c/protocol.h     Gemeinsame Konstanten (MSG_TYPE, STATUS_CODE, Längen)
src/pkjs/index.js    PebbleKit JS: Ablaufsteuerung, Auth, Graph, Senden an Uhr
src/pkjs/lib.js      Reine Logik (Zeiten, Filter, Kürzen, Fehlerklassifizierung) – mit Node testbar
src/pkjs/config.js   Clay-Konfiguration (wird dynamisch aufgebaut)
src/c/format.c/.h    Zeit-/Datumsformatierung (deutsch)
test/lib.test.js     Node-Tests für lib.js + config.js
test/index.sim.js    Simulation von index.js (Pebble/XHR/localStorage gemockt)
test/c/run.sh        C-Syntaxprüfung gegen Stub-pebble.h (emery/basalt/diorite) + native Logiktests
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
- Anzeige des Codes **auf der Uhr**: „microsoft.com/devicelogin – Code: XXXXXX“. Login startet automatisch, wenn kein Token vorhanden ist, oder über den Menüpunkt „Anmelden“ auf der Uhr (erzwingt neue Anmeldung, z. B. Kontowechsel). Während des Logins muss die App offen bleiben (PebbleKit JS läuft nur, solange die Watchapp läuft).
- Access Token + Refresh Token + Ablaufzeit in `localStorage` (PebbleKit JS). Erneuern, wenn weniger als 5 Minuten Restlaufzeit.
- `invalid_grant` / `interaction_required` → Tokens löschen, Uhr zeigt „Bitte neu anmelden“.
- Client-ID als Konstante in `src/pkjs/index.js`: `402716d7-8b06-443b-af50-c37348a787c4` (Public Client, kein Geheimnis). Steht dort `TODO`, zeigt die Uhr „Client-ID fehlt“.

## Microsoft Graph
- Kalenderliste: `GET /v1.0/me/calendars?$select=id,name,isDefaultCalendar` → in `localStorage` cachen (max. 1×/Tag automatisch erneuern, sonst beim Öffnen der Einstellungen).
- Termine: `GET /v1.0/me/calendars/{id}/calendarView?startDateTime=…&endDateTime=…&$select=subject,start,end,isAllDay,location,isCancelled,showAs,responseStatus&$orderby=start/dateTime&$top=50`
  - Ohne Kalenderauswahl: `/v1.0/me/calendar/calendarView` (Standardkalender).
  - Header `Prefer: outlook.timezone="UTC"` immer mitsenden.
  - calendarView liefert Serientermine bereits aufgelöst – **keinen eigenen RRULE-Parser bauen**.
  - Mehrere ausgewählte Kalender: je Kalender abfragen, zusammenführen, nach Start sortieren, kürzen.
- Abfragezeitraum:
  - Modus „Nur heute“: heute 00:00 bis übermorgen 00:00 (lokal). Morgen wird immer mitgeholt und gecacht, damit das Briefing am nächsten Morgen auch offline Daten hat. Die Uhr zeigt nur die heutigen.
  - Modus „Nächste N“: heute 00:00 bis +14 Tage, nur Termine mit Ende > jetzt, auf N gekürzt.
- Zeiten: Graph liefert UTC ohne `Z` → in JS in Epoch-Sekunden umrechnen und so an die Uhr schicken.
  - **Ganztägige Termine:** nur das Datum verwenden, nicht verschieben. Konvention: `EVT_START` = **lokale Mitternacht** des Startdatums, `EVT_END` = lokale Mitternacht des (exklusiven) Enddatums, beides in JS berechnet.
- Abgesagte (`isCancelled`) und optional abgelehnte Termine (`responseStatus.response == "declined"`) ausfiltern; ganztägige optional ausblenden.
- Pagination (`@odata.nextLink`) berücksichtigen.
- Fehler: 401 → einmal Token erneuern und wiederholen; 403 → „Kein Zugriff auf Kalender“; sonst „Graph-Fehler <Status>“; Status 0/Timeout → „Keine Verbindung“.

## Einstellungen (Clay)
Clay-Config wird beim Öffnen (`showConfiguration`) **dynamisch** aus der gecachten Kalenderliste gebaut (`autoHandleEvents: false`, neue Clay-Instanz pro Öffnen, `clay-settings` vorher löschen – die Defaults kommen aus unseren eigenen Settings).

| Einstellung | Typ |
|---|---|
| Kalender | Checkbox-Gruppe (Default: Standardkalender) |
| Modus | „Nur heute“ / „Nächste N Termine“ |
| N | 1–10 (Default 10) |
| Ganztägige Termine anzeigen | Toggle (Default an) |
| Abgelehnte Termine ausblenden | Toggle (Default an) |
| Tägliches Briefing | Toggle (Default aus) |
| Briefing-Uhrzeit | Zeit (Default 07:00) |
| Briefing-Wochentage | Checkbox-Gruppe (Default Mo–Fr) |
| Abmelden | Toggle → Tokens löschen |

Settings, die die Uhr betreffen (Briefing an/aus, Uhrzeit, Wochentage, Modus, N), per AppMessage (`MSG_TYPE = 5`) an die Uhr schicken und dort persistieren. JS schickt sie zusätzlich zu Beginn jeder Synchronisierung.

## AppMessage-Protokoll
Termine **einzeln** senden (Message-Größenlimit), mit Warteschlange und ACK/NACK-Retry (bis 4 Versuche, wachsende Pause).

| Key | Typ | Inhalt |
|---|---|---|
| `MSG_TYPE` | int | Handy→Uhr: 1 = Termin, 2 = Ende der Liste, 3 = Status/Fehler, 4 = Login-Code, 5 = Settings. Uhr→Handy: 10 = REQUEST (Daten anfordern), 11 = LOGIN (Neuanmeldung erzwingen) |
| `EVT_INDEX` / `EVT_COUNT` | int | Position / Gesamtzahl (`EVT_COUNT` auch bei MSG_TYPE 2) |
| `EVT_START` / `EVT_END` | int32 | Epoch-Sekunden (ganztägig: lokale Mitternacht, s. o.) |
| `EVT_ALLDAY` | int | 0/1 |
| `EVT_TITLE` | cstring | max. 48 Zeichen **und** max. 63 Byte UTF-8, in JS gekürzt (`...`) |
| `EVT_LOCATION` | cstring | max. 32 Zeichen **und** max. 39 Byte UTF-8 |
| `STATUS_CODE` | int | 1 = Synchronisiere, 2 = Anmeldung nötig, 3 = Admin-Freigabe nötig, 4 = Richtlinie blockiert, 5 = Keine Verbindung, 6 = Sonstiger Fehler, 7 = Konfiguration fehlt |
| `STATUS_TEXT` | cstring | Fehler-/Statustext (deutsch, kurz) |
| `LOGIN_URL` / `LOGIN_CODE` | cstring | Device-Code-Anzeige |
| `SET_BRIEF_ENABLED` | int | 0/1 |
| `SET_BRIEF_HOUR` / `SET_BRIEF_MINUTE` | int | Briefing-Uhrzeit |
| `SET_BRIEF_DAYS` | int | Bitmaske, Bit 0 = Sonntag … Bit 6 = Samstag (wie `tm_wday`); Mo–Fr = 62 |
| `SET_MODE` | int | 0 = Nur heute, 1 = Nächste N |
| `SET_COUNT` | int | N (1–10) |

- Die Uhr fordert beim Start Daten an (`MSG_TYPE = 10`). JS startet beim `ready`-Event ebenfalls eine Synchronisierung; parallele Anfragen werden in JS zusammengefasst.
- Die Uhr übernimmt eine neue Liste nur, wenn bei `MSG_TYPE = 2` alle `EVT_COUNT` Termine angekommen sind – sonst bleibt der alte Cache.
- Jede Nachricht außer 4 schließt das Login-Fenster auf der Uhr.
- Texte für die Uhr kein „…“-Zeichen verwenden, sondern `...` (Font-Abdeckung).

## Watchapp (C)
- Statuszeile oben: „Stand HH:MM“, „Aktualisiere...“, „Offline · Stand HH:MM“ oder Fehlertext.
- `MenuLayer` darunter, eigene Zeilenzeichnung: Uhrzeit-Spanne (bzw. „Ganztägig“), Titel, Ort. Vergangene Termine grau.
- Tagesüberschriften als Sections („Heute · Mi 01.10.“, „Morgen · …“, „Fr 03.10.“). Termine, die vor heute begonnen haben, erscheinen unter „Heute“.
- Letzte Section „Aktionen“: „Aktualisieren“, „Anmelden“.
- Select auf einem Termin → Detailfenster (Datum, Zeit, voller Titel, Ort).
- Minütlicher Tick → Ansicht neu berechnen (vergangene Termine, Tageswechsel).
- **Cache:** jeder Termin unter eigenem Persist-Key (256-Byte-Limit pro Key) + Anzahl + Zeitstempel des letzten Syncs + Cache-Version. Beim Start sofort Cache anzeigen, dann aktualisieren.
- **Speicherbudget:** Persistenter Speicher ist insgesamt ca. 4 KB pro App. Ein Termin belegt ~116 Byte → **max. 16 Termine** im Cache (~1,9 KB) plus Settings. Nicht erhöhen, ohne nachzurechnen.
- Leerer Zustand: „Keine Termine heute“ bzw. „Keine anstehenden Termine“; bei Fehlern der Fehlertext.
- Timeout für eine Synchronisierung: 30 s (außer während des Logins), dann „Offline“.

## Tägliches Briefing (Wakeup-API)
- `wakeup_schedule()` immer nur für den **nächsten** Briefing-Zeitpunkt (berücksichtigt Wochentage); vorher `wakeup_cancel_all()`.
- Neu planen: bei jedem App-Start, nach jeder Settings-Änderung und bei jedem Wakeup.
- Kollision (`E_RANGE`): bis zu 5× je +1 Minute versuchen.
- Start mit `APP_LAUNCH_WAKEUP` (oder Wakeup bei offener App): kurz vibrieren, frische Daten anfordern (Timeout 10 s), sonst Cache mit „Offline · Stand …“ anzeigen. App nach 60 s ohne Interaktion selbst schließen (nur beim Wakeup-Start).
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
6. ✅ Modus „Nächste N“, mehrere Kalender
7. ✅ Tägliches Briefing per Wakeup
8. ✅ Fehlerzustände (erste Fassung) – Feinschliff nach Test auf echter Hardware

Code ist geschrieben, aber noch **nicht mit dem echten SDK gebaut und nicht auf der Uhr getestet** (Build-Umgebung hatte keinen Zugriff auf den SDK-Server). C wurde gegen einen Stub-Header syntaktisch geprüft, JS-Logik per Node-Tests.

## Vor der Store-Veröffentlichung
- Publisher-Verifizierung in Entra ID (Microsoft-Partner-ID) – Voraussetzung, dass Nutzer in fremden Tenants überhaupt selbst zustimmen können.
- Store-Beschreibung: klarer Hinweis auf die Einschränkungen bei Firmenkonten (s. o.).
- Datenschutzhinweis: keine Datenverarbeitung durch den Entwickler, alles bleibt am Gerät.

## Offene Punkte
- App-Name (Arbeitstitel: „O365 Kalender“)
