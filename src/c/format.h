#pragma once
#include <pebble.h>
#include "events.h"

// "14:05" or "2:05 PM" depending on the watch setting
void format_clock(time_t t, char *buf, size_t size);
// "All day" or "09:00 - 10:30"
void format_event_time(const CalEvent *e, char *buf, size_t size);
// "Today, Wed Oct 1" / "Tomorrow, Thu Oct 2" / "Fri Oct 3" (DE: "Heute, Mi 01.10.")
void format_day(time_t day, char *buf, size_t size);
// "Wed Oct 1" (DE: "Mi 01.10.")
void format_date_short(time_t t, char *buf, size_t size);
