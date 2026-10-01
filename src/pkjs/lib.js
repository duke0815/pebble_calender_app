// Reine Logik ohne Pebble-/Netzwerk-Abhaengigkeiten – mit Node testbar (test/lib.test.js).
// ES5, da PebbleKit JS nicht ueberall moderne Syntax unterstuetzt.

var i18n = require('./i18n');

var MSG = {
  EVENT: 1,
  END: 2,
  STATUS: 3,
  LOGIN_CODE: 4,
  SETTINGS: 5,
  NOTE: 6,
  MOVE_RESULT: 7,
  REQUEST: 10,
  NOTE_REQUEST: 12,
  MOVE: 13
};

// Verschiebe-Optionen: Index = MOVE_OPT im Protokoll = Bit in settings.moveOptions
var MOVE_OPTIONS = [
  { hours: 1 },   // 0: 1 Stunde später
  { days: 1 },    // 1: 1 Tag später
  { days: 2 },    // 2: 2 Tage später
  { days: 7 },    // 3: 1 Woche später
  { weekday: 1 }  // 4: nächster Montag (ist der Termin montags: Montag in einer Woche)
];

// Tage bis zum nächsten Wochentag wday (0 = Sonntag) nach dem Datum des Termins, 1..7
function daysUntilWeekday(sec, wday) {
  var current = new Date(sec * 1000).getDay();
  return ((wday - current + 6) % 7) + 1;
}

var STATUS = {
  SYNCING: 1,
  NEED_LOGIN: 2,
  ADMIN_CONSENT: 3,
  POLICY_BLOCKED: 4,
  NETWORK: 5,
  ERROR: 6,
  CONFIG: 7
};

// Muss zu src/c/protocol.h passen
var MAX_DAY_EVENTS = 12;      // Termine pro Tag
var MAX_CACHE_EVENTS = 16;    // Cache-Liste heute + morgen
var CACHE_DAYS = 2;
var TITLE_CHARS = 80;
var TITLE_BYTES = 95;         // Puffer auf der Uhr: 96 inkl. '\0'
var LOCATION_CHARS = 40;
var LOCATION_BYTES = 47;      // Puffer auf der Uhr: 48 inkl. '\0'
var NOTE_CHARS = 300;
var NOTE_BYTES = 400;         // passt in eine AppMessage (Inbox 640 Byte auf der Uhr)

var DEFAULT_SETTINGS = {
  calendars: [],              // leere Liste = Standardkalender
  showAllDay: true,
  hideDeclined: true,
  briefEnabled: false,
  briefTime: '07:00',
  briefDays: 62,              // Bitmaske, Bit 0 = Sonntag; Mo-Fr
  language: 'auto',           // 'auto' | 'de' | 'en'
  touch: true,                // Touch-Bedienung auf der Uhr (Pebble Time 2)
  grayPast: false,            // vergangene Termine grau anzeigen
  moveEnabled: false,         // Termine verschieben (braucht Calendars.ReadWrite)
  moveScope: 'solo',          // 'solo' = eigene Termine ohne Teilnehmer, 'own' = alle eigenen
  moveOptions: 10,            // Bitmaske über MOVE_OPTIONS; Default: 1 Tag + 1 Woche
  accentColor: 0x0055AA,      // 0xRRGGBB (Pebble-Palette)
  textColor: 0x000000,
  bgColor: 0xFFFFFF
};

function mergeSettings(stored) {
  var result = {};
  var key;
  for (key in DEFAULT_SETTINGS) {
    if (DEFAULT_SETTINGS.hasOwnProperty(key)) {
      result[key] = DEFAULT_SETTINGS[key];
    }
  }
  if (stored && typeof stored === 'object') {
    for (key in stored) {
      if (stored.hasOwnProperty(key) && result.hasOwnProperty(key)) {
        result[key] = stored[key];
      }
    }
  }
  return result;
}

// ---------------------------------------------------------------------------
// Texte

function utf8Length(ch) {
  var c = ch.codePointAt ? ch.codePointAt(0) : ch.charCodeAt(0);
  if (c < 0x80) return 1;
  if (c < 0x800) return 2;
  if (c < 0x10000) return 3;
  return 4;
}

// Zerlegt in Codepoints (Surrogatpaare bleiben zusammen).
function codePoints(str) {
  var result = [];
  for (var i = 0; i < str.length; i++) {
    var c = str.charCodeAt(i);
    if (c >= 0xD800 && c <= 0xDBFF && i + 1 < str.length) {
      result.push(str.charAt(i) + str.charAt(i + 1));
      i++;
    } else {
      result.push(str.charAt(i));
    }
  }
  return result;
}

// Kuerzt auf maxChars Zeichen UND maxBytes UTF-8-Bytes, haengt ggf. "..." an.
function truncate(str, maxChars, maxBytes) {
  str = String(str || '').replace(/[\r\n\t]+/g, ' ').replace(/\s+/g, ' ').trim();
  var chars = codePoints(str);
  var total = 0;
  var i;
  for (i = 0; i < chars.length; i++) total += utf8Length(chars[i]);
  if (chars.length <= maxChars && total <= maxBytes) return str;

  var suffix = '...';
  var out = '';
  var bytes = 0;
  for (i = 0; i < chars.length; i++) {
    var len = utf8Length(chars[i]);
    if (i + 1 + suffix.length > maxChars || bytes + len + suffix.length > maxBytes) break;
    out += chars[i];
    bytes += len;
  }
  return out.replace(/\s+$/, '') + suffix;
}

// ---------------------------------------------------------------------------
// Zeiten

// "2026-10-01T07:30:00.0000000" (UTC, ohne Z) -> Epoch-Sekunden
function parseGraphUtc(dateTime) {
  var m = /^(\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2})(?::(\d{2}))?/.exec(dateTime || '');
  if (!m) return NaN;
  return Math.floor(Date.UTC(+m[1], +m[2] - 1, +m[3], +m[4], +m[5], +(m[6] || 0)) / 1000);
}

// Ganztaegig: nur das Datum verwenden -> lokale Mitternacht dieses Datums
function parseGraphDateLocal(dateTime) {
  var m = /^(\d{4})-(\d{2})-(\d{2})/.exec(dateTime || '');
  if (!m) return NaN;
  return Math.floor(new Date(+m[1], +m[2] - 1, +m[3]).getTime() / 1000);
}

function localDayStart(date) {
  return new Date(date.getFullYear(), date.getMonth(), date.getDate());
}

function addDays(date, days) {
  return new Date(date.getFullYear(), date.getMonth(), date.getDate() + days);
}

// Abfragezeitraum: days Tage ab lokaler Mitternacht von day
function queryWindow(day, days) {
  var start = localDayStart(day);
  return { start: start, end: addDays(start, days) };
}

function parseTime(text) {
  var m = /^(\d{1,2}):(\d{2})/.exec(text || '');
  if (!m) return { hour: 7, minute: 0 };
  return { hour: Math.min(23, +m[1]), minute: Math.min(59, +m[2]) };
}

// Clay-Checkboxen sind Mo..So, Bitmaske ist Bit 0 = Sonntag
function daysToMask(monFirst) {
  var mask = 0;
  for (var i = 0; i < 7; i++) {
    if (monFirst[i]) mask |= 1 << ((i + 1) % 7);
  }
  return mask;
}

function maskToDays(mask) {
  var result = [];
  for (var i = 0; i < 7; i++) {
    result.push(!!(mask & (1 << ((i + 1) % 7))));
  }
  return result;
}

// ---------------------------------------------------------------------------
// Termine

// Kurze Kennung (32 Bit, nie 0) für die lange Graph-ID – damit fragt die Uhr die
// Beschreibung an, ohne die ID speichern zu müssen (FNV-1a).
function eventKey(id) {
  var h = 0x811c9dc5;
  var s = String(id || '');
  for (var i = 0; i < s.length; i++) {
    h ^= s.charCodeAt(i);
    h = Math.imul ? Math.imul(h, 0x01000193) : (h * 0x01000193) >>> 0;
  }
  h = h | 0;
  return h === 0 ? 1 : h;
}

// Beschreibung (bodyPreview) für die Uhr aufbereiten: Teams-/Zoom-Einladungsblock
// abschneiden, Leerzeilen zusammenfassen, Zeilenumbrüche behalten, kürzen.
function cleanNotes(text) {
  var s = String(text || '').replace(/\r\n?/g, '\n');
  var cut = s.search(/_{8,}|-{8,}|Microsoft Teams[- ]?(Besprechung|meeting|Meeting)/);
  if (cut >= 0) s = s.slice(0, cut);
  s = s.split('\n').map(function(line) { return line.replace(/[ \t]+/g, ' ').trim(); }).join('\n');
  s = s.replace(/\n{3,}/g, '\n\n').trim();
  if (!s) return '';
  // Kürzen wie truncate(), aber Zeilenumbrüche erhalten
  var chars = codePoints(s);
  var bytes = 0;
  var out = '';
  for (var i = 0; i < chars.length; i++) {
    var len = utf8Length(chars[i]);
    if (i + 1 + 3 > NOTE_CHARS || bytes + len + 3 > NOTE_BYTES) {
      return out.replace(/\s+$/, '') + '...';
    }
    out += chars[i];
    bytes += len;
  }
  return out;
}

// Teilnehmer ohne den Organisator selbst: Outlook führt den Organisator teils auch in
// attendees (mit status.response = 'organizer') – der zählt nicht als "hat Teilnehmer".
function countAttendees(e) {
  var organizer = String((e.organizer && e.organizer.emailAddress && e.organizer.emailAddress.address) || '')
    .toLowerCase();
  return (e.attendees || []).filter(function(a) {
    var address = String((a.emailAddress && a.emailAddress.address) || '').toLowerCase();
    var response = a.status && a.status.response;
    return response !== 'organizer' && !(organizer && address === organizer);
  }).length;
}

function normalizeEvent(e) {
  if (!e || e.isCancelled) return null;
  var allDay = !!e.isAllDay;
  var start = allDay ? parseGraphDateLocal(e.start && e.start.dateTime)
                     : parseGraphUtc(e.start && e.start.dateTime);
  var end = allDay ? parseGraphDateLocal(e.end && e.end.dateTime)
                   : parseGraphUtc(e.end && e.end.dateTime);
  if (isNaN(start) || isNaN(end)) return null;
  if (end < start) end = start;
  return {
    start: start,
    end: end,
    allDay: allDay,
    key: eventKey(e.id),
    id: e.id || '',
    notes: e.hasOwnProperty('bodyPreview') ? cleanNotes(e.bodyPreview) : null,
    title: truncate(e.subject || '', TITLE_CHARS, TITLE_BYTES),
    location: truncate((e.location && e.location.displayName) || '', LOCATION_CHARS, LOCATION_BYTES),
    declined: !!(e.responseStatus && e.responseStatus.response === 'declined'),
    isOrganizer: !!e.isOrganizer,
    attendees: countAttendees(e)
  };
}

// ---------------------------------------------------------------------------
// Verschieben

// Nur eigene Termine (Organisator); je nach Einstellung nur ohne Teilnehmer.
// Einladungen anderer sind nie verschiebbar.
function isMovable(e, settings) {
  if (!e || !settings.moveEnabled || !e.isOrganizer) return false;
  return settings.moveScope === 'own' || !e.attendees;
}

// EVT_FLAGS für die Uhr: Bit 0 verschiebbar; Gründe, warum nicht (für den Hinweis in der
// Detailansicht): Bit 1 = nicht Organisator, Bit 2 = hat Teilnehmer, Bit 3 = Verschieben am Handy an
function moveFlags(e, settings) {
  var flags = 0;
  if (isMovable(e, settings)) flags |= 1;
  if (!e.isOrganizer) flags |= 2;
  if (e.attendees) flags |= 4;
  if (settings.moveEnabled) flags |= 8;
  return flags;
}

function pad2(n) {
  return (n < 10 ? '0' : '') + n;
}

// Neue Zeiten (Epoch-Sekunden) für Option opt; Tage behalten die lokale Uhrzeit
// (auch über die Zeitumstellung). null, wenn die Option nicht passt (Stunde bei ganztägig).
function shiftEvent(e, opt) {
  var o = MOVE_OPTIONS[opt];
  if (!o) return null;
  if (o.hours) {
    if (e.allDay) return null;
    return { start: e.start + o.hours * 3600, end: e.end + o.hours * 3600 };
  }
  var days = o.weekday !== undefined ? daysUntilWeekday(e.start, o.weekday) : o.days;
  function addDays(sec) {
    var d = new Date(sec * 1000);
    d.setDate(d.getDate() + days);
    return Math.floor(d.getTime() / 1000);
  }
  return { start: addDays(e.start), end: addDays(e.end) };
}

function utcDateTime(sec) {
  return new Date(sec * 1000).toISOString().slice(0, 19);
}

function localDate(sec) {
  var d = new Date(sec * 1000);
  return d.getFullYear() + '-' + pad2(d.getMonth() + 1) + '-' + pad2(d.getDate()) + 'T00:00:00';
}

// Body für PATCH /me/events/{id}
function graphMoveBody(e, opt) {
  var t = shiftEvent(e, opt);
  if (!t) return null;
  if (e.allDay) {
    return { start: { dateTime: localDate(t.start), timeZone: 'UTC' },
             end: { dateTime: localDate(t.end), timeZone: 'UTC' } };
  }
  return { start: { dateTime: utcDateTime(t.start), timeZone: 'UTC' },
           end: { dateTime: utcDateTime(t.end), timeZone: 'UTC' } };
}

// Filtert, sortiert und kuerzt die normalisierten Termine fuer die Uhr.
// win = queryWindow(...): max. MAX_DAY_EVENTS pro Tag; bei mehreren Tagen (Cache)
// insgesamt max. MAX_CACHE_EVENTS, wobei der erste Tag Vorrang hat.
function selectEvents(events, settings, win) {
  var winStart = Math.floor(win.start.getTime() / 1000);
  var winEnd = Math.floor(win.end.getTime() / 1000);
  var dayStarts = [];
  for (var d = win.start; d < win.end; d = addDays(d, 1)) {
    dayStarts.push(Math.floor(d.getTime() / 1000));
  }
  var seen = {};

  var list = events.filter(function(e) {
    if (!e) return false;
    if (e.allDay && !settings.showAllDay) return false;
    if (e.declined && settings.hideDeclined) return false;
    if (e.start >= winEnd || e.end <= winStart) return false;
    // Gleicher Termin in mehreren Kalendern -> nur einmal
    var key = e.start + '|' + e.end + '|' + e.title;
    if (seen[key]) return false;
    seen[key] = true;
    return true;
  });

  list.sort(function(a, b) {
    if (a.start !== b.start) return a.start - b.start;
    if (a.allDay !== b.allDay) return a.allDay ? -1 : 1;
    return a.end - b.end;
  });

  // Pro Tag zählen (Termine, die vorher begonnen haben, zählen zum ersten Tag)
  var perDay = dayStarts.map(function() { return 0; });
  var result = [];
  var total = dayStarts.length > 1 ? MAX_CACHE_EVENTS : MAX_DAY_EVENTS;
  list.forEach(function(e) {
    if (result.length >= total) return;
    var idx = 0;
    for (var i = dayStarts.length - 1; i > 0; i--) {
      if (e.start >= dayStarts[i]) { idx = i; break; }
    }
    if (perDay[idx] >= MAX_DAY_EVENTS) return;
    perDay[idx]++;
    result.push(e);
  });
  return result;
}

// ---------------------------------------------------------------------------
// Fehler

var ADMIN_CONSENT_CODES = ['65001', '90094', '90095'];
var POLICY_CODES = ['53000', '53001', '53002', '53003', '50097'];

// Liefert { code, text, clearTokens } fuer eine fehlgeschlagene Token-Antwort.
function classifyAuthError(json) {
  json = json || {};
  var desc = String(json.error_description || '');
  var match = /AADSTS(\d+)/.exec(desc);
  var aad = match ? match[1] : '';
  if (ADMIN_CONSENT_CODES.indexOf(aad) >= 0) {
    return { code: STATUS.ADMIN_CONSENT, text: i18n.t('adminConsent'), clearTokens: true };
  }
  if (POLICY_CODES.indexOf(aad) >= 0) {
    return { code: STATUS.POLICY_BLOCKED, text: i18n.t('policyBlocked'), clearTokens: true };
  }
  if (json.error === 'invalid_grant' || json.error === 'interaction_required' ||
      json.error === 'expired_token' || json.error === 'authorization_declined' ||
      json.error === 'bad_verification_code') {
    return { code: STATUS.NEED_LOGIN, text: i18n.t('reopenLogin'), clearTokens: true };
  }
  if (json.error === 'invalid_client' || json.error === 'unauthorized_client') {
    return { code: STATUS.CONFIG, text: i18n.t('badClient'), clearTokens: true };
  }
  return { code: STATUS.ERROR, text: i18n.t('authError') + (aad ? ' ' + aad : ''), clearTokens: false };
}

function graphErrorStatus(httpStatus) {
  if (httpStatus === 0) return { code: STATUS.NETWORK, text: i18n.t('noConnection') };
  if (httpStatus === 403) return { code: STATUS.ERROR, text: i18n.t('noCalendarAccess') };
  return { code: STATUS.ERROR, text: i18n.t('graphError') + ' ' + httpStatus };
}

function formEncode(obj) {
  var parts = [];
  for (var key in obj) {
    if (obj.hasOwnProperty(key)) {
      parts.push(encodeURIComponent(key) + '=' + encodeURIComponent(obj[key]));
    }
  }
  return parts.join('&');
}

module.exports = {
  MSG: MSG,
  STATUS: STATUS,
  MAX_DAY_EVENTS: MAX_DAY_EVENTS,
  MAX_CACHE_EVENTS: MAX_CACHE_EVENTS,
  CACHE_DAYS: CACHE_DAYS,
  DEFAULT_SETTINGS: DEFAULT_SETTINGS,
  mergeSettings: mergeSettings,
  truncate: truncate,
  parseGraphUtc: parseGraphUtc,
  parseGraphDateLocal: parseGraphDateLocal,
  queryWindow: queryWindow,
  parseTime: parseTime,
  daysToMask: daysToMask,
  maskToDays: maskToDays,
  normalizeEvent: normalizeEvent,
  MOVE_OPTIONS: MOVE_OPTIONS,
  isMovable: isMovable,
  moveFlags: moveFlags,
  shiftEvent: shiftEvent,
  graphMoveBody: graphMoveBody,
  eventKey: eventKey,
  cleanNotes: cleanNotes,
  selectEvents: selectEvents,
  classifyAuthError: classifyAuthError,
  graphErrorStatus: graphErrorStatus,
  formEncode: formEncode
};
