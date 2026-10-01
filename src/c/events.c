#include "events.h"
#include "settings.h"

#define CACHE_VERSION 1

#define PERSIST_KEY_VERSION   1
#define PERSIST_KEY_COUNT     2
#define PERSIST_KEY_LAST_SYNC 3
#define PERSIST_KEY_EVENT_0   100

static CalEvent s_events[MAX_EVENTS];
static int s_count;
static time_t s_last_sync;

static CalEvent s_incoming[MAX_EVENTS];
static uint32_t s_incoming_mask;  // Bit i = Termin i empfangen

// Ansicht
static uint8_t s_view_idx[MAX_EVENTS];
static uint8_t s_view_count;
static uint8_t s_section_first[MAX_EVENTS + 1];  // erster Ansichts-Index je Section
static time_t s_section_day[MAX_EVENTS];
static uint8_t s_section_count;

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

static time_t prv_next_day_start(time_t day_start) {
  // +26 h und abrunden: robust gegen Sommer-/Winterzeit
  return time_day_start(day_start + 26 * 60 * 60);
}

void events_load_cache(void) {
  s_count = 0;
  s_last_sync = 0;
  if (persist_read_int(PERSIST_KEY_VERSION) != CACHE_VERSION) {
    return;
  }
  int count = persist_read_int(PERSIST_KEY_COUNT);
  if (count < 0 || count > MAX_EVENTS) {
    return;
  }
  for (int i = 0; i < count; i++) {
    if (persist_read_data(PERSIST_KEY_EVENT_0 + i, &s_events[i], sizeof(CalEvent)) != (int)sizeof(CalEvent)) {
      return;  // Cache unvollstaendig -> verwerfen
    }
    s_events[i].title[TITLE_LEN - 1] = '\0';
    s_events[i].location[LOCATION_LEN - 1] = '\0';
  }
  s_count = count;
  s_last_sync = persist_read_int(PERSIST_KEY_LAST_SYNC);
}

static void prv_save_cache(void) {
  persist_write_int(PERSIST_KEY_VERSION, CACHE_VERSION);
  for (int i = 0; i < s_count; i++) {
    persist_write_data(PERSIST_KEY_EVENT_0 + i, &s_events[i], sizeof(CalEvent));
  }
  for (int i = s_count; i < MAX_EVENTS; i++) {
    if (persist_exists(PERSIST_KEY_EVENT_0 + i)) {
      persist_delete(PERSIST_KEY_EVENT_0 + i);
    }
  }
  persist_write_int(PERSIST_KEY_COUNT, s_count);
  persist_write_int(PERSIST_KEY_LAST_SYNC, (int32_t)s_last_sync);
}

time_t events_last_sync(void) {
  return s_last_sync;
}

void events_receive(int index, const CalEvent *event) {
  if (index < 0 || index >= MAX_EVENTS) {
    return;
  }
  if (index == 0) {
    s_incoming_mask = 0;
  }
  s_incoming[index] = *event;
  s_incoming_mask |= (1u << index);
}

bool events_commit(int count) {
  if (count > MAX_EVENTS) {
    count = MAX_EVENTS;
  }
  uint32_t expected = (count == 0) ? 0 : ((1u << count) - 1);
  if (count < 0 || (s_incoming_mask & expected) != expected) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "Liste unvollstaendig, Cache bleibt");
    s_incoming_mask = 0;
    return false;
  }
  memcpy(s_events, s_incoming, sizeof(CalEvent) * count);
  s_count = count;
  s_last_sync = time(NULL);
  s_incoming_mask = 0;
  prv_save_cache();
  return true;
}

// ---------------------------------------------------------------------------
// Ansicht

void events_rebuild_view(void) {
  Settings *settings = settings_get();
  time_t now = time(NULL);
  time_t today = time_day_start(now);
  time_t tomorrow = prv_next_day_start(today);

  s_view_count = 0;
  s_section_count = 0;

  for (int i = 0; i < s_count; i++) {
    const CalEvent *e = &s_events[i];
    bool visible;
    if (settings->mode == MODE_TODAY) {
      visible = e->start < tomorrow && e->end > today;
    } else {
      visible = e->end > now && s_view_count < settings->count;
    }
    if (!visible) {
      continue;
    }
    time_t shown_start = e->start > today ? e->start : today;
    time_t day = time_day_start(shown_start);
    if (s_section_count == 0 || s_section_day[s_section_count - 1] != day) {
      s_section_day[s_section_count] = day;
      s_section_first[s_section_count] = s_view_count;
      s_section_count++;
    }
    s_view_idx[s_view_count++] = i;
  }
  s_section_first[s_section_count] = s_view_count;
}

uint16_t events_view_num_sections(void) {
  return s_section_count;
}

uint16_t events_view_num_rows(uint16_t section) {
  if (section >= s_section_count) {
    return 0;
  }
  return s_section_first[section + 1] - s_section_first[section];
}

const CalEvent *events_view_get(uint16_t section, uint16_t row) {
  if (section >= s_section_count || row >= events_view_num_rows(section)) {
    return NULL;
  }
  return &s_events[s_view_idx[s_section_first[section] + row]];
}

time_t events_view_section_day(uint16_t section) {
  return section < s_section_count ? s_section_day[section] : 0;
}

bool events_view_first_upcoming(uint16_t *section, uint16_t *row) {
  time_t now = time(NULL);
  for (uint16_t s = 0; s < s_section_count; s++) {
    for (uint16_t r = 0; r < events_view_num_rows(s); r++) {
      const CalEvent *e = events_view_get(s, r);
      if (e->end > now) {
        *section = s;
        *row = r;
        return true;
      }
    }
  }
  return false;
}
