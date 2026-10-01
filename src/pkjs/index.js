// PebbleKit JS: Anmeldung (Device Code Flow), Microsoft Graph, Übertragung an die Uhr.
// Kein eigener Server – Tokens bleiben im localStorage des Handys.

var Clay = require('pebble-clay');
var messageKeys = require('message_keys');
var lib = require('./lib');
var i18n = require('./i18n');
var configPage = require('./config');

// --- Konfiguration ----------------------------------------------------------
// Client-ID der Entra-App-Registrierung (siehe README.md).
var CLIENT_ID = '402716d7-8b06-443b-af50-c37348a787c4';
// 'common' = alle Kontotypen; für Eigengebrauch kann hier die eigene Tenant-ID stehen.
var AUTHORITY = 'common';

var SCOPE = 'https://graph.microsoft.com/Calendars.Read offline_access';
var LOGIN_BASE = 'https://login.microsoftonline.com/' + AUTHORITY + '/oauth2/v2.0/';
var GRAPH = 'https://graph.microsoft.com/v1.0';
var EVENT_FIELDS = 'subject,start,end,isAllDay,location,isCancelled,showAs,responseStatus';
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

// Liefert die Settings und stellt die Sprache für Statusmeldungen ein.
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

// --- AppMessage-Warteschlange (einzeln senden, ACK/NACK mit Retry) ----------

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
    SET_COLOR_ACCENT: settings.accentColor,
    SET_COLOR_TEXT: settings.textColor,
    SET_COLOR_BG: settings.bgColor
  });
}

// listDay: Epoch-Sekunden des ersten Tages, listDays: 2 = Cache (heute + morgen), 1 = ein Tag
function sendEvents(events, listDay, listDays) {
  events.forEach(function(e, i) {
    enqueue({
      MSG_TYPE: MSG.EVENT,
      EVT_INDEX: i,
      EVT_COUNT: events.length,
      EVT_START: e.start,
      EVT_END: e.end,
      EVT_ALLDAY: e.allDay ? 1 : 0,
      EVT_TITLE: e.title,
      EVT_LOCATION: e.location
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

// --- Anmeldung --------------------------------------------------------------

var login = null;      // laufender Device-Code-Login
var tokenWaiters = []; // Callbacks, die auf das Login-Ergebnis warten

function storeTokens(json) {
  var previous = getAuth() || {};
  var auth = {
    access: json.access_token,
    refresh: json.refresh_token || previous.refresh,
    expires: Date.now() + (json.expires_in || 3600) * 1000
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
    scope: SCOPE
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
  if (!login || !login.code) return;  // Code wird noch angefordert
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
  postForm(LOGIN_BASE + 'devicecode', { client_id: CLIENT_ID, scope: SCOPE }, function(status, json) {
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

// Alle Seiten (@odata.nextLink) abrufen. callback(httpStatus, items)
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
        // Token abgelehnt -> einmal erneuern und wiederholen
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
        // Ausgewählter Kalender existiert nicht mehr -> überspringen
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

// --- Synchronisierung -------------------------------------------------------
// Aufträge werden nacheinander abgearbeitet: der Cache (heute + morgen) beim App-Start
// und einzelne Tage beim Blättern auf der Uhr (nur der zuletzt angeforderte Tag zählt).

var busy = false;
var busyIsCache = false;
var pendingCache = false;
var pendingDay = null;   // Epoch-Sekunden (lokale Mitternacht, wie von der Uhr geschickt)

// force: auch bei laufender Synchronisierung danach erneut laden (geänderte Einstellungen)
function sync(force) {
  if (busy && busyIsCache && !force) {
    // Läuft bereits -> zusammenfassen (genau eine Synchronisierung pro Start)
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
      sendEvents(events, listDay, isCache ? lib.CACHE_DAYS : 1);
      done(null);
      if (isCache && Date.now() - getCalendars().fetched > CALENDAR_MAX_AGE_MS) {
        fetchCalendars(token);
      }
    });
  });
}

// --- Einstellungen (Clay) ---------------------------------------------------

var activeClay = null;
var configCalendars = [];

function openConfig() {
  configCalendars = getCalendars().list;
  localStorage.removeItem('clay-settings');
  activeClay = new Clay(configPage.buildConfig(configCalendars, getSettings(), !!getAuth()), null,
                        { autoHandleEvents: false });
  // Clay füllt meta nur beim "ready"-Event. Bei einer später erzeugten Instanz selbst setzen,
  // sonst bietet der Farbwähler nur Schwarz/Weiß an.
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
  // Kalenderliste vorher aktualisieren, aber höchstens 4 s warten
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

// --- Nachrichten von der Uhr ------------------------------------------------

function payloadValue(payload, name) {
  if (payload.hasOwnProperty(name)) return payload[name];
  var key = messageKeys[name];
  return payload.hasOwnProperty(key) ? payload[key] : undefined;
}

Pebble.addEventListener('appmessage', function(e) {
  var type = payloadValue(e.payload || {}, 'MSG_TYPE');
  // REQ_DAY = 0: Cache neu laden (Briefing bei offener App), sonst einzelner Tag (Blättern)
  if (type === MSG.REQUEST) {
    var day = payloadValue(e.payload || {}, 'REQ_DAY');
    if (day) {
      requestDay(day);
    } else {
      sync();
    }
  }
});

// Genau eine Synchronisierung pro App-Start.
Pebble.addEventListener('ready', function() {
  sync();
});
