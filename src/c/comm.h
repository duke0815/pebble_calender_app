#pragma once
#include <pebble.h>

typedef struct {
  // list_days >= 2: Cache (heute + morgen), sonst ein einzelner Tag
  void (*list_updated)(bool complete, time_t list_day, int list_days);
  void (*status)(int code, const char *text);
  void (*login_code)(const char *url, const char *code);
  void (*settings_changed)(void);
  void (*send_failed)(void);
} CommHandlers;

void comm_init(CommHandlers handlers);
void comm_deinit(void);

// Fordert Daten an: day = 0 -> Cache (heute + morgen), sonst lokale Mitternacht
// eines einzelnen Tages. false, wenn sofort fehlgeschlagen.
bool comm_request(time_t day);
