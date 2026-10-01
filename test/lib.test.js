// Tests für src/pkjs/lib.js – Ausführen: TZ=Europe/Vienna node test/lib.test.js
var assert = require('assert');
var lib = require('../src/pkjs/lib');
var config = require('../src/pkjs/config');

var tests = [];
function test(name, fn) { tests.push({ name: name, fn: fn }); }

function utf8Bytes(s) { return Buffer.byteLength(s, 'utf8'); }
function local(y, m, d, h, min) { return Math.floor(new Date(y, m - 1, d, h || 0, min || 0).getTime() / 1000); }

test('truncate: kurze Texte bleiben unverändert', function() {
  assert.strictEqual(lib.truncate('Jour fixe', 48, 63), 'Jour fixe');
  assert.strictEqual(lib.truncate('  Zeile\n zwei ', 48, 63), 'Zeile zwei');
});

test('truncate: Zeichen- und Byte-Grenze, keine zerschnittenen Umlaute', function() {
  var long = 'Ä'.repeat(60);
  var out = lib.truncate(long, 48, 63);
  assert.ok(utf8Bytes(out) <= 63, 'Bytes: ' + utf8Bytes(out));
  assert.ok(/\.\.\.$/.test(out));
  assert.ok(out.indexOf('�') < 0);
  var ascii = lib.truncate('x'.repeat(100), 48, 63);
  assert.strictEqual(ascii.length, 48);
  var emoji = lib.truncate('🎉'.repeat(30), 48, 63);
  assert.ok(utf8Bytes(emoji) <= 63);
});

test('parseGraphUtc', function() {
  assert.strictEqual(lib.parseGraphUtc('2026-10-01T07:30:00.0000000'), Date.UTC(2026, 9, 1, 7, 30) / 1000);
  assert.ok(isNaN(lib.parseGraphUtc('')));
});

test('Ganztägig: lokale Mitternacht des Datums, unabhängig von der Uhrzeit', function() {
  assert.strictEqual(lib.parseGraphDateLocal('2026-10-03T00:00:00.0000000'), local(2026, 10, 3));
});

test('Wochentage <-> Bitmaske', function() {
  var monFri = [true, true, true, true, true, false, false];
  assert.strictEqual(lib.daysToMask(monFri), 62);
  assert.deepStrictEqual(lib.maskToDays(62), monFri);
  assert.strictEqual(lib.daysToMask([false, false, false, false, false, false, true]), 1); // So
});

test('parseTime', function() {
  assert.deepStrictEqual(lib.parseTime('07:05'), { hour: 7, minute: 5 });
  assert.deepStrictEqual(lib.parseTime('kaputt'), { hour: 7, minute: 0 });
});

test('normalizeEvent: abgesagt, ganztägig, Ort, abgelehnt', function() {
  assert.strictEqual(lib.normalizeEvent({ isCancelled: true }), null);
  var e = lib.normalizeEvent({
    subject: 'Urlaub', isAllDay: true,
    start: { dateTime: '2026-10-02T00:00:00.0000000', timeZone: 'UTC' },
    end: { dateTime: '2026-10-05T00:00:00.0000000', timeZone: 'UTC' },
    location: { displayName: 'Kärnten' },
    responseStatus: { response: 'declined' }
  });
  assert.strictEqual(e.start, local(2026, 10, 2));
  assert.strictEqual(e.end, local(2026, 10, 5));
  assert.strictEqual(e.allDay, true);
  assert.strictEqual(e.location, 'Kärnten');
  assert.strictEqual(e.declined, true);
});

function ev(start, end, title, extra) {
  var e = { start: start, end: end, allDay: false, title: title, location: '', declined: false };
  for (var k in extra || {}) e[k] = extra[k];
  return e;
}

test('selectEvents Cache: Zeitraum heute+morgen, sortiert, Filter', function() {
  var settings = lib.mergeSettings({});
  var win = lib.queryWindow(new Date(2026, 9, 1, 10, 0), lib.CACHE_DAYS);
  var list = lib.selectEvents([
    ev(local(2026, 10, 1, 14), local(2026, 10, 1, 15), 'B'),
    ev(local(2026, 10, 1, 8), local(2026, 10, 1, 9), 'A (vorbei, bleibt)'),
    ev(local(2026, 10, 2, 9), local(2026, 10, 2, 10), 'Morgen'),
    ev(local(2026, 10, 3, 9), local(2026, 10, 3, 10), 'Übermorgen (raus)'),
    ev(local(2026, 10, 1, 12), local(2026, 10, 1, 13), 'Abgelehnt', { declined: true }),
    ev(local(2026, 10, 1), local(2026, 10, 2), 'Ganztägig', { allDay: true }),
    ev(local(2026, 10, 1, 14), local(2026, 10, 1, 15), 'B')  // Duplikat aus 2. Kalender
  ], settings, win);
  assert.deepStrictEqual(list.map(function(e) { return e.title; }),
    ['Ganztägig', 'A (vorbei, bleibt)', 'B', 'Morgen']);
});

test('selectEvents: Ganztägige ausblenden, Abgelehnte zeigen', function() {
  var settings = lib.mergeSettings({ showAllDay: false, hideDeclined: false });
  var list = lib.selectEvents([
    ev(local(2026, 10, 1), local(2026, 10, 2), 'Ganztägig', { allDay: true }),
    ev(local(2026, 10, 1, 12), local(2026, 10, 1, 13), 'Abgelehnt', { declined: true })
  ], settings, lib.queryWindow(new Date(2026, 9, 1), 1));
  assert.deepStrictEqual(list.map(function(e) { return e.title; }), ['Abgelehnt']);
});

test('selectEvents einzelner Tag: max. 12, nur dieser Tag', function() {
  var many = [];
  for (var i = 0; i < 20; i++) many.push(ev(local(2026, 10, 5, 8) + i * 60, local(2026, 10, 5, 9) + i * 60, 'T' + i));
  many.push(ev(local(2026, 10, 6, 8), local(2026, 10, 6, 9), 'Nächster Tag'));
  var list = lib.selectEvents(many, lib.mergeSettings({}), lib.queryWindow(new Date(2026, 9, 5), 1));
  assert.strictEqual(list.length, lib.MAX_DAY_EVENTS);
  assert.ok(list.every(function(e) { return e.title !== 'Nächster Tag'; }));
});

test('selectEvents Cache: heute max. 12, morgen füllt bis 16 auf', function() {
  var many = [];
  for (var i = 0; i < 14; i++) many.push(ev(local(2026, 10, 1, 8) + i * 60, local(2026, 10, 1, 9) + i * 60, 'H' + i));
  for (i = 0; i < 8; i++) many.push(ev(local(2026, 10, 2, 8) + i * 60, local(2026, 10, 2, 9) + i * 60, 'M' + i));
  var list = lib.selectEvents(many, lib.mergeSettings({}), lib.queryWindow(new Date(2026, 9, 1, 10), 2));
  assert.strictEqual(list.length, lib.MAX_CACHE_EVENTS);
  assert.strictEqual(list.filter(function(e) { return e.title[0] === 'H'; }).length, 12);
  assert.strictEqual(list.filter(function(e) { return e.title[0] === 'M'; }).length, 4);
});

test('queryWindow über Zeitumstellung (25.10.2026)', function() {
  var w = lib.queryWindow(new Date(2026, 9, 24, 22, 0), 2);
  assert.strictEqual(w.start.getHours(), 0);
  assert.strictEqual(w.end.getHours(), 0);
  assert.strictEqual(w.end.getDate(), 26);
});

test('normalizeEvent: lange Titel bis 80 Zeichen / 95 Byte', function() {
  var e = lib.normalizeEvent({ subject: 'Ü'.repeat(100), start: { dateTime: '2026-10-01T07:00:00' },
                               end: { dateTime: '2026-10-01T08:00:00' } });
  assert.ok(Buffer.byteLength(e.title, 'utf8') <= 95);
  var a = lib.normalizeEvent({ subject: 'a'.repeat(100), start: { dateTime: '2026-10-01T07:00:00' },
                               end: { dateTime: '2026-10-01T08:00:00' } });
  assert.strictEqual(a.title.length, 80);
});

test('classifyAuthError', function() {
  assert.strictEqual(lib.classifyAuthError({ error: 'invalid_grant',
    error_description: 'AADSTS65001: The user or administrator has not consented' }).code, lib.STATUS.ADMIN_CONSENT);
  assert.strictEqual(lib.classifyAuthError({ error: 'invalid_grant',
    error_description: 'AADSTS53003: Access has been blocked by Conditional Access policies.' }).code,
    lib.STATUS.POLICY_BLOCKED);
  assert.strictEqual(lib.classifyAuthError({ error: 'invalid_grant',
    error_description: 'AADSTS700082: The refresh token has expired' }).code, lib.STATUS.NEED_LOGIN);
  assert.strictEqual(lib.classifyAuthError({ error: 'interaction_required' }).code, lib.STATUS.NEED_LOGIN);
  assert.strictEqual(lib.classifyAuthError({ error: 'invalid_client',
    error_description: 'AADSTS7000218: ... client_assertion or client_secret' }).code, lib.STATUS.CONFIG);
  assert.strictEqual(lib.classifyAuthError(null).code, lib.STATUS.ERROR);
});

test('Clay-Konfiguration: Defaults und Rückweg', function() {
  var cals = [{ id: 'a', name: 'Kalender', isDefaultCalendar: true }, { id: 'b', name: 'Familie' }];
  var settings = lib.mergeSettings({});
  var cfg = config.buildConfig(cals, settings, true);
  var group = cfg[1].items[1];
  assert.deepStrictEqual(group.defaultValue, [true, false]);

  var result = {
    CFG_CALENDARS: { value: [true, true] },
    CFG_ALLDAY: { value: false },
    CFG_HIDE_DECLINED: { value: true },
    CFG_BRIEF: { value: true },
    CFG_BRIEF_TIME: { value: '6:45' },
    CFG_BRIEF_DAYS: { value: [true, false, true, false, true, false, false] },
    CFG_LOGOUT: { value: false }
  };
  var applied = config.applyResult(result, cals, settings);
  assert.deepStrictEqual(applied.settings.calendars, ['a', 'b']);
  assert.strictEqual(applied.settings.showAllDay, false);
  assert.strictEqual(applied.settings.briefEnabled, true);
  assert.strictEqual(applied.settings.briefTime, '06:45');
  assert.strictEqual(applied.settings.briefDays, (1 << 1) | (1 << 3) | (1 << 5));
  assert.strictEqual(applied.logout, false);
});

test('Sprache: Auflösung und Übersetzung', function() {
  var i18n = require('../src/pkjs/i18n');
  assert.strictEqual(i18n.resolveLang('auto', 'de-AT'), 'de');
  assert.strictEqual(i18n.resolveLang('auto', 'en-US'), 'en');
  assert.strictEqual(i18n.resolveLang('auto', ''), 'en');
  assert.strictEqual(i18n.resolveLang('de', 'en-US'), 'de');
  i18n.setLang('en');
  assert.strictEqual(lib.classifyAuthError({ error: 'invalid_grant' }).text, 'Please reopen the app to sign in.');
  assert.strictEqual(lib.graphErrorStatus(500).text, 'Graph error 500');
  var cfg = config.buildConfig([], lib.mergeSettings({}), false);
  assert.strictEqual(cfg[cfg.length - 1].defaultValue, 'Save');
  i18n.setLang('de');
  assert.strictEqual(lib.classifyAuthError({ error: 'invalid_grant' }).text, 'Bitte App neu öffnen zum Anmelden.');
});

test('Clay: Farben und Sprache', function() {
  var settings = lib.mergeSettings({});
  var cfg = config.buildConfig([], settings, true);
  var colors = cfg.filter(function(s) { return s.items && s.items.some(function(i) { return i.type === 'color'; }); })[0];
  assert.deepStrictEqual(colors.items.slice(1).map(function(i) { return i.defaultValue; }), ['0055AA', '000000', 'FFFFFF']);
  var applied = config.applyResult({
    CFG_LANG: { value: 'en' },
    CFG_COLOR_ACCENT: { value: 0xFF5500 },
    CFG_COLOR_TEXT: { value: 'FFFFFF' },
    CFG_COLOR_BG: { value: 0x000000 }
  }, [], settings);
  assert.strictEqual(applied.settings.language, 'en');
  assert.strictEqual(applied.settings.accentColor, 0xFF5500);
  assert.strictEqual(applied.settings.textColor, 0xFFFFFF);
  assert.strictEqual(applied.settings.bgColor, 0x000000);
  var bad = config.applyResult({ CFG_LANG: { value: 'xx' }, CFG_COLOR_BG: { value: 'kaputt' } }, [], settings);
  assert.strictEqual(bad.settings.language, 'auto');
  assert.strictEqual(bad.settings.bgColor, 0xFFFFFF);
  assert.strictEqual(settings.touch, true);
  assert.strictEqual(config.applyResult({ CFG_TOUCH: { value: false } }, [], settings).settings.touch, false);
});

test('Beschreibung: Kennung und Aufbereitung', function() {
  var k = lib.eventKey('AAMkAGI2THVSAAA=');
  assert.strictEqual(k, lib.eventKey('AAMkAGI2THVSAAA='));
  assert.notStrictEqual(k, lib.eventKey('AAMkAGI2THVSAAB='));
  assert.ok(k !== 0 && (k | 0) === k, 'int32, nie 0');
  assert.strictEqual(lib.cleanNotes('Agenda:\r\n- Punkt 1\r\n\r\n\r\n- Punkt 2  '), 'Agenda:\n- Punkt 1\n\n- Punkt 2');
  assert.strictEqual(lib.cleanNotes('Bitte Unterlagen mitbringen. ________________________________ Microsoft Teams-Besprechung Jetzt teilnehmen'),
                     'Bitte Unterlagen mitbringen.');
  assert.strictEqual(lib.cleanNotes('________________ Microsoft Teams meeting Join'), '');
  var long = lib.cleanNotes('Ü'.repeat(400));
  assert.ok(Buffer.byteLength(long, 'utf8') <= 400 && /\.\.\.$/.test(long));
  var e = lib.normalizeEvent({ id: 'X1', subject: 'A', bodyPreview: 'Notiz', start: { dateTime: '2026-10-01T07:00:00' },
                               end: { dateTime: '2026-10-01T08:00:00' } });
  assert.strictEqual(e.notes, 'Notiz');
  assert.strictEqual(e.key, lib.eventKey('X1'));
});

var failed = 0;
tests.forEach(function(t) {
  try {
    t.fn();
    console.log('ok   ' + t.name);
  } catch (e) {
    failed++;
    console.log('FAIL ' + t.name + '\n     ' + e.message);
  }
});
console.log(failed ? failed + ' Test(s) fehlgeschlagen' : 'Alle ' + tests.length + ' Tests ok');
process.exit(failed ? 1 : 0);
