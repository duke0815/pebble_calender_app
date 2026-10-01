#include "theme.h"
#include "settings.h"

#if defined(PBL_COLOR)
static bool prv_is_light(GColor c) {
  // channels 0..3, weighted brightness 0..3000
  return (c.r * 299 + c.g * 587 + c.b * 114) > 1500;
}

GColor theme_accent(void) { return GColorFromHEX(settings_get()->color_accent); }
GColor theme_text(void) { return GColorFromHEX(settings_get()->color_text); }
GColor theme_bg(void) { return GColorFromHEX(settings_get()->color_bg); }

GColor theme_on_accent(void) {
  return prv_is_light(theme_accent()) ? GColorBlack : GColorWhite;
}

GColor theme_muted(void) {
  return prv_is_light(theme_bg()) ? GColorDarkGray : GColorLightGray;
}

GColor theme_header_bg(void) {
  return prv_is_light(theme_bg()) ? GColorLightGray : GColorDarkGray;
}

GColor theme_header_text(void) {
  return prv_is_light(theme_header_bg()) ? GColorBlack : GColorWhite;
}
#else
GColor theme_accent(void) { return GColorBlack; }
GColor theme_on_accent(void) { return GColorWhite; }
GColor theme_text(void) { return GColorBlack; }
GColor theme_bg(void) { return GColorWhite; }
GColor theme_muted(void) { return GColorBlack; }
GColor theme_header_bg(void) { return GColorBlack; }
GColor theme_header_text(void) { return GColorWhite; }
#endif
