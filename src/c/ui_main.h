#pragma once
#include <pebble.h>

typedef struct {
  void (*interaction)(void);   // any user action (for the inactivity timer)
  void (*back)(void);          // Back button in the day view
  void (*change_day)(int delta);  // long-press up/down: -1 / +1 day
} UiMainCallbacks;

void ui_main_push(UiMainCallbacks callbacks);
// Re-apply colors from the settings
void ui_main_apply_theme(void);
void ui_main_destroy(void);

// Recompute the view; optionally move the selection to the next upcoming event.
void ui_main_reload(bool select_upcoming);

// Slide the list in from the side on day change (delta > 0: from the right)
void ui_main_animate_day(int delta);
// true if the main window is on top (no detail/login window above it)
bool ui_main_is_top(void);

// The top always shows the date of the displayed day. status: optional line below
// ("Updating...", "Offline - as of ...", error; empty = hidden);
// empty_text: text shown when no events are visible.
void ui_main_set_status(const char *status, const char *empty_text);
