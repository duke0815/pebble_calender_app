#pragma once
#include <pebble.h>

typedef struct {
  void (*interaction)(void);   // jede Benutzeraktion (für Inaktivitäts-Timer)
} UiMainCallbacks;

void ui_main_push(UiMainCallbacks callbacks);
void ui_main_destroy(void);

// Ansicht neu berechnen; optional Auswahl auf den nächsten anstehenden Termin setzen.
void ui_main_reload(bool select_upcoming);

// status: Text der Statuszeile; empty_text: Text, wenn keine Termine sichtbar sind.
void ui_main_set_status(const char *status, const char *empty_text);
