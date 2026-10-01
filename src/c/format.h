#pragma once
#include <pebble.h>
#include "events.h"

// "14:05" bzw. "2:05 PM" je nach Uhr-Einstellung
void format_clock(time_t t, char *buf, size_t size);
// "Ganztägig" oder "09:00 - 10:30"
void format_event_time(const CalEvent *e, char *buf, size_t size);
// "Heute, Mi 01.10." / "Morgen, Do 02.10." / "Fr 03.10."
void format_day(time_t day, char *buf, size_t size);
// "Mi 01.10."
void format_date_short(time_t t, char *buf, size_t size);
