# CLAUDE.md – Pebble-Kalender-App (Office 365)

## Projektziel
Pebble-Watchapp, die Office-365-Kalendertermine anzeigt (heute bzw. die nächsten N Termine) und optional jeden Tag zu einer festen Uhrzeit ein Termin-Briefing auf der Uhr zeigt. Veröffentlichung im Pebble-App-Store geplant – jeder Nutzer meldet sich mit seinem eigenen Microsoft-Konto an.

**Grundprinzip: kein eigener Server.** Daten fließen ausschließlich Microsoft Graph → Handy (PebbleKit JS) → Uhr. Tokens bleiben am Handy des Nutzers.

## Plattform & Tooling
- Zielgerät: Pebble Time 2 (`emery`, 200×228, Farbe). Weitere Plattformen optional, Layout nicht hart auf Pixel festnageln.
- Pebble SDK: C (Uhr) + PebbleKit JS (Handy) + Clay (Einstellungsseite).
- Entwicklung unter Windows → Pebble SDK in WSL2 betreiben.
- Keine schweren JS-Abhängigkeiten außer Clay.

## Architektur
```
src/c/            Watchapp (C): UI, Cache, Wakeup, AppMessage-Empfang
src/pkjs/index.js PebbleKit JS: Auth, Graph-Abfragen, Filter, Senden an Uhr
src/pkjs/config.js Clay-Konfiguration (wird dynamisch aufgebaut, s. u.)
```

## Authentifizierung (Microsoft Entra ID)
- App-Registrierung: **Multi-Tenant + persönliche Microsoft-Konten**, Public Client, kein Client Secret.
- Flow: **Device Code Flow** über den `common`-Endpunkt
  - `POST https://login.microsoftonline.com/common/oauth2/v2.0/devicecode`
  - `POST https://login.microsoftonline.com/common/oauth2/v2.0/token` (Polling im angegebenen Intervall)
- Scopes: `Calendars.Read offline_access`
- Anzeige des Codes **auf der Uhr**: „microsoft.com/devicelogin – Code: XXXXXX“. Login startet automatisch, wenn kein Token vorhanden ist, oder über einen Menüpunkt auf der Uhr.
- Access Token + Refresh Token + Ablaufzeit in `localStorage` (PebbleKit JS). Vor Ablauf erneuern.
- `invalid_grant` → Tokens löschen, Uhr zeigt „Bitte neu anmelden“.
- Admin-Consent-Fehler (z. B. AADSTS65001 / AADSTS90094) → eigene, verständliche Meldung: „Deine Firmen-IT muss die App freigeben.“
- Client-ID als Konstante: `CLIENT_ID = "TODO"`.

## Microsoft Graph
- Kalenderliste: `GET /v1.0/me/calendars?$select=id,name,isDefaultCalendar` → in `localStorage` cachen.
- Termine: `GET /v1.0/me/calendars/{id}/calendarView?startDateTime=…&endDateTime=…&$select=subject,start,end,isAllDay,location,isCancelled,showAs,responseStatus&$orderby=start/dateTime&$top=50`
  - calendarView liefert Serientermine bereits aufgelöst – **keinen eigenen RRULE-Parser bauen**.
  - Mehrere ausgewählte Kalender: je Kalender abfragen, zusammenführen, nach Start sortieren, auf N kürzen.
- Zeiten: Standardmäßig UTC → in JS in Epoch-Sekunden umrechnen und so an die Uhr schicken.
  - **Ganztägige Termine** gesondert behandeln: nur das Datum verwenden, nicht verschieben.
- Abgesagte (`isCancelled`) und optional abgelehnte Termine (`responseStatus.response == "declined"`) ausfiltern.
- Pagination (`@odata.nextLink`) berücksichtigen.

## Einstellungen (Clay)
Clay-Config wird beim Öffnen (`showConfiguration`) **dynamisch** aus der gecachten Kalenderliste gebaut (`autoHandleEvents: false`).

| Einstellung | Typ |
|---|---|
| Kalender | Checkbox-Gruppe (Default: Standardkalender) |
| Modus | „Nur heute“ / „Nächste N Termine“ |
| N | 1–10 (Default 10) |
| Ganztägige Termine anzeigen | Toggle |
| Abgelehnte Termine ausblenden | Toggle |
| Tägliches Briefing | Toggle |
| Briefing-Uhrzeit | Zeit (Default 07:00) |
| Briefing-Wochentage | Checkbox-Gruppe (Default Mo–Fr) |
| Abmelden | Toggle/Button → Tokens löschen |

Settings, die die Uhr betreffen (Briefing an/aus, Uhrzeit, Wochentage), per AppMessage an die Uhr schicken und dort persistieren.

## AppMessage-Protokoll
Termine **einzeln** senden (Message-Größenlimit), mit Warteschlange und ACK/NACK-Retry.

| Key | Typ | Inhalt |
|---|---|---|
| `MSG_TYPE` | int | 1 = Termin, 2 = Ende der Liste, 3 = Status/Fehler, 4 = Login-Code, 5 = Settings |
| `EVT_INDEX` / `EVT_COUNT` | int | Position / Gesamtzahl |
| `EVT_START` / `EVT_END` | int32 | Epoch-Sekunden |
| `EVT_ALLDAY` | int | 0/1 |
| `EVT_TITLE` | cstring | max. 48 Zeichen, in JS gekürzt |
| `EVT_LOCATION` | cstring | max. 32 Zeichen |
| `STATUS_TEXT` | cstring | Fehler-/Statustext |
| `LOGIN_URL` / `LOGIN_CODE` | cstring | Device-Code-Anzeige |
| `SET_*` | int | Briefing-Settings |

Die Uhr fordert beim Start Daten an (`MSG_TYPE = REQUEST`).

## Watchapp (C)
- `MenuLayer`: Zeile = Uhrzeit (bzw. „Ganztägig“) + Titel, Untertitel = Ort.
- Bei „Nächste N“ über mehrere Tage: Tagesüberschriften als Sections.
- Kopfzeile/Status: „Stand HH:MM“, offline-Hinweis bei Cache-Anzeige.
- **Cache:** jeder Termin unter eigenem Persist-Key (256-Byte-Limit pro Key) + Zeitstempel des letzten Syncs. Beim Start sofort Cache anzeigen, dann aktualisieren.
- Leerer Zustand: „Keine Termine heute“.

## Tägliches Briefing (Wakeup-API)
- `wakeup_schedule()` immer nur für den **nächsten** Briefing-Zeitpunkt (berücksichtigt Wochentage).
- Neu planen: nach jeder Settings-Änderung und bei jedem Wakeup-Start.
- Kollision (`E_RANGE`): bis zu 5× je +1 Minute versuchen.
- Start mit `APP_LAUNCH_WAKEUP`: kurz vibrieren, frische Daten anfordern (Timeout ~10 s), sonst Cache mit „Stand: …“ anzeigen. App nach ~60 s ohne Interaktion selbst schließen.
- Wenn die App ohnehin offen ist: am Abend schon die Termine des nächsten Tages mitcachen.

## Nicht-Ziele
- Kein eigener Server/Proxy, keine Timeline-Pins.
- Keine Schreibzugriffe auf den Kalender.
- Keine anderen Kalenderquellen (Google, CalDAV) in Version 1.

## Reihenfolge der Umsetzung
1. Grundgerüst Watchapp + MenuLayer mit Dummy-Daten
2. Device Code Flow + Token-Handling in PebbleKit JS
3. Graph-Abfrage (Standardkalender, heute) + AppMessage-Übertragung
4. Cache auf der Uhr
5. Clay-Einstellungen inkl. dynamischer Kalenderliste
6. Modus „Nächste N“, mehrere Kalender
7. Tägliches Briefing per Wakeup
8. Fehlerzustände & Feinschliff

## Vor der Store-Veröffentlichung
- Publisher-Verifizierung in Entra ID (Microsoft-Partner-ID) – sonst scheitert die Anmeldung in vielen Firmen-Tenants.
- Store-Beschreibung: Hinweis, dass manche Firmen-ITs die App erst freigeben müssen.
- Datenschutzhinweis: keine Datenverarbeitung durch den Entwickler, alles bleibt am Gerät.

## Offene Punkte
- App-Name
- Client-ID der Entra-App-Registrierung
