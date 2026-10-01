#include "events.h"

#define CACHE_VERSION 2

#define PERSIST_KEY_VERSION    1
#define PERSIST_KEY_COUNT      2
#define PERSIST_KEY_LAST_SYNC  3
#define PERSIST_KEY_CACHE_DAY  5
#define PERSIST_KEY_CACHE_DAYS 6
#define PERSIST_KEY_EVENT_0    100

// Cache: heute + morgen (persistent)
static CalEvent s_cache[MAX_CACHE_EVENTS];
static int s_cache_count;
static time_t s_cache_day;
static int s_cache_days;
static time_t s_last_sync;

// Tagespuffer: ein beliebiger anderer Tag (nur RAM)
static CalEvent s_day[MAX_DAY_EVENTS];
static int s_day_count;
static time_t s_day_key;

static CalEvent s_incoming[MAX_CACHE_EVENTS];
static uint32_t s_incoming_mask;  // Bit i = Termin i empfangen

// Ansicht
static time_t s_view_day;
static const CalEvent *s_view[MAX_DAY_EVENTS];
static uint8_t s_view_count;

time_t time_make_local(time_t base, int day_offset, int hour, int minute) {
  struct tm tm = *localtime(&base);
  tm.tm_mday += day_offset;
  tm.tm_hour = hour;
  tm.tm_min = minute;
  tm.tm_sec = 0;
  time_t result = mktime(&tm);
  // tm_isdst stammt von base; liegt eine Zeitumstellung dazwischen, ist das Ergebnis
  // um eine Stunde verschoben -> gegen die gewuenschte Uhrzeit korrigieren.
  struct tm *check = localtime(&result);
  int diff = (hour - check->tm_hour) * 3600 + (minute - check->tm_min) * 60;
  if (diff > 12 * 3600) diff -= 24 * 3600;
  if (diff < -12 * 3600) diff += 24 * 3600;
  return result + diff;
}

time_t time_day_start(time_t t) {
  return time_make_local(t, 0, 0, 0);
}

// ---------------------------------------------------------------------------
// Cache

void events_load_cache(void) {
  s_cache_count = 0;
  s_cache_day = 0;
  s_cache_days = 0;
  s_last_sync = 0;
  if (persist_read_int(PERSIST_KEY_VERSION) != CACHE_VERSION) {
    return;
  }
  int count = persist_read_int(PERSIST_KEY_COUNT);
  if (count < 0 || count > MAX_CACHE_EVENTS) {
    return;
  }
  for (int i = 0; i < count; i++) {
    if (persist_read_data(PERSIST_KEY_EVENT_0 + i, &s_cache[i], sizeof(CalEvent)) != (int)sizeof(CalEvent)) {
      return;  // Cache unvollstaendig -> verwerfen
    }
    s_cache[i].title[TITLE_LEN - 1] = '\0';
    s_cache[i].location[LOCATION_LEN - 1] = '\0';
  }
  s_cache_count = count;
  s_cache_day = persist_read_int(PERSIST_KEY_CACHE_DAY);
  s_cache_days = persist_read_int(PERSIST_KEY_CACHE_DAYS);
  s_last_sync = persist_read_int(PERSIST_KEY_LAST_SYNC);
}

static void prv_save_cache(void) {
  persist_write_int(PERSIST_KEY_VERSION, CACHE_VERSION);
  for (int i = 0; i < s_cache_count; i++) {
    persist_write_data(PERSIST_KEY_EVENT_0 + i, &s_cache[i], sizeof(CalEvent));
  }
  for (int i = s_cache_count; i < MAX_CACHE_EVENTS; i++) {
    if (persist_exists(PERSIST_KEY_EVENT_0 + i)) {
      persist_delete(PERSIST_KEY_EVENT_0 + i);
    }
  }
  persist_write_int(PERSIST_KEY_COUNT, s_cache_count);
  persist_write_int(PERSIST_KEY_CACHE_DAY, (int32_t)s_cache_day);
  persist_write_int(PERSIST_KEY_CACHE_DAYS, s_cache_days);
  persist_write_int(PERSIST_KEY_LAST_SYNC, (int32_t)s_last_sync);
}

time_t events_last_sync(void) {
  return s_last_sync;
}

void events_receive(int index, const CalEvent *event) {
  if (index < 0 || index >= MAX_CACHE_EVENTS) {
    return;
  }
  if (index == 0) {
    s_incoming_mask = 0;
  }
  s_incoming[index] = *event;
  s_incoming_mask |= (1u << index);
}

bool events_commit(int count, time_t list_day, int list_days) {
  int limit = list_days >= 2 ? MAX_CACHE_EVENTS : MAX_DAY_EVENTS;
  if (count > limit) {
    count = limit;
  }
  uint32_t expected = (count <= 0) ? 0 : ((1u << count) - 1);
  if (count < 0 || list_day == 0 || (s_incoming_mask & expected) != expected) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "Liste unvollstaendig, verworfen");
    s_incoming_mask = 0;
    return false;
  }
  if (list_days >= 2) {
    memcpy(s_cache, s_incoming, sizeof(CalEvent) * count);
    s_cache_count = count;
    s_cache_day = list_day;
    s_cache_days = list_days;
    s_last_sync = time(NULL);
    prv_save_cache();
  } else {
    memcpy(s_day, s_incoming, sizeof(CalEvent) * count);
    s_day_count = count;
    s_day_key = list_day;
  }
  s_incoming_mask = 0;
  return true;
}

// ---------------------------------------------------------------------------
// Ansicht

static bool prv_cache_covers(time_t day) {
  if (s_cache_days <= 0 || s_cache_day == 0) return false;
  time_t cache_end = time_make_local(s_cache_day, s_cache_days, 0, 0);
  return day >= s_cache_day && day < cache_end;
}

void events_set_view_day(time_t day) {
  s_view_day = time_day_start(day);
  events_rebuild_view();
}

time_t events_view_day(void) {
  return s_view_day;
}

bool events_view_available(void) {
  return prv_cache_covers(s_view_day) || s_day_key == s_view_day;
}

void events_rebuild_view(void) {
  time_t day = s_view_day;
  time_t next_day = time_make_local(day, 1, 0, 0);
  const CalEvent *source;
  int count;
  if (prv_cache_covers(day)) {
    source = s_cache;
    count = s_cache_count;
  } else if (s_day_key == day) {
    source = s_day;
    count = s_day_count;
  } else {
    source = NULL;
    count = 0;
  }

  s_view_count = 0;
  for (int i = 0; i < count && s_view_count < MAX_DAY_EVENTS; i++) {
    const CalEvent *e = &source[i];
    if (e->start < next_day && e->end > day) {
      s_view[s_view_count++] = e;
    }
  }
}

uint16_t events_view_count(void) {
  return s_view_count;
}

const CalEvent *events_view_get(uint16_t row) {
  return row < s_view_count ? s_view[row] : NULL;
}

bool events_view_first_upcoming(uint16_t *row) {
  time_t now = time(NULL);
  for (uint16_t r = 0; r < s_view_count; r++) {
    if (s_view[r]->end > now) {
      *row = r;
      return true;
    }
  }
  return false;
}
