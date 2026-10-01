#include "format.h"
#include "i18n.h"
#include "settings.h"

void format_clock(time_t t, char *buf, size_t size) {
  struct tm *tm = localtime(&t);
  strftime(buf, size, clock_is_24h_style() ? "%H:%M" : "%I:%M %p", tm);
}

void format_event_time(const CalEvent *e, char *buf, size_t size) {
  if (e->all_day) {
    snprintf(buf, size, "%s", tr(STR_ALL_DAY));
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
  if (settings_get()->lang == LANG_EN) {
    snprintf(buf, size, "%s %s %d", tr_weekday(tm->tm_wday), tr_month(tm->tm_mon), tm->tm_mday);
  } else {
    snprintf(buf, size, "%s %02d.%02d.", tr_weekday(tm->tm_wday), tm->tm_mday, tm->tm_mon + 1);
  }
}

void format_day(time_t day, char *buf, size_t size) {
  time_t today = time_day_start(time(NULL));
  time_t tomorrow = time_day_start(today + 26 * 60 * 60);
  char date[16];
  format_date_short(day, date, sizeof(date));
  if (day == today) {
    snprintf(buf, size, "%s, %s", tr(STR_TODAY), date);
  } else if (day == tomorrow) {
    snprintf(buf, size, "%s, %s", tr(STR_TOMORROW), date);
  } else {
    snprintf(buf, size, "%s", date);
  }
}
