// Host-Test der Uhr-Logik (events.c, settings.c) gegen den Stub-Header.
// Ausführen: sh test/c/run.sh
#include <pebble.h>
#include <assert.h>
#include <stdlib.h>
#include "../../src/c/events.h"
#include "../../src/c/settings.h"

// --- Fakes ------------------------------------------------------------------
#define NKEYS 256
static struct { bool used; uint8_t data[256]; int len; } s_store[NKEYS];
bool persist_exists(const uint32_t key) { return key < NKEYS && s_store[key].used; }
int persist_read_data(const uint32_t key, void *buf, const size_t size) {
  if (!persist_exists(key)) return E_ERROR;
  int n = s_store[key].len < (int)size ? s_store[key].len : (int)size;
  memcpy(buf, s_store[key].data, n);
  return n;
}
int persist_write_data(const uint32_t key, const void *data, const size_t size) {
  assert(size <= 256);
  s_store[key].used = true; memcpy(s_store[key].data, data, size); s_store[key].len = size;
  return size;
}
int32_t persist_read_int(const uint32_t key) { int32_t v = 0; persist_read_data(key, &v, 4); return v; }
status_t persist_write_int(const uint32_t key, const int32_t v) { persist_write_data(key, &v, 4); return 0; }
status_t persist_delete(const uint32_t key) { s_store[key].used = false; return 0; }

static time_t s_now;
static time_t s_wakeups[8];
static int s_wakeup_count;
static int s_reject_first;
time_t fake_time(time_t *t) { if (t) *t = s_now; return s_now; }
WakeupId wakeup_schedule(time_t ts, int32_t cookie, bool notify) {
  if (s_reject_first > 0) { s_reject_first--; return E_RANGE; }
  s_wakeups[s_wakeup_count++] = ts; return 1;
}
void wakeup_cancel_all(void) { s_wakeup_count = 0; }
void app_log(uint8_t l, const char *f, int n, const char *fmt, ...) {}

static time_t local(int y, int mo, int d, int h, int mi) {
  struct tm tm = { .tm_year = y - 1900, .tm_mon = mo - 1, .tm_mday = d, .tm_hour = h, .tm_min = mi, .tm_isdst = -1 };
  return mktime(&tm);
}

static void add(int i, time_t start, time_t end, int all_day, const char *title) {
  CalEvent e; memset(&e, 0, sizeof(e));
  e.start = start; e.end = end; e.all_day = all_day;
  snprintf(e.title, sizeof(e.title), "%s", title);
  events_receive(i, &e);
}

int main(void) {
  setenv("TZ", "Europe/Vienna", 1); tzset();
  printf("sizeof(CalEvent)=%zu, Cache gesamt=%zu Byte\n", sizeof(CalEvent), sizeof(CalEvent) * MAX_CACHE_EVENTS);
  assert(sizeof(CalEvent) <= 256);
  assert(sizeof(CalEvent) * MAX_CACHE_EVENTS <= 2600);

  s_now = local(2026, 10, 1, 10, 0);  // Do 01.10.2026 10:00
  settings_load();
  events_load_cache();
  events_set_view_day(s_now);
  assert(!events_view_available());   // noch kein Cache

  time_t today = local(2026, 10, 1, 0, 0);
  time_t tomorrow = local(2026, 10, 2, 0, 0);

  // Cache-Liste (heute + morgen): Ganztägig seit gestern, vorbei, später, morgen
  add(0, local(2026, 9, 30, 0, 0), local(2026, 10, 2, 0, 0), 1, "Messe");
  add(1, local(2026, 10, 1, 8, 0), local(2026, 10, 1, 9, 0), 0, "Frueh");
  add(2, local(2026, 10, 1, 14, 0), local(2026, 10, 1, 15, 0), 0, "Spaeter");
  add(3, local(2026, 10, 2, 9, 0), local(2026, 10, 2, 10, 0), 0, "Morgen");
  assert(!events_commit(5, today, 2));        // unvollständig -> verworfen
  add(0, local(2026, 9, 30, 0, 0), local(2026, 10, 2, 0, 0), 1, "Messe");
  add(1, local(2026, 10, 1, 8, 0), local(2026, 10, 1, 9, 0), 0, "Frueh");
  add(2, local(2026, 10, 1, 14, 0), local(2026, 10, 1, 15, 0), 0, "Spaeter");
  add(3, local(2026, 10, 2, 9, 0), local(2026, 10, 2, 10, 0), 0, "Morgen");
  assert(events_commit(4, today, 2));

  // Heute: 3 Termine, erster anstehender = Messe (läuft noch)
  events_rebuild_view();
  assert(events_view_available());
  assert(events_view_count() == 3);
  uint16_t r;
  assert(events_view_first_upcoming(&r) && r == 0);

  // Morgen kommt aus dem Cache
  events_set_view_day(tomorrow);
  assert(events_view_available() && events_view_count() == 1);
  assert(strcmp(events_view_get(0)->title, "Morgen") == 0);

  // Übermorgen fehlt -> Tagesliste vom Handy (nur RAM)
  time_t day3 = local(2026, 10, 3, 0, 0);
  events_set_view_day(day3);
  assert(!events_view_available());
  add(0, local(2026, 10, 3, 11, 0), local(2026, 10, 3, 12, 0), 0, "Samstag");
  assert(events_commit(1, day3, 1));
  events_rebuild_view();
  assert(events_view_available() && events_view_count() == 1);
  // Gestern: ebenfalls Tagesliste, ersetzt den Tagespuffer
  time_t yesterday = local(2026, 9, 30, 0, 0);
  events_set_view_day(yesterday);
  assert(!events_view_available());
  assert(events_commit(0, yesterday, 1));      // leerer Tag
  events_rebuild_view();
  assert(events_view_available() && events_view_count() == 0);
  // Cache unverändert
  events_set_view_day(today);
  assert(events_view_count() == 3);

  // Max. 12 Termine pro Tag in der Ansicht
  for (int i = 0; i < 14; i++) add(i, local(2026, 10, 5, 8, i), local(2026, 10, 5, 9, i), 0, "X");
  time_t day5 = local(2026, 10, 5, 0, 0);
  assert(events_commit(14, day5, 1));          // auf MAX_DAY_EVENTS gekürzt
  events_set_view_day(day5);
  assert(events_view_count() == MAX_DAY_EVENTS);

  // Cache neu laden
  events_load_cache();
  assert(events_last_sync() == s_now);
  events_set_view_day(today);
  assert(events_view_count() == 3);

  // Nächster Tag (Fr) mit Cache vom Donnerstag: "morgen" ist jetzt heute
  s_now = local(2026, 10, 2, 7, 0);
  events_set_view_day(s_now);
  assert(events_view_available() && events_view_count() == 1);
  assert(strcmp(events_view_get(0)->title, "Morgen") == 0);
  // Samstag ist nicht im Cache
  events_set_view_day(day3);
  assert(!events_view_available());

  // Briefing: Do 01.10. 10:00, 07:00 Mo-Fr -> Fr 02.10. 07:00
  s_now = local(2026, 10, 1, 10, 0);
  Settings *st = settings_get();
  st->brief_enabled = 1; st->brief_hour = 7; st->brief_minute = 0; st->brief_days = 0x3E;
  settings_schedule_wakeup();
  assert(s_wakeup_count == 1 && s_wakeups[0] == local(2026, 10, 2, 7, 0));
  // Fr 02.10. 08:00 -> Wochenende überspringen -> Mo 05.10. 07:00
  s_now = local(2026, 10, 2, 8, 0);
  settings_schedule_wakeup();
  assert(s_wakeup_count == 1 && s_wakeups[0] == local(2026, 10, 5, 7, 0));
  // Kollision: zwei Mal E_RANGE -> +2 Minuten
  s_reject_first = 2;
  settings_schedule_wakeup();
  assert(s_wakeup_count == 1 && s_wakeups[0] == local(2026, 10, 5, 7, 2));
  // Zeitumstellung: Sa 24.10. 12:00, nur Sonntag -> So 25.10. 07:00 (MEZ)
  s_now = local(2026, 10, 24, 12, 0); st->brief_days = 0x01;
  settings_schedule_wakeup();
  assert(s_wakeup_count == 1 && s_wakeups[0] == local(2026, 10, 25, 7, 0));
  // Deaktiviert -> nichts geplant
  st->brief_enabled = 0;
  settings_schedule_wakeup();
  assert(s_wakeup_count == 0);

  // Tagesbeginn am Umstellungstag (So 25.10., 12:00 MEZ) = 00:00 MESZ
  assert(time_day_start(local(2026, 10, 25, 12, 0)) == local(2026, 10, 25, 0, 0));
  assert(time_day_start(local(2026, 3, 29, 12, 0)) == local(2026, 3, 29, 0, 0));

  printf("Uhr-Logik ok\n");
  return 0;
}
