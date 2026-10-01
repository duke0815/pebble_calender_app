#pragma once
#include <pebble.h>

typedef struct {
  uint8_t brief_enabled;
  uint8_t brief_hour;
  uint8_t brief_minute;
  uint8_t brief_days;   // bit 0 = Sunday ... bit 6 = Saturday
  uint8_t lang;         // LANG_DE / LANG_EN (i18n.h)
  uint8_t touch_off;     // 1 = touch control off (0 = on, so old settings mean "on")
  uint8_t gray_past;     // 1 = past events gray (default 0 = all the same)
  uint8_t move_opts;     // bitmask MOVE_OPT_* (0 = moving off)
  uint32_t color_accent;  // 0xRRGGBB
  uint32_t color_text;
  uint32_t color_bg;
  uint8_t auto_close_min;  // exit after N minutes without use (0 = never); added last so older stored settings stay readable
} Settings;

void settings_load(void);
void settings_save(void);
Settings *settings_get(void);

// Schedules the next briefing wakeup (or removes all if disabled).
void settings_schedule_wakeup(void);
