#pragma once
#include "events.h"

// Zeigt eine Kopie des Termins in einem eigenen Fenster.
void ui_detail_show(const CalEvent *event);
bool ui_detail_is_open(void);
void ui_detail_close(void);
