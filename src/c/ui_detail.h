#pragma once
#include "events.h"

// Shows a copy of the event in its own window.
void ui_detail_show(const CalEvent *event);
// Description from the phone (matching the key of the opened event)
void ui_detail_set_note(int32_t key, const char *text);
bool ui_detail_is_open(void);
void ui_detail_close(void);
