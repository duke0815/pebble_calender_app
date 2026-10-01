#pragma once
#include "events.h"

// true if the event may be moved and at least one option applies
bool ui_move_available(const CalEvent *event);
// Short reason why it cannot be moved (yet), or NULL (moving off
// or movable). For the hint in the detail view.
const char *ui_move_reason(const CalEvent *event);
// Pick a move option (with target date) -> confirmation -> result
void ui_move_show(const CalEvent *event);
// Reply from the phone. On success the selection, confirmation and detail view close.
void ui_move_result(int32_t key, bool ok, const char *text);
