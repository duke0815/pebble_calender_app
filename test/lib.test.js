// Tests for src/pkjs/lib.js - run: TZ=Europe/Vienna node test/lib.test.js
var assert = require('assert');
var lib = require('../src/pkjs/lib');
var config = require('../src/pkjs/config');

var tests = [];
function test(name, fn) { tests.push({ name: name, fn: fn }); }

function utf8Bytes(s) { return Buffer.byteLength(s, 'utf8'); }
function local(y, m, d, h, min) { return Math.floor(new Date(y, m - 1, d, h || 0, min || 0).getTime() / 1000); }

test('truncate: short texts stay unchanged', function() {
  assert.strictEqual(lib.truncate('Jour fixe', 48, 63), 'Jour fixe');
  assert.strictEqual(lib.truncate('  Zeile\n zwei ', 48, 63), 'Zeile zwei');
});

test('truncate: character and byte limit, no split umlauts', function() {
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

test('All-day: local midnight of the date, regardless of the time', function() {
  assert.strictEqual(lib.parseGraphDateLocal('2026-10-03T00:00:00.0000000'), local(2026, 10, 3));
});

test('Weekdays <-> bitmask', function() {
  var monFri = [true, true, true, true, true, false, false];
  assert.strictEqual(lib.daysToMask(monFri), 62);
  assert.deepStrictEqual(lib.maskToDays(62), monFri);
  assert.strictEqual(lib.daysToMask([false, false, false, false, false, false, true]), 1); // Sun
});

test('parseTime', function() {
  assert.deepStrictEqual(lib.parseTime('07:05'), { hour: 7, minute: 5 });
  assert.deepStrictEqual(lib.parseTime('kaputt'), { hour: 7, minute: 0 });
});

test('normalizeEvent: cancelled, all-day, location, declined', function() {
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

test('selectEvents cache: window today+tomorrow, sorted, filters', function() {
  var settings = lib.mergeSettings({});
  var win = lib.queryWindow(new Date(2026, 9, 1, 10, 0), lib.CACHE_DAYS);
  var list = lib.selectEvents([
    ev(local(2026, 10, 1, 14), local(2026, 10, 1, 15), 'B'),
    ev(local(2026, 10, 1, 8), local(2026, 10, 1, 9), 'A (vorbei, bleibt)'),
    ev(local(2026, 10, 2, 9), local(2026, 10, 2, 10), 'Morgen'),
    ev(local(2026, 10, 3, 9), local(2026, 10, 3, 10), 'Übermorgen (raus)'),
    ev(local(2026, 10, 1, 12), local(2026, 10, 1, 13), 'Abgelehnt', { declined: true }),
    ev(local(2026, 10, 1), local(2026, 10, 2), 'Ganztägig', { allDay: true }),
    ev(local(2026, 10, 1, 14), local(2026, 10, 1, 15), 'B')  // duplicate from 2nd calendar
  ], settings, win);
  assert.deepStrictEqual(list.map(function(e) { return e.title; }),
    ['Ganztägig', 'A (vorbei, bleibt)', 'B', 'Morgen']);
});

test('selectEvents: hide all-day, show declined', function() {
  var settings = lib.mergeSettings({ showAllDay: false, hideDeclined: false });
  var list = lib.selectEvents([
    ev(local(2026, 10, 1), local(2026, 10, 2), 'Ganztägig', { allDay: true }),
    ev(local(2026, 10, 1, 12), local(2026, 10, 1, 13), 'Abgelehnt', { declined: true })
  ], settings, lib.queryWindow(new Date(2026, 9, 1), 1));
  assert.deepStrictEqual(list.map(function(e) { return e.title; }), ['Abgelehnt']);
});

test('selectEvents single day: max. 12, only this day', function() {
  var many = [];
  for (var i = 0; i < 20; i++) many.push(ev(local(2026, 10, 5, 8) + i * 60, local(2026, 10, 5, 9) + i * 60, 'T' + i));
  many.push(ev(local(2026, 10, 6, 8), local(2026, 10, 6, 9), 'Nächster Tag'));
  var list = lib.selectEvents(many, lib.mergeSettings({}), lib.queryWindow(new Date(2026, 9, 5), 1));
  assert.strictEqual(list.length, lib.MAX_DAY_EVENTS);
  assert.ok(list.every(function(e) { return e.title !== 'Nächster Tag'; }));
});

test('selectEvents cache: today max. 12, tomorrow fills up to 16', function() {
  var many = [];
  for (var i = 0; i < 14; i++) many.push(ev(local(2026, 10, 1, 8) + i * 60, local(2026, 10, 1, 9) + i * 60, 'H' + i));
  for (i = 0; i < 8; i++) many.push(ev(local(2026, 10, 2, 8) + i * 60, local(2026, 10, 2, 9) + i * 60, 'M' + i));
  var list = lib.selectEvents(many, lib.mergeSettings({}), lib.queryWindow(new Date(2026, 9, 1, 10), 2));
  assert.strictEqual(list.length, lib.MAX_CACHE_EVENTS);
  assert.strictEqual(list.filter(function(e) { return e.title[0] === 'H'; }).length, 12);
  assert.strictEqual(list.filter(function(e) { return e.title[0] === 'M'; }).length, 4);
});

test('queryWindow across DST change (2026-10-25)', function() {
  var w = lib.queryWindow(new Date(2026, 9, 24, 22, 0), 2);
  assert.strictEqual(w.start.getHours(), 0);
  assert.strictEqual(w.end.getHours(), 0);
  assert.strictEqual(w.end.getDate(), 26);
});

test('normalizeEvent: long titles up to 80 chars / 95 bytes', function() {
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

test('Clay configuration: defaults and round trip', function() {
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

test('Language: resolution and translation', function() {
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

test('Clay: colors and language', function() {
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
  assert.strictEqual(settings.grayPast, false);
  assert.strictEqual(config.applyResult({ CFG_GRAY_PAST: { value: true } }, [], settings).settings.grayPast, true);
  assert.strictEqual(config.applyResult({ CFG_TOUCH: { value: false } }, [], settings).settings.touch, false);
  assert.strictEqual(settings.autoClose, 5);
  assert.strictEqual(config.applyResult({ CFG_AUTO_CLOSE: { value: '0' } }, [], settings).settings.autoClose, 0);
  assert.strictEqual(config.applyResult({ CFG_AUTO_CLOSE: { value: '10' } }, [], settings).settings.autoClose, 10);
  assert.strictEqual(lib.autoCloseMinutes('7'), 5);
  assert.strictEqual(lib.autoCloseMinutes(undefined), 5);
});

test('Description: key and cleanup', function() {
  var k = lib.eventKey('AAMkAGI2THVSAAA=');
  assert.strictEqual(k, lib.eventKey('AAMkAGI2THVSAAA='));
  assert.notStrictEqual(k, lib.eventKey('AAMkAGI2THVSAAB='));
  assert.ok(k !== 0 && (k | 0) === k, 'int32, never 0');
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

test('Moving: permission, times, Graph body', function() {
  var on = lib.mergeSettings({ moveEnabled: true });
  var solo = { isOrganizer: true, attendees: 0, allDay: false };
  assert.strictEqual(lib.isMovable(solo, on), true);
  assert.strictEqual(lib.isMovable({ isOrganizer: true, attendees: 3 }, on), false);
  assert.strictEqual(lib.isMovable({ isOrganizer: true, attendees: 3 }, lib.mergeSettings({ moveEnabled: true, moveScope: 'own' })), true);
  assert.strictEqual(lib.isMovable({ isOrganizer: false, attendees: 0 }, lib.mergeSettings({ moveEnabled: true, moveScope: 'own' })), false);
  assert.strictEqual(lib.isMovable(solo, lib.mergeSettings({})), false, 'off = never');

  // Across the DST change: +1 week from Sat 24.10. 09:00 CEST -> Sat 31.10. 09:00 CET
  var e = { start: local(2026, 10, 24, 9), end: local(2026, 10, 24, 10), allDay: false };
  var week = lib.shiftEvent(e, 3);
  assert.strictEqual(week.start, local(2026, 10, 31, 9), 'local time of day is kept');
  assert.strictEqual(lib.shiftEvent(e, 0).start, e.start + 3600);
  // Next Monday: Sat 24.10. -> Mon 26.10. (across the DST change), Mon 26.10. -> Mon 02.11.
  assert.strictEqual(lib.shiftEvent(e, 4).start, local(2026, 10, 26, 9));
  var monday = { start: local(2026, 10, 26, 14), end: local(2026, 10, 26, 15), allDay: false };
  assert.strictEqual(lib.shiftEvent(monday, 4).start, local(2026, 11, 2, 14));
  var sunday = { start: local(2026, 11, 1), end: local(2026, 11, 2), allDay: true };
  assert.strictEqual(lib.shiftEvent(sunday, 4).start, local(2026, 11, 2));
  assert.deepStrictEqual(lib.graphMoveBody(e, 1), {
    start: { dateTime: new Date(local(2026, 10, 25, 9) * 1000).toISOString().slice(0, 19), timeZone: 'UTC' },
    end: { dateTime: new Date(local(2026, 10, 25, 10) * 1000).toISOString().slice(0, 19), timeZone: 'UTC' }
  });
  var allDay = { start: local(2026, 10, 1), end: local(2026, 10, 2), allDay: true };
  assert.strictEqual(lib.shiftEvent(allDay, 0), null, 'no hour option for all-day');
  assert.deepStrictEqual(lib.graphMoveBody(allDay, 1), {
    start: { dateTime: '2026-10-02T00:00:00', timeZone: 'UTC' },
    end: { dateTime: '2026-10-03T00:00:00', timeZone: 'UTC' }
  });
  assert.strictEqual(lib.eventFlags(solo, on), 1 | 8);
  assert.strictEqual(lib.eventFlags({ isOrganizer: false, attendees: 2 }, on), 2 | 4 | 8);
  assert.strictEqual(lib.eventFlags(solo, lib.mergeSettings({})), 0);
  var n = lib.normalizeEvent({ id: 'a', isOrganizer: true, attendees: [{}, {}], start: { dateTime: '2026-10-01T07:00:00' },
                               end: { dateTime: '2026-10-01T08:00:00' } });
  assert.strictEqual(n.attendees, 2);
  // Organizer in the attendee list does not count
  var me = { emailAddress: { address: 'Me@Example.com' } };
  var self = lib.normalizeEvent({ id: 'b', isOrganizer: true, organizer: me,
    attendees: [{ emailAddress: { address: 'me@example.com' }, status: { response: 'none' } }],
    start: { dateTime: '2026-10-01T07:00:00' }, end: { dateTime: '2026-10-01T08:00:00' } });
  assert.strictEqual(self.attendees, 0);
  var self2 = lib.normalizeEvent({ id: 'c', isOrganizer: true,
    attendees: [{ emailAddress: { address: 'x@y.z' }, status: { response: 'organizer' } }],
    start: { dateTime: '2026-10-01T07:00:00' }, end: { dateTime: '2026-10-01T08:00:00' } });
  assert.strictEqual(self2.attendees, 0);
  assert.strictEqual(lib.isMovable(self, on), true);

  var applied = config.applyResult({ CFG_MOVE: { value: true }, CFG_MOVE_SCOPE: { value: 'own' },
                                     CFG_MOVE_OPTS: { value: [true, false, false, true] } }, [], lib.mergeSettings({}));
  assert.strictEqual(applied.settings.moveEnabled, true);
  assert.strictEqual(applied.settings.moveScope, 'own');
  assert.strictEqual(applied.settings.moveOptions, 9);
});

test('Responding to invitations: permission, flags, settings', function() {
  var on = lib.mergeSettings({ respondEnabled: true });
  var invite = lib.normalizeEvent({ id: 'i', isOrganizer: false, responseStatus: { response: 'tentativelyAccepted' },
    attendees: [{}], start: { dateTime: '2026-10-01T07:00:00' }, end: { dateTime: '2026-10-01T08:00:00' } });
  assert.strictEqual(invite.response, 2);
  assert.strictEqual(lib.isRespondable(invite, on), true);
  assert.strictEqual(lib.isRespondable(invite, lib.mergeSettings({})), false, 'off = never');
  assert.strictEqual(lib.isRespondable({ isOrganizer: true }, on), false, 'own events cannot be answered');
  // bit 4 respondable, bits 5-6 response; moving off -> no bit 3
  assert.strictEqual(lib.eventFlags(invite, on), 2 | 4 | 16 | (2 << 5));
  assert.strictEqual(lib.eventFlags(invite, lib.mergeSettings({})), 2 | 4 | (2 << 5));
  var none = lib.normalizeEvent({ id: 'j', responseStatus: { response: 'notResponded' },
    start: { dateTime: '2026-10-01T07:00:00' }, end: { dateTime: '2026-10-01T08:00:00' } });
  assert.strictEqual(none.response, 0);
  assert.strictEqual(lib.normalizeEvent({ id: 'k', responseStatus: { response: 'declined' },
    start: { dateTime: '2026-10-01T07:00:00' }, end: { dateTime: '2026-10-01T08:00:00' } }).response, 3);
  assert.deepStrictEqual(lib.RESPONSES, ['accept', 'tentativelyAccept', 'decline']);
  assert.strictEqual(lib.needsWriteAccess(lib.mergeSettings({})), false);
  assert.strictEqual(lib.needsWriteAccess(on), true);
  assert.strictEqual(lib.needsWriteAccess(lib.mergeSettings({ moveEnabled: true })), true);
  var applied = config.applyResult({ CFG_RESPOND: { value: true } }, [], lib.mergeSettings({}));
  assert.strictEqual(applied.settings.respondEnabled, true);
  assert.strictEqual(config.applyResult({}, [], lib.mergeSettings({})).settings.respondEnabled, false);
});

test('demo: sample events run through the normal pipeline', function() {
  var demo = require('../src/pkjs/demo');
  var settings = lib.mergeSettings({ moveEnabled: true });
  var now = new Date(2026, 9, 1, 9, 15);  // Thu Oct 1 2026
  var win = lib.queryWindow(now, 2);
  var url = 'https://graph.microsoft.com/v1.0/me/calendar/calendarView?startDateTime=' +
            encodeURIComponent(win.start.toISOString()) + '&endDateTime=' + encodeURIComponent(win.end.toISOString());
  var r = demo.handle('GET', url, null, now);
  assert.strictEqual(r[0], 200);
  var events = lib.selectEvents(r[1].value.map(lib.normalizeEvent), settings, win);
  var today = events.filter(function(e) { return e.start < local(2026, 10, 2); });
  assert.strictEqual(today.length, 6);
  assert.strictEqual(today[0].allDay, true);
  assert.strictEqual(today[0].title, 'Project deadline');
  assert.strictEqual(today[1].start, local(2026, 10, 1, 8, 30));
  assert.strictEqual(events.length, 10);
  // own event without attendees is movable, invitation is not
  var dentist = events.filter(function(e) { return e.title === 'Dentist appointment'; })[0];
  var standup = events.filter(function(e) { return e.title === 'Team stand-up'; })[0];
  assert.ok(lib.isMovable(dentist, settings));
  assert.ok(!lib.isMovable(standup, settings));
  // Teams block is cut from the description
  assert.strictEqual(standup.notes, 'Yesterday, today, blockers. Keep it short.');
  // Description fallback and moving (+1 day)
  var noteUrl = 'https://graph.microsoft.com/v1.0/me/events/' + encodeURIComponent(dentist.id) + '?$select=bodyPreview';
  assert.strictEqual(demo.handle('GET', noteUrl, null, now)[1].bodyPreview, 'Bring insurance card.');
  var patch = demo.handle('PATCH', noteUrl.split('?')[0], JSON.stringify(lib.graphMoveBody(dentist, 1)), now);
  assert.strictEqual(patch[0], 200);
  var after = demo.handle('GET', url, null, now)[1].value.map(lib.normalizeEvent).filter(function(e) {
    return e.title === 'Dentist appointment';
  });
  assert.strictEqual(after.length, 1);
  assert.strictEqual(after[0].start, local(2026, 10, 2, 15, 0));
  // Responding: invitations only, the new response shows up in the list
  var workshop = events.filter(function(e) { return e.title === 'Design workshop'; })[0];
  assert.strictEqual(workshop.response, 0);
  var respUrl = 'https://graph.microsoft.com/v1.0/me/events/' + encodeURIComponent(workshop.id) + '/tentativelyAccept';
  assert.strictEqual(demo.handle('POST', respUrl, '{"sendResponse":true}', now)[0], 202);
  var ownUrl = 'https://graph.microsoft.com/v1.0/me/events/' + encodeURIComponent(dentist.id) + '/accept';
  assert.strictEqual(demo.handle('POST', ownUrl, '{"sendResponse":true}', now)[0], 400);
  var answered = demo.handle('GET', url, null, now)[1].value.map(lib.normalizeEvent).filter(function(e) {
    return e.title === 'Design workshop';
  })[0];
  assert.strictEqual(answered.response, 2);
  // Other days and weekends have events too, other calendars are empty
  var sat = lib.queryWindow(new Date(2026, 9, 3), 1);
  var satUrl = 'https://graph.microsoft.com/v1.0/me/calendar/calendarView?startDateTime=' +
               encodeURIComponent(sat.start.toISOString()) + '&endDateTime=' + encodeURIComponent(sat.end.toISOString());
  assert.strictEqual(demo.handle('GET', satUrl, null, now)[1].value[0].subject, 'Farmers market');
  assert.strictEqual(demo.handle('GET', satUrl.replace('/me/calendar/', '/me/calendars/other/'), null, now)[1].value.length, 0);
});

test('demo mode is off in the published code', function() {
  var src = require('fs').readFileSync(require('path').join(__dirname, '../src/pkjs/index.js'), 'utf8');
  assert.ok(/\nvar DEMO = false;/.test(src), 'DEMO must be false');
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
console.log(failed ? failed + ' test(s) failed' : 'All ' + tests.length + ' tests ok');
process.exit(failed ? 1 : 0);
