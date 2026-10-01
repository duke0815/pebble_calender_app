#pragma once
#include <pebble.h>

typedef struct {
  void (*interaction)(void);   // jede Benutzeraktion (für Inaktivitäts-Timer)
  void (*back)(void);          // Zurück-Taste in der Tagesansicht (nur Touch-Plattformen)
} UiMainCallbacks;

void ui_main_push(UiMainCallbacks callbacks);
// Farben aus den Einstellungen neu anwenden
void ui_main_apply_theme(void);
void ui_main_destroy(void);

// Ansicht neu berechnen; optional Auswahl auf den nächsten anstehenden Termin setzen.
void ui_main_reload(bool select_upcoming);

// true, wenn das Hauptfenster oben liegt (kein Detail-/Login-Fenster darüber)
bool ui_main_is_top(void);

// Oben steht immer das Datum des angezeigten Tages. status: optionale Zeile darunter
// ("Aktualisiere...", "Offline - Stand ...", Fehler; leer = ausgeblendet);
// empty_text: Text, wenn keine Termine sichtbar sind.
void ui_main_set_status(const char *status, const char *empty_text);
