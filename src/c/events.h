#pragma once
#include <pebble.h>
#include "protocol.h"

typedef struct {
  int32_t start;          // Epoch-Sekunden (ganztaegig: lokale Mitternacht)
  int32_t end;            // Epoch-Sekunden (ganztaegig: exklusive lokale Mitternacht)
  int32_t key;            // Kennung für die Beschreibung (0 = keine)
  uint8_t all_day;
  char title[TITLE_LEN];
  char location[LOCATION_LEN];
} CalEvent;

// Cache (heute + morgen, persistent)
void events_load_cache(void);
time_t events_last_sync(void);

// Empfang einer Liste: Termine einzeln, dann commit.
void events_receive(int index, const CalEvent *event);
// Übernimmt die empfangene Liste, wenn vollständig. list_day = lokale Mitternacht des
// ersten Tages, list_days = Anzahl Tage. Listen mit >= 2 Tagen ersetzen den Cache,
// 1-Tages-Listen landen im Tagespuffer (nur RAM) – aber nur für den gerade angezeigten Tag.
// Gibt true zurück, wenn übernommen.
bool events_commit(int count, time_t list_day, int list_days);

// Ansicht: genau ein Tag
void events_set_view_day(time_t day);
time_t events_view_day(void);
// true, wenn für den angezeigten Tag Daten vorliegen (Cache oder Tagespuffer)
bool events_view_available(void);
void events_rebuild_view(void);
uint16_t events_view_count(void);
const CalEvent *events_view_get(uint16_t row);
// Zeile des ersten noch nicht beendeten Termins; false, wenn keiner.
bool events_view_first_upcoming(uint16_t *row);

time_t time_day_start(time_t t);
// Lokale Uhrzeit hour:minute am Tag von base + day_offset (robust bei Zeitumstellung)
time_t time_make_local(time_t base, int day_offset, int hour, int minute);
