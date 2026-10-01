# O365 Kalender für Pebble

Pebble-Watchapp (Ziel: Pebble Time 2), die Termine aus deinem Microsoft-365-/Outlook-Kalender tageweise anzeigt und optional jeden Morgen ein Termin-Briefing auf der Uhr zeigt. Kein eigener Server: Microsoft Graph → Handy (PebbleKit JS) → Uhr.

Details zu Architektur, Protokoll und Entscheidungen: [CLAUDE.md](CLAUDE.md).

## 1. App in Microsoft Entra ID registrieren (einmalig, ca. 5 Minuten)

1. <https://entra.microsoft.com> → **Identität → Anwendungen → App-Registrierungen → Neue Registrierung**
2. Name: z. B. `Pebble Kalender`
3. Unterstützte Kontotypen:
   - Nur für dich mit deinem Firmen-/Schulkonto: **„Nur Konten in diesem Organisationsverzeichnis“**
   - Für alle (auch outlook.com): **„Konten in einem beliebigen Organisationsverzeichnis und persönliche Microsoft-Konten“**
4. Umleitungs-URI: leer lassen → **Registrieren**
5. **Authentifizierung → Erweiterte Einstellungen → „Öffentliche Clientflows zulassen“ = Ja** → Speichern
6. **API-Berechtigungen → Berechtigung hinzufügen → Microsoft Graph → Delegierte Berechtigungen → `Calendars.Read`** (`offline_access` ist automatisch dabei). Bist du Admin im eigenen Tenant, gleich **„Administratorzustimmung erteilen“** klicken.
7. **Übersicht → „Anwendungs-ID (Client)“** kopieren und in [`src/pkjs/index.js`](src/pkjs/index.js) eintragen:
   ```js
   var CLIENT_ID = 'deine-client-id';
   var AUTHORITY = 'common';   // bei "Nur dieses Organisationsverzeichnis": deine Tenant-ID
   ```

## 2. Pebble SDK in WSL2 einrichten

```bash
# in WSL2 (Ubuntu)
curl -LsSf https://astral.sh/uv/install.sh | sh
uv tool install pebble-tool
pebble sdk install latest
```

## 3. Bauen und installieren

```bash
pebble build
# Emulator (Pebble Time 2):
pebble install --emulator emery
pebble emu-app-config --emulator emery   # Einstellungsseite im Emulator
# Echte Uhr: in der Pebble-App am Handy die Entwicklerverbindung aktivieren, dann
pebble install --phone <IP-des-Handys>
```

Alternativ die `build/*.pbw` aufs Handy kopieren und mit der Pebble-App öffnen.

## 4. Erste Anmeldung

1. App auf der Uhr starten. Ohne Anmeldung zeigt die Uhr automatisch eine Adresse und einen Code.
2. Am PC oder Handy `microsoft.com/devicelogin` öffnen, den Code eingeben und mit deinem Microsoft-Konto anmelden.
3. **App auf der Uhr währenddessen offen lassen.** Danach erscheinen die Termine.

Einstellungen (Kalender, Sprache Deutsch/Englisch, Farben, Briefing) findest du in der Pebble-App am Handy unter den Einstellungen der Watchapp.

## Bedienung

- **Tagesansicht:** bis zu 12 Termine eines Tages (vergangene optional grau, Einstellung „Vergangene Termine ausgrauen“). Oben steht immer das Datum des angezeigten Tages; darunter nur bei Bedarf „Aktualisiere...“, „Offline – Stand HH:MM“ oder ein Fehler.
- **Tasten:** hoch/runter = Termin auswählen (der erste Druck zeigt nur die Auswahl), **lang drücken = vorheriger/nächster Tag**, Select = Details.
- **Wischen (Pebble Time 2, abschaltbar in den Einstellungen):** links = nächster Tag, rechts = vorheriger Tag, hoch/runter = flüssig scrollen. Heute und morgen sind sofort da (auch offline), andere Tage werden vom Handy geladen. Für das flüssige Scrollen und Tippen muss auf der Uhr die **Touch-Navigation** eingeschaltet sein (Einstellungen der Uhr).
- **Termin öffnen:** antippen oder Select. Die Detailansicht zeigt auch die Beschreibung aus Outlook – sie wird beim Öffnen vom Handy geladen; ist das Handy nicht in Reichweite, steht dort ein Hinweis. Wischen hoch/runter scrollt, nach rechts wischen geht zurück.
- **Aktualisieren:** automatisch einmal beim Öffnen der App.
- **Konto wechseln:** in den Einstellungen „Abmelden“, dann die App neu öffnen.
- **Briefing:** Zur eingestellten Zeit startet die App, vibriert, lädt frische Daten (sonst Cache) und schließt sich nach 60 s ohne Bedienung wieder.

## Fehlermeldungen

| Meldung | Bedeutung |
|---|---|
| Client-ID fehlt | `CLIENT_ID` in `src/pkjs/index.js` eintragen |
| App-Registrierung fehlerhaft | „Öffentliche Clientflows zulassen“ nicht aktiviert oder falsche Client-ID |
| Bitte App neu öffnen zum Anmelden | Token abgelaufen oder widerrufen → App schließen und neu öffnen, dann startet der Login |
| Deine Firmen-IT muss die App freigeben | Admin-Zustimmung nötig (AADSTS65001/90094) |
| Firmenrichtlinie blockiert diese Anmeldung | Conditional Access sperrt den Device Code Flow, kein Workaround |
| Offline – Stand HH:MM | Handy oder Internet nicht erreichbar, es wird der Cache angezeigt |

## Tests

```bash
TZ=Europe/Vienna node test/lib.test.js    # JS-Logik
TZ=Europe/Vienna node test/index.sim.js   # Ablauf Login → Graph → Uhr (gemockt)
sh test/c/run.sh                          # C-Syntax (alle Plattformen) + Uhr-Logik nativ
```

Die C-Tests nutzen einen Stub von `pebble.h`. Sie ersetzen keinen echten `pebble build`.
