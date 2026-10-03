#pragma once
#include "events.h"

// Actions on an event via Select in the detail view: move own events,
// respond to invitations. Menu -> confirmation -> result.

// true if at least one action is available for the event
bool ui_action_available(const CalEvent *event);
// Hint for the detail view ("Select: Move" / "Select: Respond"), or NULL
const char *ui_action_hint(const CalEvent *event);
// Short reason why the event cannot be moved (yet), or NULL (moving off,
// movable or respondable). For the hint in the detail view.
const char *ui_action_reason(const CalEvent *event);
// Own response to an invitation ("Your response: Accepted"), or NULL if
// the event cannot be answered. Valid until the next call.
const char *ui_action_response_text(const CalEvent *event);
// Action menu (move options with target date and/or responses)
void ui_action_show(const CalEvent *event);
// Reply from the phone. On success the menu, confirmation and detail view close.
void ui_action_result(int32_t key, bool ok, const char *text);
