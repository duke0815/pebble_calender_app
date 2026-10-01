// Demo mode: a fake Microsoft Graph with neutral English sample events.
// Used for store screenshots (index.js: DEMO = true, see tools/screenshots.sh).
// No sign-in, no network. Moves are kept in memory until the app is closed.

var ME = { emailAddress: { name: 'Me', address: 'me@example.com' } };
var CALENDAR_ID = 'demo-calendar';
var moved = {};  // event id -> {start, end} (Graph dateTime strings)

function person(name) {
  return { emailAddress: { name: name, address: name.toLowerCase().replace(/[^a-z]/g, '') + '@example.com' } };
}

function attendees(names) {
  return names.map(function(name) {
    var a = person(name);
    a.type = 'required';
    a.status = { response: 'accepted' };
    return a;
  });
}

function pad2(n) {
  return (n < 10 ? '0' : '') + n;
}

// Local time on day `day` (local midnight) -> Graph UTC dateTime without "Z"
function utc(day, hour, minute) {
  var d = new Date(day.getFullYear(), day.getMonth(), day.getDate(), hour, minute);
  return d.toISOString().slice(0, 19) + '.0000000';
}

function dateOnly(day, plusDays) {
  var d = new Date(day.getFullYear(), day.getMonth(), day.getDate() + (plusDays || 0));
  return d.getFullYear() + '-' + pad2(d.getMonth() + 1) + '-' + pad2(d.getDate()) + 'T00:00:00.0000000';
}

// t = [subject, start "HH:MM" or null (all-day), end "HH:MM", location, extra]
// extra: {organizer: name (someone else), attendees: [names], notes: text}
function build(day, index, t) {
  var x = t[4] || {};
  var id = 'demo-' + dateOnly(day).slice(0, 10) + '-' + index;
  var allDay = !t[1];
  var start;
  var end;
  if (allDay) {
    start = dateOnly(day);
    end = dateOnly(day, 1);
  } else {
    start = utc(day, +t[1].slice(0, 2), +t[1].slice(3));
    end = utc(day, +t[2].slice(0, 2), +t[2].slice(3));
  }
  if (moved[id]) {
    start = moved[id].start;
    end = moved[id].end;
  }
  var mine = !x.organizer;
  return {
    id: id,
    subject: t[0],
    bodyPreview: x.notes || '',
    start: { dateTime: start, timeZone: 'UTC' },
    end: { dateTime: end, timeZone: 'UTC' },
    isAllDay: allDay,
    location: { displayName: t[3] || '' },
    isCancelled: false,
    showAs: 'busy',
    responseStatus: { response: mine ? 'organizer' : 'accepted' },
    isOrganizer: mine,
    organizer: mine ? ME : person(x.organizer),
    attendees: attendees(x.attendees || [])
  };
}

var TEAMS = '\r\n________________________________________________________________________________\r\n' +
            'Microsoft Teams meeting\r\nJoin on your computer, mobile app or room device';

var TODAY = [
  ['Project deadline', null, null, '', {}],
  ['Team stand-up', '08:30', '08:45', 'Microsoft Teams', { organizer: 'Sam Taylor',
    attendees: ['Me', 'Chris Lee', 'Dana Fox'], notes: 'Yesterday, today, blockers. Keep it short.' + TEAMS }],
  ['Product review', '10:00', '11:30', 'Conference Room A', { attendees: ['Chris Lee', 'Dana Fox', 'Jordan Kim'],
    notes: 'Agenda:\r\n- Q4 roadmap\r\n- Open issues\r\n- Next steps\r\n\r\nPlease bring the latest numbers.' }],
  ['Lunch with Alex', '12:30', '13:30', 'Corner Bistro', { notes: 'Table for two booked.' }],
  ['Dentist appointment', '15:00', '15:30', 'Main Street 12', { notes: 'Bring insurance card.' }],
  ['Yoga class', '17:30', '18:30', 'City Gym', {}]
];

var TOMORROW = [
  ['Weekly planning', '09:00', '10:00', 'Room 3.01', { attendees: ['Chris Lee', 'Dana Fox'],
    notes: 'Review last week, set priorities for this week.' }],
  ['Supplier call', '11:00', '11:45', 'Microsoft Teams', { organizer: 'Robin Gray', attendees: ['Me'],
    notes: 'Delivery dates and pricing for next quarter.' + TEAMS }],
  ['Design workshop', '14:00', '16:00', 'Studio B', { organizer: 'Jordan Kim', attendees: ['Me', 'Dana Fox'] }],
  ['Dinner with friends', '19:00', '21:00', "Luigi's Trattoria", {}]
];

var WEEKDAY_POOL = [
  ['Focus time', '09:00', '11:00', '', {}],
  ['1:1 with manager', '11:30', '12:00', 'Room 2.14', { organizer: 'Sam Taylor', attendees: ['Me'] }],
  ['Budget review', '13:00', '14:00', 'Conference Room B', { attendees: ['Robin Gray'] }],
  ['Customer call', '14:30', '15:00', 'Microsoft Teams', { organizer: 'Chris Lee', attendees: ['Me'] }],
  ['Interview', '10:00', '11:00', 'Room 1.05', { attendees: ['Dana Fox'] }],
  ['Gym', '18:00', '19:00', 'City Gym', {}]
];

var WEEKEND = [
  ['Farmers market', '09:30', '10:30', 'Town Square', {}],
  ['Brunch with family', '11:00', '12:30', 'Garden Cafe', {}]
];

function templatesFor(day, offset) {
  if (offset === 0) return TODAY;
  if (offset === 1) return TOMORROW;
  var wday = day.getDay();
  if (wday === 0 || wday === 6) return WEEKEND;
  var n = ((offset % 3) + 3) % 3;
  return [WEEKDAY_POOL[n], WEEKDAY_POOL[n + 2], WEEKDAY_POOL[5 - n]].sort(function(a, b) {
    return a[1] < b[1] ? -1 : 1;
  });
}

function eventsAt(day, today) {
  var offset = Math.round((day.getTime() - today.getTime()) / 86400000);
  return templatesFor(day, offset).map(function(t, i) { return build(day, i, t); });
}

function toMs(dateTime) {
  return Date.parse(dateTime.slice(0, 19) + 'Z');
}

// All events overlapping [startMs, endMs). Days a week around it are generated too
// so that moved events show up on their new day.
function eventsIn(startMs, endMs, now) {
  var today = new Date(now.getFullYear(), now.getMonth(), now.getDate());
  var first = new Date(startMs);
  first = new Date(first.getFullYear(), first.getMonth(), first.getDate() - 10);
  var result = [];
  for (var d = first; d.getTime() < endMs + 86400000; d = new Date(d.getFullYear(), d.getMonth(), d.getDate() + 1)) {
    result = result.concat(eventsAt(d, today));
  }
  return result.filter(function(e) {
    var s = e.isAllDay ? new Date(e.start.dateTime.slice(0, 10) + 'T00:00:00').getTime() : toMs(e.start.dateTime);
    var en = e.isAllDay ? new Date(e.end.dateTime.slice(0, 10) + 'T00:00:00').getTime() : toMs(e.end.dateTime);
    return s < endMs && en > startMs;
  }).sort(function(a, b) {
    return a.start.dateTime < b.start.dateTime ? -1 : 1;
  });
}

function findEvent(id, now) {
  var all = eventsIn(now.getTime() - 70 * 86400000, now.getTime() + 100 * 86400000, now);
  for (var i = 0; i < all.length; i++) {
    if (all[i].id === id) return all[i];
  }
  return null;
}

function query(url, name) {
  var m = new RegExp('[?&]' + name + '=([^&]*)').exec(url);
  return m ? decodeURIComponent(m[1]) : null;
}

// Answers a Graph request like the real API: returns [httpStatus, json].
function handle(method, url, body, now) {
  now = now || new Date();
  var path = url.replace(/^https:\/\/graph\.microsoft\.com\/v1\.0/, '').split('?')[0];
  if (method === 'GET' && path === '/me/calendars') {
    return [200, { value: [{ id: CALENDAR_ID, name: 'Calendar', isDefaultCalendar: true }] }];
  }
  if (method === 'GET' && /\/calendarView$/.test(path)) {
    if (path !== '/me/calendar/calendarView' && path !== '/me/calendars/' + CALENDAR_ID + '/calendarView') {
      return [200, { value: [] }];
    }
    var start = Date.parse(query(url, 'startDateTime'));
    var end = Date.parse(query(url, 'endDateTime'));
    return [200, { value: eventsIn(start, end, now) }];
  }
  var ev = /^\/me\/events\/([^/]+)$/.exec(path);
  if (ev) {
    var e = findEvent(decodeURIComponent(ev[1]), now);
    if (!e) return [404, { error: { code: 'ErrorItemNotFound' } }];
    if (method === 'GET') return [200, { id: e.id, bodyPreview: e.bodyPreview }];
    if (method === 'PATCH') {
      var patch = JSON.parse(body);
      moved[e.id] = { start: patch.start.dateTime, end: patch.end.dateTime };
      return [200, { id: e.id }];
    }
  }
  return [404, { error: { code: 'NotFound' } }];
}

module.exports = {
  handle: handle
};
