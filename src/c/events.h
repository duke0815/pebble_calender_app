#pragma once
#include <pebble.h>
#include "protocol.h"

typedef struct {
  int32_t start;          // Epoch-Sekunden (ganztaegig: lokale Mitternacht)
  int32_t end;            // Epoch-Sekunden (ganztaegig: exklusive lokale Mitternacht)
  uint8_t all_day;
  char title[TITLE_LEN];
  char location[LOCATION_LEN];
} CalEvent;

// Cache
void events_load_cache(void);
time_t events_last_sync(void);

// Empfang einer neuen Liste: Termine einzeln, dann commit.
void events_receive(int index, const CalEvent *event);
// Uebernimmt die empfangene Liste, wenn vollstaendig. Gibt true zurueck, wenn uebernommen.
bool events_commit(int count);

// Ansicht (gefiltert nach Modus, gruppiert nach Tag)
void events_rebuild_view(void);
uint16_t events_view_num_sections(void);
uint16_t events_view_num_rows(uint16_t section);
const CalEvent *events_view_get(uint16_t section, uint16_t row);
time_t events_view_section_day(uint16_t section);
// Index (section,row) des ersten noch nicht beendeten Termins; false, wenn keiner.
bool events_view_first_upcoming(uint16_t *section, uint16_t *row);

time_t time_day_start(time_t t);
// Lokale Uhrzeit hour:minute am Tag von base + day_offset (robust bei Zeitumstellung)
time_t time_make_local(time_t base, int day_offset, int hour, int minute);
