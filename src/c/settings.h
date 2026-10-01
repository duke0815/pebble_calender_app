#pragma once
#include <pebble.h>

typedef struct {
  uint8_t brief_enabled;
  uint8_t brief_hour;
  uint8_t brief_minute;
  uint8_t brief_days;   // Bit 0 = Sonntag ... Bit 6 = Samstag
  uint8_t lang;         // LANG_DE / LANG_EN (i18n.h)
  uint8_t touch_off;     // 1 = Touch-Bedienung aus (0 = an, damit alte Settings "an" bedeuten)
  uint8_t gray_past;     // 1 = vergangene Termine grau (Default 0 = alle gleich)
  uint8_t move_opts;     // Bitmaske MOVE_OPT_* (0 = Verschieben aus)
  uint32_t color_accent;  // 0xRRGGBB
  uint32_t color_text;
  uint32_t color_bg;
} Settings;

void settings_load(void);
void settings_save(void);
Settings *settings_get(void);

// Plant den naechsten Briefing-Wakeup (oder entfernt alle, wenn deaktiviert).
void settings_schedule_wakeup(void);
