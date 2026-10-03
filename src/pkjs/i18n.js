// Translations (German/English) for status messages and the settings page.
// The current language is set from the settings (setLang), t(key) returns the text.

var STRINGS = {
  de: {
    syncing: 'Aktualisiere...',
    noClientId: 'Client-ID fehlt (index.js).',
    noConnection: 'Keine Verbindung.',
    codeExpired: 'Code abgelaufen. App neu öffnen.',
    reopenLogin: 'Bitte App neu öffnen zum Anmelden.',
    loggedOut: 'Abgemeldet. App neu öffnen zum Anmelden.',
    loggedOutShort: 'Abgemeldet.',
    adminConsent: 'Deine Firmen-IT muss die App freigeben.',
    policyBlocked: 'Firmenrichtlinie blockiert diese Anmeldung.',
    badClient: 'App-Registrierung fehlerhaft (Client-ID).',
    authError: 'Anmeldefehler',
    noCalendarAccess: 'Kein Zugriff auf Kalender.',
    graphError: 'Graph-Fehler',
    // Settings page
    cfgCalendars: 'Kalender',
    cfgShownCalendars: 'Angezeigte Kalender',
    cfgNoCalendars: 'Noch keine Kalenderliste vorhanden. Bitte zuerst auf der Uhr anmelden – ' +
                    'bis dahin wird der Standardkalender verwendet.',
    cfgDisplay: 'Anzeige',
    cfgShowAllDay: 'Ganztägige Termine anzeigen',
    cfgHideDeclined: 'Abgelehnte Termine ausblenden',
    cfgGrayPast: 'Vergangene Termine ausgrauen',
    moveDenied: 'Dieser Termin darf nicht verschoben werden.',
    moveFailed: 'Verschieben fehlgeschlagen',
    cfgMove: 'Termine verschieben',
    cfgMoveEnable: 'Verschieben erlauben',
    cfgMoveEnableDesc: 'In der Detailansicht mit Select. Beim Einschalten zeigt die Uhr einmalig einen neuen ' +
                       'Anmelde-Code, weil die App dafür Schreibrechte braucht.',
    cfgMoveScope: 'Erlaubt für',
    cfgMoveSolo: 'Eigene Termine ohne Teilnehmer',
    cfgMoveOwn: 'Alle eigenen Termine (Teilnehmer erhalten ein Update)',
    cfgMoveOptions: 'Möglichkeiten',
    moveOptionLabels: ['1 Stunde später', '1 Tag später', '2 Tage später', '1 Woche später', 'Nächster Montag'],
    respondDenied: 'Auf diesen Termin kann nicht geantwortet werden.',
    respondFailed: 'Antworten fehlgeschlagen',
    cfgRespond: 'Einladungen',
    cfgRespondEnable: 'Auf Einladungen antworten',
    cfgRespondEnableDesc: 'In der Detailansicht mit Select: Zusagen, Mit Vorbehalt oder Absagen. Der Organisator ' +
                          'erhält deine Antwort. Braucht Schreibrechte wie das Verschieben (einmalig neuer ' +
                          'Anmelde-Code, falls noch nicht erteilt).',
    cfgLanguage: 'Sprache',
    cfgAutoClose: 'Automatisch schließen',
    cfgAutoCloseDesc: 'Beendet die App nach dieser Zeit ohne Bedienung und spart Akku (vor allem mit Touch). Nicht während der Anmeldung.',
    cfgAutoCloseNever: 'Nie',
    cfgAutoClose2: 'Nach 2 Minuten',
    cfgAutoClose5: 'Nach 5 Minuten',
    cfgAutoClose10: 'Nach 10 Minuten',
    cfgTouch: 'Touch-Bedienung',
    cfgTouchDesc: 'Wischen links/rechts wechselt den Tag, hoch/runter scrollt, Tippen öffnet einen Termin. ' +
                  'Ohne Touch: hoch/runter lang drücken wechselt den Tag.',
    cfgLangAuto: 'Automatisch (wie Handy)',
    cfgColors: 'Farben',
    cfgColorAccent: 'Akzentfarbe',
    cfgColorAccentDesc: 'Datumsleiste und ausgewählter Termin',
    cfgColorText: 'Schriftfarbe',
    cfgColorBg: 'Hintergrundfarbe',
    cfgBriefing: 'Tägliches Briefing',
    cfgBriefEnable: 'Briefing aktivieren',
    cfgBriefDesc: 'Die Uhr öffnet die App zur gewählten Uhrzeit und vibriert kurz.',
    cfgTime: 'Uhrzeit',
    cfgWeekdays: 'Wochentage',
    weekdaysMonFirst: ['Mo', 'Di', 'Mi', 'Do', 'Fr', 'Sa', 'So'],
    cfgAccount: 'Konto',
    cfgLoggedIn: 'Angemeldet.',
    cfgLoggedOut: 'Nicht angemeldet. Die Anmeldung startet auf der Uhr.',
    cfgLogout: 'Abmelden (Tokens löschen)',
    cfgSave: 'Speichern'
  },
  en: {
    syncing: 'Updating...',
    noClientId: 'Client ID missing (index.js).',
    noConnection: 'No connection.',
    codeExpired: 'Code expired. Reopen the app.',
    reopenLogin: 'Please reopen the app to sign in.',
    loggedOut: 'Signed out. Reopen the app to sign in.',
    loggedOutShort: 'Signed out.',
    adminConsent: 'Your IT admin must approve this app.',
    policyBlocked: 'Company policy blocks this sign-in.',
    badClient: 'App registration invalid (client ID).',
    authError: 'Sign-in error',
    noCalendarAccess: 'No access to calendar.',
    graphError: 'Graph error',
    cfgCalendars: 'Calendars',
    cfgShownCalendars: 'Calendars to show',
    cfgNoCalendars: 'No calendar list yet. Please sign in on the watch first – ' +
                    'until then the default calendar is used.',
    cfgDisplay: 'Display',
    cfgShowAllDay: 'Show all-day events',
    cfgHideDeclined: 'Hide declined events',
    cfgGrayPast: 'Gray out past events',
    moveDenied: 'This event cannot be moved.',
    moveFailed: 'Move failed',
    cfgMove: 'Move events',
    cfgMoveEnable: 'Allow moving',
    cfgMoveEnableDesc: 'Press Select in the detail view. When enabled, the watch shows a new sign-in code once, ' +
                       'because the app needs write access for this.',
    cfgMoveScope: 'Allowed for',
    cfgMoveSolo: 'Own events without attendees',
    cfgMoveOwn: 'All own events (attendees get an update)',
    cfgMoveOptions: 'Options',
    moveOptionLabels: ['1 hour later', '1 day later', '2 days later', '1 week later', 'Next Monday'],
    respondDenied: 'You cannot respond to this event.',
    respondFailed: 'Response failed',
    cfgRespond: 'Invitations',
    cfgRespondEnable: 'Respond to invitations',
    cfgRespondEnableDesc: 'Press Select in the detail view: Accept, Tentative or Decline. The organizer gets ' +
                          'your response. Needs write access like moving (a new sign-in code once, if not ' +
                          'granted yet).',
    cfgLanguage: 'Language',
    cfgAutoClose: 'Close automatically',
    cfgAutoCloseDesc: 'Exits the app after this time without use to save battery (especially with touch). Not during sign-in.',
    cfgAutoCloseNever: 'Never',
    cfgAutoClose2: 'After 2 minutes',
    cfgAutoClose5: 'After 5 minutes',
    cfgAutoClose10: 'After 10 minutes',
    cfgTouch: 'Touch control',
    cfgTouchDesc: 'Swipe left/right to change the day, up/down to scroll, tap to open an event. ' +
                  'Without touch: long-press up/down to change the day.',
    cfgLangAuto: 'Automatic (like phone)',
    cfgColors: 'Colors',
    cfgColorAccent: 'Accent color',
    cfgColorAccentDesc: 'Date bar and selected event',
    cfgColorText: 'Text color',
    cfgColorBg: 'Background color',
    cfgBriefing: 'Daily briefing',
    cfgBriefEnable: 'Enable briefing',
    cfgBriefDesc: 'The watch opens the app at the chosen time and vibrates briefly.',
    cfgTime: 'Time',
    cfgWeekdays: 'Weekdays',
    weekdaysMonFirst: ['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun'],
    cfgAccount: 'Account',
    cfgLoggedIn: 'Signed in.',
    cfgLoggedOut: 'Not signed in. Sign-in starts on the watch.',
    cfgLogout: 'Sign out (delete tokens)',
    cfgSave: 'Save'
  }
};

var current = 'de';

// setting: 'auto' | 'de' | 'en'; phoneLang: e.g. navigator.language
function resolveLang(setting, phoneLang) {
  if (setting === 'de' || setting === 'en') return setting;
  return /^de/i.test(phoneLang || '') ? 'de' : 'en';
}

function setLang(lang) {
  current = lang === 'en' ? 'en' : 'de';
}

function getLang() {
  return current;
}

function t(key) {
  var table = STRINGS[current];
  return table.hasOwnProperty(key) ? table[key] : STRINGS.de[key];
}

module.exports = {
  resolveLang: resolveLang,
  setLang: setLang,
  getLang: getLang,
  t: t
};
