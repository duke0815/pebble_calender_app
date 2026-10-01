// PebbleKit JS: sign-in (device code flow), Microsoft Graph, transfer to the watch.
// No server of our own - tokens stay in the phone's localStorage.

var Clay = require('pebble-clay');
var messageKeys = require('message_keys');
var lib = require('./lib');
var i18n = require('./i18n');
var configPage = require('./config');

// --- Configuration ----------------------------------------------------------
// Client ID of the Entra app registration (see README.md).
var CLIENT_ID = '402716d7-8b06-443b-af50-c37348a787c4';
// 'common' = all account types; for personal use this can be your own tenant ID.
var AUTHORITY = 'common';

var SCOPE_READ = 'https://graph.microsoft.com/Calendars.Read offline_access';
// Only when "Move events" is enabled (incremental consent)
var SCOPE_RW = 'https://graph.microsoft.com/Calendars.ReadWrite offline_access';
var LOGIN_BASE = 'https://login.microsoftonline.com/' + AUTHORITY + '/oauth2/v2.0/';
var GRAPH = 'https://graph.microsoft.com/v1.0';
var EVENT_FIELDS = 'id,subject,bodyPreview,start,end,isAllDay,location,isCancelled,showAs,responseStatus,' +
                   'isOrganizer,organizer,attendees';
var NOTES_INDEX_MAX = 80;
var CALENDAR_MAX_AGE_MS = 24 * 60 * 60 * 1000;
var TOKEN_MARGIN_MS = 5 * 60 * 1000;

var MSG = lib.MSG;
var STATUS = lib.STATUS;

// --- localStorage -----------------------------------------------------------

function load(key, fallback) {
  try {
    var raw = localStorage.getItem(key);
    return raw ? JSON.parse(raw) : fallback;
  } catch (e) {
    return fallback;
  }
}

function save(key, value) {
  localStorage.setItem(key, JSON.stringify(value));
}

// Sets the language for status messages from the settings.
function applyLanguage(settings) {
  var phoneLang = (typeof navigator !== 'undefined' && navigator.language) || '';
  i18n.setLang(i18n.resolveLang(settings.language, phoneLang));
}

function getSettings() {
  var settings = lib.mergeSettings(load('settings', null));
  applyLanguage(settings);
  return settings;
}

function getAuth() {
  return load('auth', null);
}

function clearAuth() {
  localStorage.removeItem('auth');
  localStorage.removeItem('calendars');
}

function getCalendars() {
  return load('calendars', { list: [], fetched: 0 });
}

// --- AppMessage queue (send one at a time, ACK/NACK with retry) -------------

var queue = [];
var sending = false;

function enqueue(message) {
  queue.push({ message: message, tries: 0 });
  pump();
}

function pump() {
  if (sending || !queue.length) return;
  sending = true;
  var item = queue[0];
  Pebble.sendAppMessage(item.message, function() {
    queue.shift();
    sending = false;
    pump();
  }, function() {
    item.tries++;
    sending = false;
    if (item.tries >= 4) {
      console.log('Nachricht verworfen: ' + JSON.stringify(item.message));
      queue.shift();
    }
    setTimeout(pump, 500 * item.tries);
  });
}

function sendStatus(code, text) {
  enqueue({ MSG_TYPE: MSG.STATUS, STATUS_CODE: code, STATUS_TEXT: text || '' });
}

function sendSettings(settings) {
  applyLanguage(settings);
  var t = lib.parseTime(settings.briefTime);
  enqueue({
    MSG_TYPE: MSG.SETTINGS,
    SET_BRIEF_ENABLED: settings.briefEnabled ? 1 : 0,
    SET_BRIEF_HOUR: t.hour,
    SET_BRIEF_MINUTE: t.minute,
    SET_BRIEF_DAYS: settings.briefDays,
    SET_LANG: i18n.getLang() === 'en' ? 1 : 0,
    SET_TOUCH: settings.touch ? 1 : 0,
    SET_GRAY_PAST: settings.grayPast ? 1 : 0,
    SET_AUTO_CLOSE: lib.autoCloseMinutes(settings.autoClose),
    SET_MOVE_OPTS: settings.moveEnabled ? settings.moveOptions : 0,
    SET_COLOR_ACCENT: settings.accentColor,
    SET_COLOR_TEXT: settings.textColor,
    SET_COLOR_BG: settings.bgColor
  });
}

// listDay: epoch seconds of the first day, listDays: 2 = cache (today + tomorrow), 1 = single day
// --- Descriptions -----------------------------------------------------------
// Key -> {id, notes}. The watch only knows the key (EVT_KEY) and requests the description
// when opening the details (MSG_TYPE 12). Kept in localStorage so it survives a restart.

function rememberNotes(events) {
  var index = load('notes', {});
  events.forEach(function(e) {
    index[e.key] = {
      id: e.id, notes: e.notes, t: Date.now(),
      start: e.start, end: e.end, allDay: e.allDay, movable: !!e.movable
    };
  });
  var keys = Object.keys(index);
  if (keys.length > NOTES_INDEX_MAX) {
    keys.sort(function(a, b) { return index[a].t - index[b].t; });
    keys.slice(0, keys.length - NOTES_INDEX_MAX).forEach(function(k) { delete index[k]; });
  }
  save('notes', index);
}

function sendNote(key, text) {
  enqueue({ MSG_TYPE: MSG.NOTE, EVT_KEY: key, NOTE_TEXT: text || '' });
}

function handleNoteRequest(key) {
  var entry = load('notes', {})[key];
  if (!entry) {
    sendNote(key, '');
    return;
  }
  if (entry.notes !== null && entry.notes !== undefined) {
    sendNote(key, entry.notes);
    return;
  }
  // Description was not in the list -> fetch it individually
  getToken(function(err, token) {
    if (err || !entry.id) {
      sendNote(key, '');
      return;
    }
    graphGet(GRAPH + '/me/events/' + encodeURIComponent(entry.id) + '?$select=bodyPreview', token,
      function(status, json) {
        var notes = status === 200 && json ? lib.cleanNotes(json.bodyPreview) : '';
        sendNote(key, notes);
      });
  });
}

// --- Moving events ----------------------------------------------------------

function sendMoveResult(key, ok, text) {
  enqueue({ MSG_TYPE: MSG.MOVE_RESULT, EVT_KEY: key, MOVE_OK: ok ? 1 : 0, STATUS_TEXT: text || '' });
}

function handleMove(key, opt) {
  var settings = getSettings();
  var entry = load('notes', {})[key];
  // Check again - the watch may have stale data
  if (!entry || !entry.movable || !settings.moveEnabled || !(settings.moveOptions & (1 << opt))) {
    sendMoveResult(key, false, i18n.t('moveDenied'));
    return;
  }
  var body = lib.graphMoveBody(entry, opt);
  if (!body) {
    sendMoveResult(key, false, i18n.t('moveFailed'));
    return;
  }
  getToken(function(err, token) {
    if (err) {
      sendMoveResult(key, false, err.text);
      return;
    }
    request('PATCH', GRAPH + '/me/events/' + encodeURIComponent(entry.id), {
      'Authorization': 'Bearer ' + token,
      'Content-Type': 'application/json',
      'Prefer': 'outlook.timezone="UTC"'
    }, JSON.stringify(body), function(status) {
      if (status === 200) {
        sendMoveResult(key, true, '');
        sync(true);  // Reload the cache (today + tomorrow)
      } else if (status === 403) {
        sendMoveResult(key, false, i18n.t('moveDenied'));
      } else if (status === 0) {
        sendMoveResult(key, false, i18n.t('noConnection'));
      } else {
        sendMoveResult(key, false, i18n.t('moveFailed') + ' (' + status + ')');
      }
    });
  });
}

function sendEvents(events, listDay, listDays) {
  rememberNotes(events);
  events.forEach(function(e, i) {
    enqueue({
      MSG_TYPE: MSG.EVENT,
      EVT_INDEX: i,
      EVT_COUNT: events.length,
      EVT_START: e.start,
      EVT_END: e.end,
      EVT_ALLDAY: e.allDay ? 1 : 0,
      EVT_TITLE: e.title,
      EVT_LOCATION: e.location,
      EVT_KEY: e.key,
      EVT_FLAGS: e.flags || 0
    });
  });
  enqueue({ MSG_TYPE: MSG.END, EVT_COUNT: events.length, LIST_DAY: listDay, LIST_DAYS: listDays });
}

// --- HTTP -------------------------------------------------------------------

function request(method, url, headers, body, callback) {
  var xhr = new XMLHttpRequest();
  var finished = false;
  function finish(status, text) {
    if (finished) return;
    finished = true;
    var json = null;
    try {
      json = text ? JSON.parse(text) : null;
    } catch (e) {
      json = null;
    }
    callback(status, json);
  }
  xhr.open(method, url, true);
  for (var name in headers) {
    if (headers.hasOwnProperty(name)) xhr.setRequestHeader(name, headers[name]);
  }
  xhr.timeout = 20000;
  xhr.onload = function() { finish(xhr.status, xhr.responseText); };
  xhr.onerror = function() { finish(0, null); };
  xhr.ontimeout = function() { finish(0, null); };
  xhr.send(body || null);
}

function postForm(url, params, callback) {
  request('POST', url, { 'Content-Type': 'application/x-www-form-urlencoded' },
          lib.formEncode(params), callback);
}

// --- Sign-in ----------------------------------------------------------------

var login = null;      // device code login in progress
var tokenWaiters = []; // callbacks waiting for the login result

function storeTokens(json) {
  var previous = getAuth() || {};
  var auth = {
    access: json.access_token,
    refresh: json.refresh_token || previous.refresh,
    expires: Date.now() + (json.expires_in || 3600) * 1000,
    // Write access granted? (response contains the granted scopes)
    rw: json.scope ? /Calendars\.ReadWrite/i.test(json.scope) : !!previous.rw
  };
  save('auth', auth);
  return auth;
}

// callback(err, accessToken); err = {code, text}
function getToken(callback) {
  if (CLIENT_ID === 'TODO') {
    callback({ code: STATUS.CONFIG, text: i18n.t('noClientId') });
    return;
  }
  var auth = getAuth();
  if (auth && getSettings().moveEnabled && !auth.rw) {
    // Moving was enabled -> sign in again once to obtain write access
    clearAuth();
    auth = null;
  }
  if (auth && auth.access && auth.expires - Date.now() > TOKEN_MARGIN_MS) {
    callback(null, auth.access);
  } else if (auth && auth.refresh) {
    refreshToken(auth.refresh, callback);
  } else {
    startLogin(callback);
  }
}

function refreshToken(refresh, callback) {
  postForm(LOGIN_BASE + 'token', {
    client_id: CLIENT_ID,
    grant_type: 'refresh_token',
    refresh_token: refresh,
    scope: (getAuth() || {}).rw ? SCOPE_RW : SCOPE_READ
  }, function(status, json) {
    if (status === 200 && json && json.access_token) {
      callback(null, storeTokens(json).access);
      return;
    }
    if (status === 0) {
      callback({ code: STATUS.NETWORK, text: i18n.t('noConnection') });
      return;
    }
    var err = lib.classifyAuthError(json);
    if (err.clearTokens) clearAuth();
    callback(err);
  });
}

function sendLoginCode() {
  if (!login || !login.code) return;  // code is still being requested
  enqueue({ MSG_TYPE: MSG.LOGIN_CODE, LOGIN_URL: login.url, LOGIN_CODE: login.code });
}

function finishLogin(err, token) {
  if (login && login.timer) clearTimeout(login.timer);
  login = null;
  var waiters = tokenWaiters;
  tokenWaiters = [];
  waiters.forEach(function(cb) { cb(err, token); });
}

function startLogin(callback) {
  tokenWaiters.push(callback);
  if (login) {
    sendLoginCode();
    return;
  }
  login = { pending: true };
  postForm(LOGIN_BASE + 'devicecode', { client_id: CLIENT_ID, scope: getSettings().moveEnabled ? SCOPE_RW : SCOPE_READ }, function(status, json) {
    if (status !== 200 || !json || !json.device_code) {
      finishLogin(status === 0 ? { code: STATUS.NETWORK, text: i18n.t('noConnection') }
                               : lib.classifyAuthError(json));
      return;
    }
    login = {
      deviceCode: json.device_code,
      code: json.user_code,
      url: String(json.verification_uri || 'microsoft.com/devicelogin').replace(/^https?:\/\//, ''),
      interval: (json.interval || 5) * 1000,
      expiresAt: Date.now() + (json.expires_in || 900) * 1000,
      timer: null
    };
    sendLoginCode();
    schedulePoll();
  });
}

function schedulePoll() {
  login.timer = setTimeout(pollToken, login.interval);
}

function pollToken() {
  if (!login) return;
  if (Date.now() > login.expiresAt) {
    finishLogin({ code: STATUS.NEED_LOGIN, text: i18n.t('codeExpired') });
    return;
  }
  postForm(LOGIN_BASE + 'token', {
    client_id: CLIENT_ID,
    grant_type: 'urn:ietf:params:oauth:grant-type:device_code',
    device_code: login.deviceCode
  }, function(status, json) {
    if (!login) return;
    if (status === 200 && json && json.access_token) {
      finishLogin(null, storeTokens(json).access);
      return;
    }
    var error = json && json.error;
    if (status === 0 || error === 'authorization_pending') {
      schedulePoll();
      return;
    }
    if (error === 'slow_down') {
      login.interval += 5000;
      schedulePoll();
      return;
    }
    finishLogin(lib.classifyAuthError(json));
  });
}

// --- Microsoft Graph --------------------------------------------------------

function graphGet(url, token, callback) {
  request('GET', url, {
    'Authorization': 'Bearer ' + token,
    'Prefer': 'outlook.timezone="UTC"'
  }, null, callback);
}

// Fetch all pages (@odata.nextLink). callback(httpStatus, items)
function graphGetAll(url, token, items, callback) {
  graphGet(url, token, function(status, json) {
    if (status !== 200 || !json) {
      callback(status, items);
      return;
    }
    items = items.concat(json.value || []);
    if (json['@odata.nextLink']) {
      graphGetAll(json['@odata.nextLink'], token, items, callback);
    } else {
      callback(200, items);
    }
  });
}

function fetchCalendars(token, callback) {
  graphGetAll(GRAPH + '/me/calendars?$select=id,name,isDefaultCalendar&$top=100', token, [],
    function(status, items) {
      if (status === 200) {
        save('calendars', {
          list: items.map(function(c) {
            return { id: c.id, name: c.name, isDefaultCalendar: !!c.isDefaultCalendar };
          }),
          fetched: Date.now()
        });
      }
      if (callback) callback(status);
    });
}

function fetchEvents(token, settings, win, callback) {
  var query = '/calendarView?startDateTime=' + encodeURIComponent(win.start.toISOString()) +
              '&endDateTime=' + encodeURIComponent(win.end.toISOString()) +
              '&$select=' + EVENT_FIELDS + '&$orderby=start/dateTime&$top=50';
  var bases = settings.calendars && settings.calendars.length
    ? settings.calendars.map(function(id) { return GRAPH + '/me/calendars/' + encodeURIComponent(id); })
    : [GRAPH + '/me/calendar'];

  var all = [];
  var i = 0;
  var retried = false;

  function next() {
    if (i >= bases.length) {
      callback(null, lib.selectEvents(all.map(lib.normalizeEvent), settings, win));
      return;
    }
    graphGetAll(bases[i] + query, token, [], function(status, items) {
      if (status === 401 && !retried) {
        // Token rejected -> refresh once and retry
        retried = true;
        var auth = getAuth();
        if (auth) {
          auth.expires = 0;
          save('auth', auth);
        }
        getToken(function(err, fresh) {
          if (err) {
            callback(err);
            return;
          }
          token = fresh;
          next();
        });
        return;
      }
      if (status === 404 && bases.length > 1) {
        // Selected calendar no longer exists -> skip it
        i++;
        next();
        return;
      }
      if (status !== 200) {
        callback(status === 401 ? { code: STATUS.NEED_LOGIN, text: i18n.t('reopenLogin') }
                                : lib.graphErrorStatus(status));
        return;
      }
      all = all.concat(items);
      i++;
      next();
    });
  }
  next();
}

// --- Synchronization --------------------------------------------------------
// Jobs are processed one after another: the cache (today + tomorrow) on app start
// and single days when paging on the watch (only the most recently requested day counts).

var busy = false;
var busyIsCache = false;
var pendingCache = false;
var pendingDay = null;   // epoch seconds (local midnight, as sent by the watch)

// force: reload afterwards even if a sync is running (changed settings)
function sync(force) {
  if (busy && busyIsCache && !force) {
    // Already running -> merge (exactly one sync per start)
    if (login) sendLoginCode();
    return;
  }
  pendingCache = true;
  run();
}

function requestDay(day) {
  pendingDay = day;
  run();
}

function run() {
  if (busy) return;
  var isCache;
  var listDay;
  var win;
  if (pendingCache) {
    pendingCache = false;
    isCache = true;
    win = lib.queryWindow(new Date(), lib.CACHE_DAYS);
    listDay = Math.floor(win.start.getTime() / 1000);
  } else if (pendingDay !== null) {
    isCache = false;
    listDay = pendingDay;
    pendingDay = null;
    win = lib.queryWindow(new Date(listDay * 1000), 1);
  } else {
    return;
  }
  busy = true;
  busyIsCache = isCache;
  var settings = getSettings();
  if (isCache) {
    sendSettings(settings);
    sendStatus(STATUS.SYNCING, i18n.t('syncing'));
  }

  function done(err) {
    busy = false;
    if (err) sendStatus(err.code, err.text);
    run();
  }

  getToken(function(err, token) {
    if (err) {
      done(err);
      return;
    }
    fetchEvents(token, settings, win, function(fetchErr, events) {
      if (fetchErr) {
        done(fetchErr);
        return;
      }
      events.forEach(function(ev) {
        ev.movable = lib.isMovable(ev, settings);
        ev.flags = lib.moveFlags(ev, settings);
      });
      sendEvents(events, listDay, isCache ? lib.CACHE_DAYS : 1);
      done(null);
      if (isCache && Date.now() - getCalendars().fetched > CALENDAR_MAX_AGE_MS) {
        fetchCalendars(token);
      }
    });
  });
}

// --- Settings (Clay) --------------------------------------------------------

var activeClay = null;
var configCalendars = [];

function openConfig() {
  configCalendars = getCalendars().list;
  localStorage.removeItem('clay-settings');
  activeClay = new Clay(configPage.buildConfig(configCalendars, getSettings(), !!getAuth()), null,
                        { autoHandleEvents: false });
  // Clay only fills meta on the "ready" event. Set it ourselves for an instance created later,
  // otherwise the color picker only offers black/white.
  try {
    activeClay.meta.activeWatchInfo = Pebble.getActiveWatchInfo ? Pebble.getActiveWatchInfo() : null;
    activeClay.meta.accountToken = Pebble.getAccountToken ? Pebble.getAccountToken() : '';
    activeClay.meta.watchToken = Pebble.getWatchToken ? Pebble.getWatchToken() : '';
  } catch (e) {
    console.log('Uhr-Infos nicht verfügbar: ' + e);
  }
  Pebble.openURL(activeClay.generateUrl());
}

Pebble.addEventListener('showConfiguration', function() {
  var auth = getAuth();
  if (CLIENT_ID === 'TODO' || !auth || !auth.refresh) {
    openConfig();
    return;
  }
  // Refresh the calendar list first, but wait at most 4 s
  var opened = false;
  var timer = setTimeout(function() {
    opened = true;
    openConfig();
  }, 4000);
  getToken(function(err, token) {
    if (err) {
      if (!opened) {
        clearTimeout(timer);
        opened = true;
        openConfig();
      }
      return;
    }
    fetchCalendars(token, function() {
      if (!opened) {
        clearTimeout(timer);
        opened = true;
        openConfig();
      }
    });
  });
});

Pebble.addEventListener('webviewclosed', function(e) {
  if (!e || !e.response || !activeClay) return;
  var result;
  try {
    result = activeClay.getSettings(e.response, false);
  } catch (err) {
    console.log('Ungültige Clay-Antwort: ' + err);
    return;
  }
  localStorage.removeItem('clay-settings');

  var applied = configPage.applyResult(result, configCalendars, getSettings());
  save('settings', applied.settings);

  if (applied.logout) {
    if (login) finishLogin({ code: STATUS.NEED_LOGIN, text: i18n.t('loggedOutShort') });
    clearAuth();
    sendSettings(applied.settings);
    sendEvents([], Math.floor(lib.queryWindow(new Date(), 1).start.getTime() / 1000), lib.CACHE_DAYS);
    sendStatus(STATUS.NEED_LOGIN, i18n.t('loggedOut'));
    return;
  }
  sync(true);
});

// --- Messages from the watch ------------------------------------------------

function payloadValue(payload, name) {
  if (payload.hasOwnProperty(name)) return payload[name];
  var key = messageKeys[name];
  return payload.hasOwnProperty(key) ? payload[key] : undefined;
}

Pebble.addEventListener('appmessage', function(e) {
  var type = payloadValue(e.payload || {}, 'MSG_TYPE');
  // REQ_DAY = 0: reload cache (briefing while app is open), otherwise single day (paging)
  if (type === MSG.REQUEST) {
    var day = payloadValue(e.payload || {}, 'REQ_DAY');
    if (day) {
      requestDay(day);
    } else {
      sync();
    }
  } else if (type === MSG.NOTE_REQUEST) {
    handleNoteRequest(payloadValue(e.payload || {}, 'EVT_KEY'));
  } else if (type === MSG.MOVE) {
    handleMove(payloadValue(e.payload || {}, 'EVT_KEY'), payloadValue(e.payload || {}, 'MOVE_OPT'));
  }
});

// Exactly one sync per app start.
Pebble.addEventListener('ready', function() {
  sync();
});
