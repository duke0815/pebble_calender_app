// Simulation of src/pkjs/index.js with mocked Pebble, XMLHttpRequest and localStorage.
// Run: TZ=Europe/Vienna node test/index.sim.js
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
      // Reject the first event message once -> retry must kick in
      if (msg.MSG_TYPE === 1 && nackOnce) { nackOnce = false; fail({}); return; }
      sent.push(msg);
      ok({});
    }, 1);
  },
  openURL: function(url) { global.openedUrl = url; },
  getActiveWatchInfo: function() { return { platform: 'emery', firmware: { major: 4, minor: 9 } }; }
};
function fire(name, e) { (listeners[name] || []).forEach(function(fn) { fn(e || {}); }); }

// Mock events lie on today's local date (the sync only fetches today + tomorrow)
function localDay(offset) {
  var d = new Date();
  d.setDate(d.getDate() + (offset || 0));
  return d.getFullYear() + '-' + (d.getMonth() < 9 ? '0' : '') + (d.getMonth() + 1) + '-' +
         (d.getDate() < 10 ? '0' : '') + d.getDate();
}
var TODAY = localDay(0);
var TOMORROW = localDay(1);

var tokenPolls = 0;
var requests = [];
var lastScope = '';
var patches = [];
var posts = [];
var withInvite = false;
function respond(method, url, body) {
  requests.push(method + ' ' + url.split('?')[0]);
  if (method === 'POST' && /graph\.microsoft\.com/.test(url)) {
    posts.push({ url: url, body: JSON.parse(body) });
    return [202, null];
  }
  if (method === 'PATCH') {
    patches.push({ url: url, body: JSON.parse(body) });
    return [200, { id: 'evt-standup' }];
  }
  if (/devicecode$/.test(url)) {
    lastScope = decodeURIComponent(/scope=([^&]*)/.exec(body)[1]);
    return [200, { device_code: 'DEV', user_code: 'ABCD-1234', verification_uri: 'https://microsoft.com/devicelogin',
                   interval: 0.01, expires_in: 900 }];
  }
  if (/token$/.test(url)) {
    if (/device_code/.test(body)) {
      tokenPolls++;
      if (tokenPolls < 3) return [400, { error: 'authorization_pending' }];
      return [200, { access_token: 'AT', refresh_token: 'RT', expires_in: 3600, scope: lastScope }];
    }
    return [200, { access_token: 'AT2', expires_in: 3600 }];
  }
  if (/\/me\/calendars?(\/cal1)?\/calendarView/.test(url)) {
    return [200, {
      value: [{ id: 'evt-standup', bodyPreview: 'Bitte Status vorbereiten.', subject: 'Standup', isAllDay: false,
                isOrganizer: true, attendees: [], start: { dateTime: TODAY + 'T07:00:00.0000000' },
                end: { dateTime: TODAY + 'T07:15:00.0000000' }, location: { displayName: 'Teams' } }],
      '@odata.nextLink': 'https://graph.microsoft.com/v1.0/page2'
    }];
  }
  if (/page2$/.test(url)) {
    var invite = withInvite ? [{ id: 'evt-invite', subject: 'Einladung', isOrganizer: false, attendees: [{}],
      responseStatus: { response: 'notResponded' },
      start: { dateTime: TODAY + 'T12:00:00' }, end: { dateTime: TODAY + 'T13:00:00' } }] : [];
    return [200, { value: invite.concat([
      { subject: 'Abgesagt', isCancelled: true, start: { dateTime: TODAY + 'T09:00:00' }, end: { dateTime: TODAY + 'T10:00:00' } },
      { subject: 'Feiertag', isAllDay: true, start: { dateTime: TODAY + 'T00:00:00' }, end: { dateTime: TOMORROW + 'T00:00:00' } }
    ]) }];
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
      assert.ok(/^Bearer /.test(self.headers.Authorization), 'Authorization missing');
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
  if (request === '@rebble/clay') {
    var Clay = function(config) { this.config = config; this.meta = { activeWatchInfo: null }; };
    Clay.prototype.generateUrl = function() { return 'data:clay'; };
    Clay.prototype.getSettings = function(r) { return JSON.parse(decodeURIComponent(r)); };
    return Clay;
  }
  if (request === 'message_keys') return { MSG_TYPE: 10000 };
  return origLoad.apply(this, arguments);
};

// Load index.js with a test client ID
var file = path.join(__dirname, '../src/pkjs/index.js');
var src = fs.readFileSync(file, 'utf8').replace(/var CLIENT_ID = '[^']*';/, "var CLIENT_ID = 'test-client';");
var m = new Module(file, module);
m.filename = file;
m.paths = Module._nodeModulePaths(path.dirname(file));
m._compile(src, file);

// --- Flow ------------------------------------------------------------------
function waitFor(cond, ms, cb) {
  var start = Date.now();
  (function check() {
    if (cond()) return cb();
    if (Date.now() - start > ms) { console.log(JSON.stringify(sent), requests); throw new Error('Timeout while waiting'); }
    setTimeout(check, 5);
  })();
}

fire('ready');
// Duplicate request from the watch during a running sync -> only resend the login code
fire('appmessage', { payload: { MSG_TYPE: 10 } });

waitFor(function() { return sent.some(function(m) { return m.MSG_TYPE === 2; }); }, 3000, function() {
  var types = sent.map(function(m) { return m.MSG_TYPE; });
  assert.strictEqual(types[0], 5, 'settings first');
  assert.strictEqual(types[1], 3, 'then status');
  var login = sent.filter(function(m) { return m.MSG_TYPE === 4; });
  assert.ok(login.length >= 1);
  assert.strictEqual(login[0].LOGIN_URL, 'microsoft.com/devicelogin');
  assert.strictEqual(login[0].LOGIN_CODE, 'ABCD-1234');

  var events = sent.filter(function(m) { return m.MSG_TYPE === 1; });
  assert.deepStrictEqual(events.map(function(e) { return e.EVT_TITLE; }), ['Feiertag', 'Standup']);
  assert.strictEqual(events[0].EVT_ALLDAY, 1);
  var now = new Date();
  assert.strictEqual(events[0].EVT_START, Math.floor(new Date(now.getFullYear(), now.getMonth(), now.getDate()).getTime() / 1000));
  assert.strictEqual(events[1].EVT_START, Date.parse(TODAY + 'T07:00:00Z') / 1000);
  assert.strictEqual(events[1].EVT_LOCATION, 'Teams');
  var end = sent.filter(function(m) { return m.MSG_TYPE === 2; })[0];
  assert.strictEqual(end.EVT_COUNT, 2);
  assert.strictEqual(end.LIST_DAYS, 2);
  var todayStart = Math.floor(new Date(new Date().getFullYear(), new Date().getMonth(), new Date().getDate()).getTime() / 1000);
  assert.strictEqual(end.LIST_DAY, todayStart);

  var auth = JSON.parse(store.auth);
  assert.strictEqual(auth.refresh, 'RT');
  assert.strictEqual(requests.filter(function(r) { return /devicecode$/.test(r); }).length, 1, 'only one login');

  // Save settings -> new sync with settings sent to the watch
  waitFor(function() { return !!store.calendars; }, 1000, function() {
    fire('showConfiguration');
    waitFor(function() { return !!global.openedUrl; }, 1000, function() {
      sent = [];
      var response = encodeURIComponent(JSON.stringify({
        CFG_CALENDARS: { value: [true] },
        CFG_ALLDAY: { value: true }, CFG_HIDE_DECLINED: { value: true }, CFG_BRIEF: { value: true },
        CFG_BRIEF_TIME: { value: '06:30' }, CFG_BRIEF_DAYS: { value: [true, true, true, true, true, false, false] },
        CFG_LOGOUT: { value: false },
        CFG_LANG: { value: 'en' }, CFG_COLOR_ACCENT: { value: 0xAA0000 }
      }));
      fire('webviewclosed', { response: response });
      waitFor(function() { return sent.some(function(m) { return m.MSG_TYPE === 2; }); }, 2000, function() {
        var settingsMsg = sent.filter(function(m) { return m.MSG_TYPE === 5; })[0];
        assert.strictEqual(settingsMsg.SET_BRIEF_ENABLED, 1);
        assert.strictEqual(settingsMsg.SET_BRIEF_HOUR, 6);
        assert.strictEqual(settingsMsg.SET_BRIEF_MINUTE, 30);
        assert.strictEqual(settingsMsg.SET_BRIEF_DAYS, 62);
        assert.strictEqual(settingsMsg.SET_MODE, undefined);
        assert.strictEqual(settingsMsg.SET_LANG, 1);
        assert.strictEqual(settingsMsg.SET_TOUCH, 1);
        assert.strictEqual(settingsMsg.SET_AUTO_CLOSE, 5);
        assert.strictEqual(settingsMsg.SET_GRAY_PAST, 0);
        assert.strictEqual(settingsMsg.SET_COLOR_ACCENT, 0xAA0000);
        assert.strictEqual(settingsMsg.SET_COLOR_BG, 0xFFFFFF);
        var statusMsg = sent.filter(function(m) { return m.MSG_TYPE === 3; })[0];
        assert.strictEqual(statusMsg.STATUS_TEXT, 'Updating...');
        assert.ok(requests.some(function(r) { return /\/me\/calendars\/cal1\/calendarView$/.test(r); }),
                  'selected calendar is queried');
        // Paging: request two days in quick succession
        sent = [];
        var d1 = todayStart + 2 * 86400;
        var d2 = todayStart - 86400;
        fire('appmessage', { payload: { MSG_TYPE: 10, REQ_DAY: d1 } });
        fire('appmessage', { payload: { MSG_TYPE: 10, REQ_DAY: d2 } });
        waitFor(function() { return sent.filter(function(m) { return m.MSG_TYPE === 2; }).length >= 2; }, 2000, function() {
          var ends = sent.filter(function(m) { return m.MSG_TYPE === 2; });
          assert.strictEqual(ends[0].LIST_DAY, d1);
          assert.strictEqual(ends[0].LIST_DAYS, 1);
          assert.strictEqual(ends[1].LIST_DAY, d2);
          assert.ok(!sent.some(function(m) { return m.MSG_TYPE === 3; }), 'no status when paging');
          // Request description: known key -> text, unknown -> empty
          sent = [];
          var key = events[1].EVT_KEY;
          fire('appmessage', { payload: { MSG_TYPE: 12, EVT_KEY: key } });
          fire('appmessage', { payload: { MSG_TYPE: 12, EVT_KEY: 4711 } });
          waitFor(function() { return sent.filter(function(m) { return m.MSG_TYPE === 6; }).length >= 2; }, 2000, function() {
            var notes = sent.filter(function(m) { return m.MSG_TYPE === 6; });
            assert.strictEqual(notes[0].EVT_KEY, key);
            assert.strictEqual(notes[0].NOTE_TEXT, 'Bitte Status vorbereiten.');
            assert.strictEqual(notes[1].EVT_KEY, 4711);
            assert.strictEqual(notes[1].NOTE_TEXT, '');
            assert.ok(/Calendars\.Read /.test(lastScope + ' ') && !/ReadWrite/.test(lastScope), 'read-only access without moving');

            // Enable moving -> one-time re-login with write access
            sent = [];
            var polls = tokenPolls;
            fire('showConfiguration');
            waitFor(function() { return !!global.openedUrl; }, 1000, function() {
              fire('webviewclosed', { response: encodeURIComponent(JSON.stringify({
                CFG_MOVE: { value: true }, CFG_MOVE_SCOPE: { value: 'solo' },
                CFG_MOVE_OPTS: { value: [false, true, false, true] }
              })) });
              waitFor(function() { return sent.some(function(m) { return m.MSG_TYPE === 2; }); }, 3000, function() {
                assert.ok(/Calendars\.ReadWrite/.test(lastScope), 'login with write access');
                assert.ok(tokenPolls > polls, 'new sign-in');
                assert.ok(sent.some(function(m) { return m.MSG_TYPE === 4; }), 'login code sent to the watch');
                var s5 = sent.filter(function(m) { return m.MSG_TYPE === 5; }).pop();
                assert.strictEqual(s5.SET_MOVE_OPTS, 10);
                var ev = sent.filter(function(m) { return m.MSG_TYPE === 1 && m.EVT_TITLE === 'Standup'; })[0];
                assert.strictEqual(ev.EVT_FLAGS & 1, 1, 'own event without attendees is movable');
                assert.strictEqual(ev.EVT_FLAGS & 8, 8, 'moving enabled on the phone');

                // 1 day later -> PATCH; 1 hour later (not allowed) -> denied
                sent = [];
                fire('appmessage', { payload: { MSG_TYPE: 13, EVT_KEY: ev.EVT_KEY, MOVE_OPT: 1 } });
                fire('appmessage', { payload: { MSG_TYPE: 13, EVT_KEY: ev.EVT_KEY, MOVE_OPT: 0 } });
                waitFor(function() { return sent.filter(function(m) { return m.MSG_TYPE === 7; }).length >= 2; }, 2000, function() {
                  var results = sent.filter(function(m) { return m.MSG_TYPE === 7; });
                  var denied = results.filter(function(r) { return !r.MOVE_OK; })[0];
                  var okRes = results.filter(function(r) { return r.MOVE_OK; })[0];
                  assert.ok(denied && okRes, 'one success, one denial');
                  assert.strictEqual(patches.length, 1);
                  assert.ok(/\/me\/events\/evt-standup$/.test(patches[0].url));
                  // +1 day keeps the local time of day
                  function nextDayUtc(iso) {
                    var d = new Date(iso);
                    d.setDate(d.getDate() + 1);
                    return d.toISOString().slice(0, 19);
                  }
                  assert.deepStrictEqual(patches[0].body, {
                    start: { dateTime: nextDayUtc(TODAY + 'T07:00:00Z'), timeZone: 'UTC' },
                    end: { dateTime: nextDayUtc(TODAY + 'T07:15:00Z'), timeZone: 'UTC' }
                  });
                  assert.strictEqual(ev.EVT_FLAGS & 16, 0, 'own event is not respondable');

                  // Enable responding (write access already there -> no new sign-in)
                  withInvite = true;
                  var polls2 = tokenPolls;
                  global.openedUrl = null;
                  fire('showConfiguration');
                  waitFor(function() { return !!global.openedUrl; }, 1000, function() {
                    sent = [];
                    fire('webviewclosed', { response: encodeURIComponent(JSON.stringify({
                      CFG_MOVE: { value: true }, CFG_RESPOND: { value: true }
                    })) });
                    waitFor(function() { return sent.some(function(m) { return m.MSG_TYPE === 2; }); }, 3000, function() {
                      assert.strictEqual(tokenPolls, polls2, 'no new sign-in');
                      var inv = sent.filter(function(m) { return m.MSG_TYPE === 1 && m.EVT_TITLE === 'Einladung'; })[0];
                      assert.ok(inv, 'invitation sent');
                      assert.strictEqual(inv.EVT_FLAGS & 16, 16, 'invitation is respondable');
                      assert.strictEqual((inv.EVT_FLAGS >> 5) & 3, 0, 'not responded yet');
                      assert.strictEqual(inv.EVT_FLAGS & 1, 0, 'invitation is not movable');
                      // Decline the invitation; responding to an own event is denied
                      sent = [];
                      fire('appmessage', { payload: { MSG_TYPE: 14, EVT_KEY: inv.EVT_KEY, RESP_OPT: 2 } });
                      fire('appmessage', { payload: { MSG_TYPE: 14, EVT_KEY: ev.EVT_KEY, RESP_OPT: 0 } });
                      waitFor(function() { return sent.filter(function(m) { return m.MSG_TYPE === 7; }).length >= 2; }, 2000, function() {
                        var res = sent.filter(function(m) { return m.MSG_TYPE === 7; });
                        var okR = res.filter(function(r) { return r.MOVE_OK; })[0];
                        var deniedR = res.filter(function(r) { return !r.MOVE_OK; })[0];
                        assert.strictEqual(okR.EVT_KEY, inv.EVT_KEY);
                        assert.strictEqual(deniedR.EVT_KEY, ev.EVT_KEY);
                        assert.strictEqual(posts.length, 1);
                        assert.ok(/\/me\/events\/evt-invite\/decline$/.test(posts[0].url));
                        assert.deepStrictEqual(posts[0].body, { sendResponse: true });
                        console.log('Simulation ok');
                      });
                    });
                  });
                });
              });
            });
          });
        });
      });
    });
  });
});
