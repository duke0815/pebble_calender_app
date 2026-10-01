#include "ui_main.h"
#include "ui_detail.h"
#include "events.h"
#include "format.h"
#include "i18n.h"
#include "theme.h"

#if defined(PBL_PLATFORM_EMERY)
  #define FONT_TIME   FONT_KEY_GOTHIC_18_BOLD
  #define FONT_TITLE  FONT_KEY_GOTHIC_24_BOLD
  #define FONT_LOC    FONT_KEY_GOTHIC_18
  #define H_TIME      20
  #define H_TITLE     28
  #define H_LOC       22
  #define STATUS_H    24
#else
  #define FONT_TIME   FONT_KEY_GOTHIC_14_BOLD
  #define FONT_TITLE  FONT_KEY_GOTHIC_18_BOLD
  #define FONT_LOC    FONT_KEY_GOTHIC_14
  #define H_TIME      16
  #define H_TITLE     22
  #define H_LOC       18
  #define STATUS_H    20
#endif

#define PAD_X        6
#define PAD_Y        2
#define HEADER_H     (H_TIME + 4)
#define INFO_ROW_H   (H_TITLE * 3 + 2 * PAD_Y)


static Window *s_window;
static TextLayer *s_status_layer;
static MenuLayer *s_menu_layer;
static UiMainCallbacks s_callbacks;

static char s_date_text[32];
static char s_status_text[64];
static char s_empty_text[96];

static void prv_interaction(void) {
  if (s_callbacks.interaction) s_callbacks.interaction();
}

// Eine Section: der angezeigte Tag. Ohne Termine eine Info-Zeile.
static bool prv_is_info(void) {
  return events_view_count() == 0;
}

// ---------------------------------------------------------------------------
// MenuLayer-Callbacks

static uint16_t prv_num_sections(MenuLayer *menu, void *ctx) {
  return 1;
}

static uint16_t prv_num_rows(MenuLayer *menu, uint16_t section, void *ctx) {
  return prv_is_info() ? 1 : events_view_count();
}

// Section-Header = Statuszeile, nur sichtbar, wenn es etwas zu melden gibt
static int16_t prv_header_height(MenuLayer *menu, uint16_t section, void *ctx) {
  return s_status_text[0] ? HEADER_H : 0;
}

static int16_t prv_cell_height(MenuLayer *menu, MenuIndex *index, void *ctx) {
  if (prv_is_info()) return INFO_ROW_H;
  const CalEvent *e = events_view_get(index->row);
  int16_t h = 2 * PAD_Y + H_TIME + H_TITLE;
  if (e && e->location[0]) h += H_LOC;
  return h;
}

static void prv_draw_header(GContext *ctx, const Layer *cell, uint16_t section, void *data) {
  if (!s_status_text[0]) return;
  GRect bounds = layer_get_bounds(cell);
  graphics_context_set_fill_color(ctx, theme_header_bg());
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  graphics_context_set_text_color(ctx, theme_header_text());
  graphics_draw_text(ctx, s_status_text, fonts_get_system_font(FONT_LOC),
                     GRect(PAD_X, -2, bounds.size.w - 2 * PAD_X, HEADER_H),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void prv_draw_event(GContext *ctx, const Layer *cell, const CalEvent *e) {
  GRect bounds = layer_get_bounds(cell);
  bool highlighted = menu_cell_layer_is_highlighted(cell);
  bool past = e->end <= time(NULL);
  GColor text_color = highlighted ? theme_on_accent() : (past ? theme_muted() : theme_text());
  graphics_context_set_text_color(ctx, text_color);

  int16_t w = bounds.size.w - 2 * PAD_X;
  int16_t y = PAD_Y;
  char time_text[24];
  format_event_time(e, time_text, sizeof(time_text));

  graphics_draw_text(ctx, time_text, fonts_get_system_font(FONT_TIME),
                     GRect(PAD_X, y - 2, w, H_TIME), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);
  y += H_TIME;
  graphics_draw_text(ctx, e->title[0] ? e->title : tr(STR_NO_TITLE), fonts_get_system_font(FONT_TITLE),
                     GRect(PAD_X, y - 4, w, H_TITLE), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);
  y += H_TITLE;
  if (e->location[0]) {
    graphics_draw_text(ctx, e->location, fonts_get_system_font(FONT_LOC),
                       GRect(PAD_X, y - 3, w, H_LOC), GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentLeft, NULL);
  }
}

static void prv_draw_info(GContext *ctx, const Layer *cell) {
  GRect bounds = layer_get_bounds(cell);
  graphics_context_set_text_color(ctx, menu_cell_layer_is_highlighted(cell) ? theme_on_accent() : theme_text());
  graphics_draw_text(ctx, s_empty_text, fonts_get_system_font(FONT_TITLE),
                     GRect(PAD_X, PAD_Y, bounds.size.w - 2 * PAD_X, bounds.size.h - 2 * PAD_Y),
                     GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

static void prv_draw_row(GContext *ctx, const Layer *cell, MenuIndex *index, void *data) {
  if (prv_is_info()) {
    prv_draw_info(ctx, cell);
    return;
  }
  const CalEvent *e = events_view_get(index->row);
  if (e) prv_draw_event(ctx, cell, e);
}

static void prv_select(MenuLayer *menu, MenuIndex *index, void *data) {
  prv_interaction();
  if (prv_is_info()) return;
  const CalEvent *e = events_view_get(index->row);
  if (e) ui_detail_show(e);
}

static void prv_selection_changed(MenuLayer *menu, MenuIndex new_index, MenuIndex old_index, void *data) {
  prv_interaction();
}

#if defined(PBL_TOUCH)
// Eigene Tastenbelegung (wie MenuLayer), damit "Zurück" abgefangen werden kann: Die
// Touch-Navigation meldet Wischen nach rechts als BACK, das soll die App nicht schließen.
static void prv_click_up(ClickRecognizerRef recognizer, void *context) {
  prv_interaction();
  menu_layer_set_selected_next(s_menu_layer, true, MenuRowAlignCenter, true);
}

static void prv_click_down(ClickRecognizerRef recognizer, void *context) {
  prv_interaction();
  menu_layer_set_selected_next(s_menu_layer, false, MenuRowAlignCenter, true);
}

static void prv_click_select(ClickRecognizerRef recognizer, void *context) {
  MenuIndex index = menu_layer_get_selected_index(s_menu_layer);
  prv_select(s_menu_layer, &index, NULL);
}

static void prv_click_back(ClickRecognizerRef recognizer, void *context) {
  if (s_callbacks.back) {
    s_callbacks.back();
  } else {
    window_stack_pop(true);
  }
}

static void prv_click_config(void *context) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 100, prv_click_up);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 100, prv_click_down);
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_click_select);
  window_single_click_subscribe(BUTTON_ID_BACK, prv_click_back);
}
#endif

// ---------------------------------------------------------------------------
// Fenster

static void prv_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_status_layer = text_layer_create(GRect(0, 0, bounds.size.w, STATUS_H));
  text_layer_set_font(s_status_layer, fonts_get_system_font(FONT_TIME));
  text_layer_set_text_alignment(s_status_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_status_layer, GTextOverflowModeTrailingEllipsis);
  text_layer_set_text(s_status_layer, s_date_text);
  layer_add_child(root, text_layer_get_layer(s_status_layer));

  s_menu_layer = menu_layer_create(GRect(0, STATUS_H, bounds.size.w, bounds.size.h - STATUS_H));
  menu_layer_set_callbacks(s_menu_layer, NULL, (MenuLayerCallbacks) {
    .get_num_sections = prv_num_sections,
    .get_num_rows = prv_num_rows,
    .get_header_height = prv_header_height,
    .get_cell_height = prv_cell_height,
    .draw_header = prv_draw_header,
    .draw_row = prv_draw_row,
    .select_click = prv_select,
    .selection_changed = prv_selection_changed,
  });
#if defined(PBL_TOUCH)
  window_set_click_config_provider(window, prv_click_config);
#else
  menu_layer_set_click_config_onto_window(s_menu_layer, window);
#endif
  layer_add_child(root, menu_layer_get_layer(s_menu_layer));
  ui_main_apply_theme();
}

static void prv_window_appear(Window *window) {
  prv_interaction();
}

static void prv_window_unload(Window *window) {
  menu_layer_destroy(s_menu_layer);
  text_layer_destroy(s_status_layer);
  s_menu_layer = NULL;
  s_status_layer = NULL;
}

void ui_main_apply_theme(void) {
  if (!s_window) return;
  window_set_background_color(s_window, theme_bg());
  if (s_status_layer) {
    text_layer_set_background_color(s_status_layer, theme_accent());
    text_layer_set_text_color(s_status_layer, theme_on_accent());
  }
  if (s_menu_layer) {
    menu_layer_set_normal_colors(s_menu_layer, theme_bg(), theme_text());
    menu_layer_set_highlight_colors(s_menu_layer, theme_accent(), theme_on_accent());
    layer_mark_dirty(menu_layer_get_layer(s_menu_layer));
  }
}

void ui_main_push(UiMainCallbacks callbacks) {
  s_callbacks = callbacks;
  snprintf(s_empty_text, sizeof(s_empty_text), "%s", tr(STR_LOADING_EVENTS));
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .appear = prv_window_appear,
    .unload = prv_window_unload,
  });
  events_rebuild_view();
  format_day(events_view_day(), s_date_text, sizeof(s_date_text));
  window_stack_push(s_window, true);
}

void ui_main_destroy(void) {
  if (s_window) {
    window_destroy(s_window);
    s_window = NULL;
  }
}

static void prv_update_date(void) {
  format_day(events_view_day(), s_date_text, sizeof(s_date_text));
  if (s_status_layer) {
    layer_mark_dirty(text_layer_get_layer(s_status_layer));
  }
}

void ui_main_reload(bool select_upcoming) {
  events_rebuild_view();
  prv_update_date();
  if (!s_menu_layer) return;
  menu_layer_reload_data(s_menu_layer);
  MenuIndex current = menu_layer_get_selected_index(s_menu_layer);
  bool out_of_range = current.row >= prv_num_rows(s_menu_layer, 0, NULL);
  if (select_upcoming || out_of_range) {
    MenuIndex index = { .section = 0, .row = 0 };
    events_view_first_upcoming(&index.row);
    menu_layer_set_selected_index(s_menu_layer, index, MenuRowAlignTop, false);
  }
}

bool ui_main_is_top(void) {
  return s_window && window_stack_get_top_window() == s_window;
}

void ui_main_set_status(const char *status, const char *empty_text) {
  bool had_status = s_status_text[0] != '\0';
  strncpy(s_status_text, status, sizeof(s_status_text) - 1);
  s_status_text[sizeof(s_status_text) - 1] = '\0';
  if (s_menu_layer && had_status != (s_status_text[0] != '\0')) {
    menu_layer_reload_data(s_menu_layer);  // Statuszeile ein-/ausblenden ändert die Header-Höhe
  }
  strncpy(s_empty_text, empty_text, sizeof(s_empty_text) - 1);
  s_empty_text[sizeof(s_empty_text) - 1] = '\0';
  prv_update_date();
  if (s_menu_layer) {
    layer_mark_dirty(menu_layer_get_layer(s_menu_layer));
  }
}
