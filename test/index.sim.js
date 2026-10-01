// Simulation von src/pkjs/index.js mit gemocktem Pebble, XMLHttpRequest und localStorage.
// Ausführen: TZ=Europe/Vienna node test/index.sim.js
var assert = require('assert');
var fs = require('fs');
var path = require('path');
var Module = require('module');

// --- Mocks -----------------------------------------------------------------
var store = {};
global.localStorage = {
  getItem: function(k) { return store.hasOwnProperty(k) ? store[k] : null; },
  setItem: function(k, v) { store[k] = String(v); },
  removeItem: function(k) { delete store[k]; }
};

var listeners = {};
var sent = [];
var nackOnce = true;
global.Pebble = {
  addEventListener: function(name, fn) { (listeners[name] = listeners[name] || []).push(fn); },
  sendAppMessage: function(msg, ok, fail) {
    setTimeout(function() {
      // Erste Termin-Nachricht einmal ablehnen -> Retry muss greifen
      if (msg.MSG_TYPE === 1 && nackOnce) { nackOnce = false; fail({}); return; }
      sent.push(msg);
      ok({});
    }, 1);
  },
  openURL: function(url) { global.openedUrl = url; }
};
function fire(name, e) { (listeners[name] || []).forEach(function(fn) { fn(e || {}); }); }

var tokenPolls = 0;
var requests = [];
function respond(method, url, body) {
  requests.push(method + ' ' + url.split('?')[0]);
  if (/devicecode$/.test(url)) {
    return [200, { device_code: 'DEV', user_code: 'ABCD-1234', verification_uri: 'https://microsoft.com/devicelogin',
                   interval: 0.01, expires_in: 900 }];
  }
  if (/token$/.test(url)) {
    if (/device_code/.test(body)) {
      tokenPolls++;
      if (tokenPolls < 3) return [400, { error: 'authorization_pending' }];
      return [200, { access_token: 'AT', refresh_token: 'RT', expires_in: 3600 }];
    }
    return [200, { access_token: 'AT2', expires_in: 3600 }];
  }
  if (/\/me\/calendars?(\/cal1)?\/calendarView/.test(url)) {
    return [200, {
      value: [{ subject: 'Standup', isAllDay: false, start: { dateTime: '2026-10-01T07:00:00.0000000' },
                end: { dateTime: '2026-10-01T07:15:00.0000000' }, location: { displayName: 'Teams' } }],
      '@odata.nextLink': 'https://graph.microsoft.com/v1.0/page2'
    }];
  }
  if (/page2$/.test(url)) {
    return [200, { value: [
      { subject: 'Abgesagt', isCancelled: true, start: { dateTime: '2026-10-01T09:00:00' }, end: { dateTime: '2026-10-01T10:00:00' } },
      { subject: 'Feiertag', isAllDay: true, start: { dateTime: '2026-10-01T00:00:00' }, end: { dateTime: '2026-10-02T00:00:00' } }
    ] }];
  }
  if (/\/me\/calendars\?/.test(url)) {
    return [200, { value: [{ id: 'cal1', name: 'Kalender', isDefaultCalendar: true }] }];
  }
  return [404, { error: { code: 'NotFound' } }];
}

global.XMLHttpRequest = function() {};
global.XMLHttpRequest.prototype = {
  open: function(method, url) { this.method = method; this.url = url; this.headers = {}; },
  setRequestHeader: function(k, v) { this.headers[k] = v; },
  send: function(body) {
    var self = this;
    if (/graph\.microsoft\.com/.test(self.url)) {
      assert.ok(/^Bearer /.test(self.headers.Authorization), 'Authorization fehlt');
      assert.strictEqual(self.headers.Prefer, 'outlook.timezone="UTC"');
    }
    setTimeout(function() {
      var r = respond(self.method, self.url, body);
      self.status = r[0];
      self.responseText = JSON.stringify(r[1]);
      self.onload();
    }, 1);
  }
};

// Module stubs
var origLoad = Module._load;
Module._load = function(request) {
  if (request === 'pebble-clay') {
    var Clay = function(config) { this.config = config; };
    Clay.prototype.generateUrl = function() { return 'data:clay'; };
    Clay.prototype.getSettings = function(r) { return JSON.parse(decodeURIComponent(r)); };
    return Clay;
  }
  if (request === 'message_keys') return { MSG_TYPE: 10000 };
  return origLoad.apply(this, arguments);
};

// index.js mit Test-Client-ID laden
var file = path.join(__dirname, '../src/pkjs/index.js');
var src = fs.readFileSync(file, 'utf8').replace(/var CLIENT_ID = '[^']*';/, "var CLIENT_ID = 'test-client';");
var m = new Module(file, module);
m.filename = file;
m.paths = Module._nodeModulePaths(path.dirname(file));
m._compile(src, file);

// --- Ablauf ------------------------------------------------------------------
function waitFor(cond, ms, cb) {
  var start = Date.now();
  (function check() {
    if (cond()) return cb();
    if (Date.now() - start > ms) { console.log(JSON.stringify(sent), requests); throw new Error('Timeout beim Warten'); }
    setTimeout(check, 5);
  })();
}

fire('ready');
// Doppelte Anfrage der Uhr während laufender Synchronisierung -> nur Login-Code erneut
fire('appmessage', { payload: { MSG_TYPE: 10 } });

waitFor(function() { return sent.some(function(m) { return m.MSG_TYPE === 2; }); }, 3000, function() {
  var types = sent.map(function(m) { return m.MSG_TYPE; });
  assert.strictEqual(types[0], 5, 'zuerst Settings');
  assert.strictEqual(types[1], 3, 'dann Status');
  var login = sent.filter(function(m) { return m.MSG_TYPE === 4; });
  assert.ok(login.length >= 1);
  assert.strictEqual(login[0].LOGIN_URL, 'microsoft.com/devicelogin');
  assert.strictEqual(login[0].LOGIN_CODE, 'ABCD-1234');

  var events = sent.filter(function(m) { return m.MSG_TYPE === 1; });
  assert.deepStrictEqual(events.map(function(e) { return e.EVT_TITLE; }), ['Feiertag', 'Standup']);
  assert.strictEqual(events[0].EVT_ALLDAY, 1);
  assert.strictEqual(events[0].EVT_START, Math.floor(new Date(2026, 9, 1).getTime() / 1000));
  assert.strictEqual(events[1].EVT_START, Date.UTC(2026, 9, 1, 7, 0) / 1000);
  assert.strictEqual(events[1].EVT_LOCATION, 'Teams');
  var end = sent.filter(function(m) { return m.MSG_TYPE === 2; })[0];
  assert.strictEqual(end.EVT_COUNT, 2);

  var auth = JSON.parse(store.auth);
  assert.strictEqual(auth.refresh, 'RT');
  assert.strictEqual(requests.filter(function(r) { return /devicecode$/.test(r); }).length, 1, 'nur ein Login');

  // Einstellungen speichern -> neue Synchronisierung mit Settings an die Uhr
  waitFor(function() { return !!store.calendars; }, 1000, function() {
    fire('showConfiguration');
    waitFor(function() { return !!global.openedUrl; }, 1000, function() {
      sent = [];
      var response = encodeURIComponent(JSON.stringify({
        CFG_CALENDARS: { value: [true] }, CFG_MODE: { value: 'next' }, CFG_COUNT: { value: 3 },
        CFG_ALLDAY: { value: true }, CFG_HIDE_DECLINED: { value: true }, CFG_BRIEF: { value: true },
        CFG_BRIEF_TIME: { value: '06:30' }, CFG_BRIEF_DAYS: { value: [true, true, true, true, true, false, false] },
        CFG_LOGOUT: { value: false }
      }));
      fire('webviewclosed', { response: response });
      waitFor(function() { return sent.some(function(m) { return m.MSG_TYPE === 2; }); }, 2000, function() {
        var settingsMsg = sent.filter(function(m) { return m.MSG_TYPE === 5; })[0];
        assert.strictEqual(settingsMsg.SET_BRIEF_ENABLED, 1);
        assert.strictEqual(settingsMsg.SET_BRIEF_HOUR, 6);
        assert.strictEqual(settingsMsg.SET_BRIEF_MINUTE, 30);
        assert.strictEqual(settingsMsg.SET_BRIEF_DAYS, 62);
        assert.strictEqual(settingsMsg.SET_MODE, 1);
        assert.strictEqual(settingsMsg.SET_COUNT, 3);
        assert.ok(requests.some(function(r) { return /\/me\/calendars\/cal1\/calendarView$/.test(r); }),
                  'ausgewählter Kalender wird abgefragt');
        console.log('Simulation ok');
      });
    });
  });
});
