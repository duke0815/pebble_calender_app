// Reine Logik ohne Pebble-/Netzwerk-Abhaengigkeiten – mit Node testbar (test/lib.test.js).
// ES5, da PebbleKit JS nicht ueberall moderne Syntax unterstuetzt.

var MSG = {
  EVENT: 1,
  END: 2,
  STATUS: 3,
  LOGIN_CODE: 4,
  SETTINGS: 5,
  REQUEST: 10
};

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

var DEFAULT_SETTINGS = {
  calendars: [],              // leere Liste = Standardkalender
  showAllDay: true,
  hideDeclined: true,
  briefEnabled: false,
  briefTime: '07:00',
  briefDays: 62               // Bitmaske, Bit 0 = Sonntag; Mo-Fr
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
    title: truncate(e.subject || '', TITLE_CHARS, TITLE_BYTES),
    location: truncate((e.location && e.location.displayName) || '', LOCATION_CHARS, LOCATION_BYTES),
    declined: !!(e.responseStatus && e.responseStatus.response === 'declined')
  };
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
    return { code: STATUS.ADMIN_CONSENT, text: 'Deine Firmen-IT muss die App freigeben.', clearTokens: true };
  }
  if (POLICY_CODES.indexOf(aad) >= 0) {
    return { code: STATUS.POLICY_BLOCKED, text: 'Firmenrichtlinie blockiert diese Anmeldung.', clearTokens: true };
  }
  if (json.error === 'invalid_grant' || json.error === 'interaction_required' ||
      json.error === 'expired_token' || json.error === 'authorization_declined' ||
      json.error === 'bad_verification_code') {
    return { code: STATUS.NEED_LOGIN, text: 'Bitte App neu öffnen zum Anmelden.', clearTokens: true };
  }
  if (json.error === 'invalid_client' || json.error === 'unauthorized_client') {
    return { code: STATUS.CONFIG, text: 'App-Registrierung fehlerhaft (Client-ID).', clearTokens: true };
  }
  return { code: STATUS.ERROR, text: 'Anmeldefehler' + (aad ? ' ' + aad : ''), clearTokens: false };
}

function graphErrorStatus(httpStatus) {
  if (httpStatus === 0) return { code: STATUS.NETWORK, text: 'Keine Verbindung.' };
  if (httpStatus === 403) return { code: STATUS.ERROR, text: 'Kein Zugriff auf Kalender.' };
  return { code: STATUS.ERROR, text: 'Graph-Fehler ' + httpStatus };
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
  selectEvents: selectEvents,
  classifyAuthError: classifyAuthError,
  graphErrorStatus: graphErrorStatus,
  formEncode: formEncode
};
