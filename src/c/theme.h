#pragma once
#include <pebble.h>

// Colors from the settings (accent, text, background) plus contrast colors
// derived from them. Fixed colors on black-and-white watches.
GColor theme_accent(void);      // date bar, selected row
GColor theme_on_accent(void);   // text on the accent color (automatically light/dark)
GColor theme_text(void);
GColor theme_bg(void);
GColor theme_muted(void);       // past events
GColor theme_header_bg(void);   // status line
GColor theme_header_text(void);
