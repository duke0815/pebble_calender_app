#include <pebble.h>
#include "ui_login.h"
#include "protocol.h"
#include "i18n.h"

#if defined(PBL_PLATFORM_EMERY)
  #define FONT_SMALL FONT_KEY_GOTHIC_18
  #define FONT_URL   FONT_KEY_GOTHIC_24_BOLD
  #define FONT_CODE  FONT_KEY_GOTHIC_28_BOLD
  #define H_SMALL 22
  #define H_URL   52
  #define H_CODE  36
#else
  #define FONT_SMALL FONT_KEY_GOTHIC_14
  #define FONT_URL   FONT_KEY_GOTHIC_18_BOLD
  #define FONT_CODE  FONT_KEY_GOTHIC_24_BOLD
  #define H_SMALL 18
  #define H_URL   40
  #define H_CODE  30
#endif

#define PAD 6

static Window *s_window;
static TextLayer *s_title_layer;
static TextLayer *s_open_layer;
static TextLayer *s_url_layer;
static TextLayer *s_enter_layer;
static TextLayer *s_code_layer;
static TextLayer *s_hint_layer;
static char s_url[LOGIN_URL_LEN];
static char s_code[LOGIN_CODE_LEN];

static TextLayer *prv_text(Layer *root, GRect frame, const char *text, const char *font,
                           GTextAlignment align) {
  TextLayer *layer = text_layer_create(frame);
  text_layer_set_text(layer, text);
  text_layer_set_font(layer, fonts_get_system_font(font));
  text_layer_set_text_alignment(layer, align);
  text_layer_set_overflow_mode(layer, GTextOverflowModeWordWrap);
  text_layer_set_background_color(layer, GColorClear);
  layer_add_child(root, text_layer_get_layer(layer));
  return layer;
}

static void prv_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  int16_t w = bounds.size.w - 2 * PAD;
  int16_t y = 0;

  s_title_layer = prv_text(root, GRect(0, y, bounds.size.w, H_SMALL + 4), tr(STR_LOGIN_TITLE),
                           FONT_SMALL, GTextAlignmentCenter);
  text_layer_set_background_color(s_title_layer, PBL_IF_COLOR_ELSE(GColorOxfordBlue, GColorBlack));
  text_layer_set_text_color(s_title_layer, GColorWhite);
  y += H_SMALL + 8;

  s_open_layer = prv_text(root, GRect(PAD, y, w, H_SMALL), tr(STR_LOGIN_OPEN), FONT_SMALL,
                          GTextAlignmentLeft);
  y += H_SMALL;
  s_url_layer = prv_text(root, GRect(PAD, y, w, H_URL), s_url, FONT_URL, GTextAlignmentLeft);
  y += H_URL;
  s_enter_layer = prv_text(root, GRect(PAD, y, w, H_SMALL), tr(STR_LOGIN_ENTER), FONT_SMALL,
                           GTextAlignmentLeft);
  y += H_SMALL;
  s_code_layer = prv_text(root, GRect(PAD, y, w, H_CODE), s_code, FONT_CODE, GTextAlignmentCenter);
  text_layer_set_text_color(s_code_layer, PBL_IF_COLOR_ELSE(GColorCobaltBlue, GColorBlack));
  y += H_CODE + 4;
  s_hint_layer = prv_text(root, GRect(PAD, y, w, bounds.size.h - y), tr(STR_LOGIN_HINT),
                          FONT_SMALL, GTextAlignmentLeft);
}

static void prv_window_unload(Window *window) {
  text_layer_destroy(s_title_layer);
  text_layer_destroy(s_open_layer);
  text_layer_destroy(s_url_layer);
  text_layer_destroy(s_enter_layer);
  text_layer_destroy(s_code_layer);
  text_layer_destroy(s_hint_layer);
  window_destroy(s_window);
  s_window = NULL;
}

void ui_login_show(const char *url, const char *code) {
  strncpy(s_url, url, sizeof(s_url) - 1);
  s_url[sizeof(s_url) - 1] = '\0';
  strncpy(s_code, code, sizeof(s_code) - 1);
  s_code[sizeof(s_code) - 1] = '\0';

  if (s_window) {
    layer_mark_dirty(window_get_root_layer(s_window));
    return;
  }
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  window_stack_push(s_window, true);
  vibes_short_pulse();
}

void ui_login_hide(void) {
  if (s_window) {
    window_stack_remove(s_window, true);
  }
}
