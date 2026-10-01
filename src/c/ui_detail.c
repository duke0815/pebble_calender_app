#include "ui_detail.h"
#include "format.h"

#if defined(PBL_PLATFORM_EMERY)
  #define FONT_SMALL FONT_KEY_GOTHIC_18
  #define FONT_TIME  FONT_KEY_GOTHIC_24_BOLD
  #define FONT_TITLE FONT_KEY_GOTHIC_28_BOLD
#else
  #define FONT_SMALL FONT_KEY_GOTHIC_14
  #define FONT_TIME  FONT_KEY_GOTHIC_18_BOLD
  #define FONT_TITLE FONT_KEY_GOTHIC_24_BOLD
#endif

#define PAD 6
#define TEXT_COUNT 4

static Window *s_window;
static ScrollLayer *s_scroll;
static TextLayer *s_layers[TEXT_COUNT];
static CalEvent s_event;
static char s_date[24];
static char s_time[24];

static int16_t prv_add_text(int idx, const char *text, const char *font_key,
                            int16_t y, int16_t width) {
  GFont font = fonts_get_system_font(font_key);
  GRect box = GRect(PAD, y, width - 2 * PAD, 2000);
  GSize size = graphics_text_layout_get_content_size(text, font, box, GTextOverflowModeWordWrap,
                                                     GTextAlignmentLeft);
  box.size.h = size.h + 6;
  s_layers[idx] = text_layer_create(box);
  text_layer_set_text(s_layers[idx], text);
  text_layer_set_font(s_layers[idx], font);
  text_layer_set_overflow_mode(s_layers[idx], GTextOverflowModeWordWrap);
  text_layer_set_background_color(s_layers[idx], GColorClear);
  scroll_layer_add_child(s_scroll, text_layer_get_layer(s_layers[idx]));
  return y + box.size.h;
}

static void prv_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_scroll = scroll_layer_create(bounds);
  scroll_layer_set_click_config_onto_window(s_scroll, window);
  scroll_layer_set_shadow_hidden(s_scroll, true);
  layer_add_child(root, scroll_layer_get_layer(s_scroll));

  time_t start_day = time_day_start(s_event.start);
  time_t last_day = s_event.all_day ? time_day_start(s_event.end - 1) : time_day_start(s_event.end);
  if (last_day > start_day) {
    char from[12];
    char to[12];
    format_date_short(s_event.start, from, sizeof(from));
    format_date_short(s_event.all_day ? s_event.end - 1 : s_event.end, to, sizeof(to));
    snprintf(s_date, sizeof(s_date), "%s - %s", from, to);
  } else {
    format_day(start_day, s_date, sizeof(s_date));
  }
  format_event_time(&s_event, s_time, sizeof(s_time));

  int16_t y = PAD;
  int16_t w = bounds.size.w;
  for (int i = 0; i < TEXT_COUNT; i++) s_layers[i] = NULL;
  y = prv_add_text(0, s_date, FONT_SMALL, y, w);
  y = prv_add_text(1, s_time, FONT_TIME, y, w);
  y = prv_add_text(2, s_event.title[0] ? s_event.title : "(ohne Titel)",
                   FONT_TITLE, y, w);
  if (s_event.location[0]) {
    y = prv_add_text(3, s_event.location, FONT_SMALL, y, w);
  }
  scroll_layer_set_content_size(s_scroll, GSize(w, y + PAD));
}

static void prv_window_unload(Window *window) {
  for (int i = 0; i < TEXT_COUNT; i++) {
    if (s_layers[i]) {
      text_layer_destroy(s_layers[i]);
      s_layers[i] = NULL;
    }
  }
  scroll_layer_destroy(s_scroll);
  s_scroll = NULL;
  window_destroy(s_window);
  s_window = NULL;
}

void ui_detail_show(const CalEvent *event) {
  if (s_window) return;
  s_event = *event;
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  window_stack_push(s_window, true);
}
