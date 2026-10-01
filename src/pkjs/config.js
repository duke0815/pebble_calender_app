// Clay-Konfiguration – wird bei jedem Öffnen der Einstellungen neu aufgebaut,
// damit die Kalenderliste aktuell ist (siehe CLAUDE.md).

var lib = require('./lib');

var WEEKDAYS = ['Mo', 'Di', 'Mi', 'Do', 'Fr', 'Sa', 'So'];

// calendars: [{id, name, isDefaultCalendar}], settings: siehe lib.DEFAULT_SETTINGS
function buildConfig(calendars, settings, loggedIn) {
  var calendarItem;
  if (calendars.length) {
    var selected = settings.calendars || [];
    calendarItem = {
      type: 'checkboxgroup',
      messageKey: 'CFG_CALENDARS',
      label: 'Angezeigte Kalender',
      options: calendars.map(function(c) { return c.name; }),
      defaultValue: calendars.map(function(c) {
        return selected.length ? selected.indexOf(c.id) >= 0 : !!c.isDefaultCalendar;
      })
    };
  } else {
    calendarItem = {
      type: 'text',
      defaultValue: 'Noch keine Kalenderliste vorhanden. Bitte zuerst auf der Uhr anmelden – ' +
                    'bis dahin wird der Standardkalender verwendet.'
    };
  }

  return [
    { type: 'heading', defaultValue: 'O365 Kalender' },
    {
      type: 'section',
      items: [
        { type: 'heading', defaultValue: 'Kalender' },
        calendarItem
      ]
    },
    {
      type: 'section',
      items: [
        { type: 'heading', defaultValue: 'Anzeige' },
        {
          type: 'select',
          messageKey: 'CFG_MODE',
          label: 'Modus',
          defaultValue: settings.mode,
          options: [
            { label: 'Nur heute', value: 'today' },
            { label: 'Nächste N Termine', value: 'next' }
          ]
        },
        {
          type: 'slider',
          messageKey: 'CFG_COUNT',
          label: 'Anzahl Termine (N)',
          defaultValue: settings.count,
          min: 1,
          max: 10,
          step: 1
        },
        {
          type: 'toggle',
          messageKey: 'CFG_ALLDAY',
          label: 'Ganztägige Termine anzeigen',
          defaultValue: settings.showAllDay
        },
        {
          type: 'toggle',
          messageKey: 'CFG_HIDE_DECLINED',
          label: 'Abgelehnte Termine ausblenden',
          defaultValue: settings.hideDeclined
        }
      ]
    },
    {
      type: 'section',
      items: [
        { type: 'heading', defaultValue: 'Tägliches Briefing' },
        {
          type: 'toggle',
          messageKey: 'CFG_BRIEF',
          label: 'Briefing aktivieren',
          description: 'Die Uhr öffnet die App zur gewählten Uhrzeit und vibriert kurz.',
          defaultValue: settings.briefEnabled
        },
        {
          type: 'input',
          messageKey: 'CFG_BRIEF_TIME',
          label: 'Uhrzeit',
          defaultValue: settings.briefTime,
          attributes: { type: 'time' }
        },
        {
          type: 'checkboxgroup',
          messageKey: 'CFG_BRIEF_DAYS',
          label: 'Wochentage',
          options: WEEKDAYS,
          defaultValue: lib.maskToDays(settings.briefDays)
        }
      ]
    },
    {
      type: 'section',
      items: [
        { type: 'heading', defaultValue: 'Konto' },
        {
          type: 'text',
          defaultValue: loggedIn ? 'Angemeldet.' : 'Nicht angemeldet. Die Anmeldung startet auf der Uhr.'
        },
        {
          type: 'toggle',
          messageKey: 'CFG_LOGOUT',
          label: 'Abmelden (Tokens löschen)',
          defaultValue: false
        }
      ]
    },
    { type: 'submit', defaultValue: 'Speichern' }
  ];
}

// Wert eines Clay-Ergebnisses ({key: {value: x}} oder {key: x})
function value(result, key, fallback) {
  if (!result || !result.hasOwnProperty(key)) return fallback;
  var v = result[key];
  if (v && typeof v === 'object' && v.hasOwnProperty('value')) v = v.value;
  return v === undefined || v === null ? fallback : v;
}

// Überträgt das Clay-Ergebnis in unsere Settings. Gibt {settings, logout} zurück.
function applyResult(result, calendars, settings) {
  var next = lib.mergeSettings(settings);

  var selection = value(result, 'CFG_CALENDARS', null);
  if (selection && calendars.length) {
    next.calendars = calendars.filter(function(c, i) { return !!selection[i]; })
                              .map(function(c) { return c.id; });
  }
  var mode = value(result, 'CFG_MODE', next.mode);
  next.mode = mode === 'next' ? 'next' : 'today';
  var count = parseInt(value(result, 'CFG_COUNT', next.count), 10);
  next.count = isNaN(count) ? 10 : Math.max(1, Math.min(10, count));
  next.showAllDay = !!value(result, 'CFG_ALLDAY', next.showAllDay);
  next.hideDeclined = !!value(result, 'CFG_HIDE_DECLINED', next.hideDeclined);
  next.briefEnabled = !!value(result, 'CFG_BRIEF', next.briefEnabled);
  var t = lib.parseTime(value(result, 'CFG_BRIEF_TIME', next.briefTime));
  next.briefTime = (t.hour < 10 ? '0' : '') + t.hour + ':' + (t.minute < 10 ? '0' : '') + t.minute;
  var days = value(result, 'CFG_BRIEF_DAYS', null);
  if (days && days.length === 7) next.briefDays = lib.daysToMask(days);

  return { settings: next, logout: !!value(result, 'CFG_LOGOUT', false) };
}

module.exports = {
  buildConfig: buildConfig,
  applyResult: applyResult
};
