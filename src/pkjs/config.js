// Clay-Konfiguration – wird bei jedem Öffnen der Einstellungen neu aufgebaut,
// damit die Kalenderliste aktuell ist (siehe CLAUDE.md).

var lib = require('./lib');
var i18n = require('./i18n');
var t = i18n.t;

function hex(color) {
  var h = (Number(color) & 0xFFFFFF).toString(16);
  while (h.length < 6) h = '0' + h;
  return h.toUpperCase();
}

function colorItem(key, label, value, description) {
  var item = {
    type: 'color',
    messageKey: key,
    label: label,
    defaultValue: hex(value),
    sunlight: false
  };
  if (description) item.description = description;
  return item;
}

// calendars: [{id, name, isDefaultCalendar}], settings: siehe lib.DEFAULT_SETTINGS
function buildConfig(calendars, settings, loggedIn) {
  var calendarItem;
  if (calendars.length) {
    var selected = settings.calendars || [];
    calendarItem = {
      type: 'checkboxgroup',
      messageKey: 'CFG_CALENDARS',
      label: t('cfgShownCalendars'),
      options: calendars.map(function(c) { return c.name; }),
      defaultValue: calendars.map(function(c) {
        return selected.length ? selected.indexOf(c.id) >= 0 : !!c.isDefaultCalendar;
      })
    };
  } else {
    calendarItem = {
      type: 'text',
      defaultValue: t('cfgNoCalendars')
    };
  }

  return [
    { type: 'heading', defaultValue: 'O365 Kalender' },
    {
      type: 'section',
      items: [
        { type: 'heading', defaultValue: t('cfgCalendars') },
        calendarItem
      ]
    },
    {
      type: 'section',
      items: [
        { type: 'heading', defaultValue: t('cfgDisplay') },
        {
          type: 'select',
          messageKey: 'CFG_LANG',
          label: t('cfgLanguage'),
          defaultValue: settings.language,
          options: [
            { label: t('cfgLangAuto'), value: 'auto' },
            { label: 'Deutsch', value: 'de' },
            { label: 'English', value: 'en' }
          ]
        },
        {
          type: 'toggle',
          messageKey: 'CFG_ALLDAY',
          label: t('cfgShowAllDay'),
          defaultValue: settings.showAllDay
        },
        {
          type: 'toggle',
          messageKey: 'CFG_HIDE_DECLINED',
          label: t('cfgHideDeclined'),
          defaultValue: settings.hideDeclined
        }
      ]
    },
    {
      type: 'section',
      items: [
        { type: 'heading', defaultValue: t('cfgColors') },
        colorItem('CFG_COLOR_ACCENT', t('cfgColorAccent'), settings.accentColor, t('cfgColorAccentDesc')),
        colorItem('CFG_COLOR_TEXT', t('cfgColorText'), settings.textColor),
        colorItem('CFG_COLOR_BG', t('cfgColorBg'), settings.bgColor)
      ]
    },
    {
      type: 'section',
      items: [
        { type: 'heading', defaultValue: t('cfgBriefing') },
        {
          type: 'toggle',
          messageKey: 'CFG_BRIEF',
          label: t('cfgBriefEnable'),
          description: t('cfgBriefDesc'),
          defaultValue: settings.briefEnabled
        },
        {
          type: 'input',
          messageKey: 'CFG_BRIEF_TIME',
          label: t('cfgTime'),
          defaultValue: settings.briefTime,
          attributes: { type: 'time' }
        },
        {
          type: 'checkboxgroup',
          messageKey: 'CFG_BRIEF_DAYS',
          label: t('cfgWeekdays'),
          options: t('weekdaysMonFirst'),
          defaultValue: lib.maskToDays(settings.briefDays)
        }
      ]
    },
    {
      type: 'section',
      items: [
        { type: 'heading', defaultValue: t('cfgAccount') },
        {
          type: 'text',
          defaultValue: loggedIn ? t('cfgLoggedIn') : t('cfgLoggedOut')
        },
        {
          type: 'toggle',
          messageKey: 'CFG_LOGOUT',
          label: t('cfgLogout'),
          defaultValue: false
        }
      ]
    },
    { type: 'submit', defaultValue: t('cfgSave') }
  ];
}

// Wert eines Clay-Ergebnisses ({key: {value: x}} oder {key: x})
function value(result, key, fallback) {
  if (!result || !result.hasOwnProperty(key)) return fallback;
  var v = result[key];
  if (v && typeof v === 'object' && v.hasOwnProperty('value')) v = v.value;
  return v === undefined || v === null ? fallback : v;
}

// Clay liefert Farben als Zahl (0xRRGGBB), ältere Versionen ggf. als Hex-String
function colorValue(v, fallback) {
  if (v === null || v === undefined || v === '') return fallback;
  var n = typeof v === 'number' ? v : parseInt(String(v).replace(/^#|^0x/i, ''), 16);
  return isNaN(n) ? fallback : (n & 0xFFFFFF);
}

// Überträgt das Clay-Ergebnis in unsere Settings. Gibt {settings, logout} zurück.
function applyResult(result, calendars, settings) {
  var next = lib.mergeSettings(settings);

  var selection = value(result, 'CFG_CALENDARS', null);
  if (selection && calendars.length) {
    next.calendars = calendars.filter(function(c, i) { return !!selection[i]; })
                              .map(function(c) { return c.id; });
  }
  next.showAllDay = !!value(result, 'CFG_ALLDAY', next.showAllDay);
  next.hideDeclined = !!value(result, 'CFG_HIDE_DECLINED', next.hideDeclined);
  next.briefEnabled = !!value(result, 'CFG_BRIEF', next.briefEnabled);
  var t = lib.parseTime(value(result, 'CFG_BRIEF_TIME', next.briefTime));
  next.briefTime = (t.hour < 10 ? '0' : '') + t.hour + ':' + (t.minute < 10 ? '0' : '') + t.minute;
  var lang = value(result, 'CFG_LANG', next.language);
  next.language = (lang === 'de' || lang === 'en') ? lang : 'auto';
  next.accentColor = colorValue(value(result, 'CFG_COLOR_ACCENT', null), next.accentColor);
  next.textColor = colorValue(value(result, 'CFG_COLOR_TEXT', null), next.textColor);
  next.bgColor = colorValue(value(result, 'CFG_COLOR_BG', null), next.bgColor);
  var days = value(result, 'CFG_BRIEF_DAYS', null);
  if (days && days.length === 7) next.briefDays = lib.daysToMask(days);

  return { settings: next, logout: !!value(result, 'CFG_LOGOUT', false) };
}

module.exports = {
  buildConfig: buildConfig,
  applyResult: applyResult
};
