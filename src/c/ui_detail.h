#pragma once
#include "events.h"

// Zeigt eine Kopie des Termins in einem eigenen Fenster.
void ui_detail_show(const CalEvent *event);
// Beschreibung vom Handy (passend zur Kennung des geöffneten Termins)
void ui_detail_set_note(int32_t key, const char *text);
bool ui_detail_is_open(void);
void ui_detail_close(void);
