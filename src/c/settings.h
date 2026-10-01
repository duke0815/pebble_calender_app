#pragma once
#include <pebble.h>

typedef struct {
  uint8_t brief_enabled;
  uint8_t brief_hour;
  uint8_t brief_minute;
  uint8_t brief_days;   // Bit 0 = Sonntag ... Bit 6 = Samstag
} Settings;

void settings_load(void);
void settings_save(void);
Settings *settings_get(void);

// Plant den naechsten Briefing-Wakeup (oder entfernt alle, wenn deaktiviert).
void settings_schedule_wakeup(void);
