#include "format.h"

static const char *const WEEKDAYS[] = { "So", "Mo", "Di", "Mi", "Do", "Fr", "Sa" };

void format_clock(time_t t, char *buf, size_t size) {
  struct tm *tm = localtime(&t);
  strftime(buf, size, clock_is_24h_style() ? "%H:%M" : "%I:%M %p", tm);
}

void format_event_time(const CalEvent *e, char *buf, size_t size) {
  if (e->all_day) {
    snprintf(buf, size, "Ganztägig");
    return;
  }
  char from[10];
  char to[10];
  format_clock(e->start, from, sizeof(from));
  format_clock(e->end, to, sizeof(to));
  snprintf(buf, size, "%s - %s", from, to);
}

void format_date_short(time_t t, char *buf, size_t size) {
  struct tm *tm = localtime(&t);
  snprintf(buf, size, "%s %02d.%02d.", WEEKDAYS[tm->tm_wday % 7], tm->tm_mday, tm->tm_mon + 1);
}

void format_day(time_t day, char *buf, size_t size) {
  time_t today = time_day_start(time(NULL));
  time_t tomorrow = time_day_start(today + 26 * 60 * 60);
  char date[16];
  format_date_short(day, date, sizeof(date));
  if (day == today) {
    snprintf(buf, size, "Heute, %s", date);
  } else if (day == tomorrow) {
    snprintf(buf, size, "Morgen, %s", date);
  } else {
    snprintf(buf, size, "%s", date);
  }
}
