#pragma once
#include <pebble.h>
#include "protocol.h"

typedef struct {
  int32_t start;          // epoch seconds (all-day: local midnight)
  int32_t end;            // epoch seconds (all-day: exclusive local midnight)
  int32_t key;            // key for the description (0 = none)
  uint8_t all_day;
  uint8_t flags;          // EVT_FLAG_*
  char title[TITLE_LEN];
  char location[LOCATION_LEN];
} CalEvent;

// Cache (today + tomorrow, persistent)
void events_load_cache(void);
time_t events_last_sync(void);

// Receiving a list: events one by one, then commit.
void events_receive(int index, const CalEvent *event);
// Applies the received list if complete. list_day = local midnight of the
// first day, list_days = number of days. Lists with >= 2 days replace the cache,
// 1-day lists go into the day buffer (RAM only) - but only for the currently displayed day.
// Returns true if applied.
bool events_commit(int count, time_t list_day, int list_days);

// View: exactly one day
void events_set_view_day(time_t day);
time_t events_view_day(void);
// Discard the day buffer (e.g. after moving an event)
void events_invalidate_day(void);
// true if data is available for the displayed day (cache or day buffer)
bool events_view_available(void);
void events_rebuild_view(void);
uint16_t events_view_count(void);
const CalEvent *events_view_get(uint16_t row);
// Row of the first event that has not ended yet; false if none.
bool events_view_first_upcoming(uint16_t *row);

time_t time_day_start(time_t t);
// Local time hour:minute on the day of base + day_offset (robust across DST changes)
time_t time_make_local(time_t base, int day_offset, int hour, int minute);
