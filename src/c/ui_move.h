#pragma once
#include "events.h"

// true, wenn der Termin verschoben werden darf und mindestens eine Option passt
bool ui_move_available(const CalEvent *event);
// Kurzer Grund, warum (noch) nicht verschoben werden kann, oder NULL (Verschieben aus
// bzw. verschiebbar). Für den Hinweis in der Detailansicht.
const char *ui_move_reason(const CalEvent *event);
// Auswahl der Verschiebe-Optionen (mit Zieldatum) -> Bestätigung -> Ergebnis
void ui_move_show(const CalEvent *event);
// Antwort vom Handy. Bei Erfolg schließen sich Auswahl, Bestätigung und Detailansicht.
void ui_move_result(int32_t key, bool ok, const char *text);
