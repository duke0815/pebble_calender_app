// Übersetzungen (Deutsch/Englisch) für Statusmeldungen und die Einstellungsseite.
// Die aktuelle Sprache wird aus den Settings gesetzt (setLang), t(key) liefert den Text.

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
    // Einstellungsseite
    cfgCalendars: 'Kalender',
    cfgShownCalendars: 'Angezeigte Kalender',
    cfgNoCalendars: 'Noch keine Kalenderliste vorhanden. Bitte zuerst auf der Uhr anmelden – ' +
                    'bis dahin wird der Standardkalender verwendet.',
    cfgDisplay: 'Anzeige',
    cfgShowAllDay: 'Ganztägige Termine anzeigen',
    cfgHideDeclined: 'Abgelehnte Termine ausblenden',
    cfgGrayPast: 'Vergangene Termine ausgrauen',
    cfgLanguage: 'Sprache',
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
    cfgLanguage: 'Language',
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

// setting: 'auto' | 'de' | 'en'; phoneLang: z. B. navigator.language
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
