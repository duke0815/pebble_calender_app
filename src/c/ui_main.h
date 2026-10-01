#pragma once
#include <pebble.h>

typedef struct {
  void (*interaction)(void);   // jede Benutzeraktion (für Inaktivitäts-Timer)
} UiMainCallbacks;

void ui_main_push(UiMainCallbacks callbacks);
void ui_main_destroy(void);

// Ansicht neu berechnen; optional Auswahl auf den nächsten anstehenden Termin setzen.
void ui_main_reload(bool select_upcoming);

// Auswahl um rows Zeilen verschieben (negativ = nach oben), z. B. per Wischgeste.
void ui_main_scroll(int rows);
// true, wenn das Hauptfenster oben liegt (kein Detail-/Login-Fenster darüber)
bool ui_main_is_top(void);

// status: Text der Statuszeile; empty_text: Text, wenn keine Termine sichtbar sind.
void ui_main_set_status(const char *status, const char *empty_text);
