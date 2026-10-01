#include "ui_main.h"
#include "ui_detail.h"
#include "events.h"
#include "format.h"

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

#define COLOR_ACCENT PBL_IF_COLOR_ELSE(GColorCobaltBlue, GColorBlack)
#define COLOR_BAR    PBL_IF_COLOR_ELSE(GColorOxfordBlue, GColorBlack)
#define COLOR_PAST   PBL_IF_COLOR_ELSE(GColorDarkGray, GColorBlack)

static Window *s_window;
static TextLayer *s_status_layer;
static MenuLayer *s_menu_layer;
static UiMainCallbacks s_callbacks;

static char s_status_text[64];
static char s_empty_text[96] = "Lade Termine...";

static void prv_interaction(void) {
  if (s_callbacks.interaction) s_callbacks.interaction();
}

// Sections: Termin-Tage, oder eine Info-Section, wenn nichts sichtbar ist
static bool prv_is_info_section(uint16_t section) {
  return events_view_num_sections() == 0 && section == 0;
}

// ---------------------------------------------------------------------------
// MenuLayer-Callbacks

static uint16_t prv_num_sections(MenuLayer *menu, void *ctx) {
  uint16_t n = events_view_num_sections();
  return n > 0 ? n : 1;
}

static uint16_t prv_num_rows(MenuLayer *menu, uint16_t section, void *ctx) {
  if (prv_is_info_section(section)) return 1;
  return events_view_num_rows(section);
}

static int16_t prv_header_height(MenuLayer *menu, uint16_t section, void *ctx) {
  return HEADER_H;
}

static int16_t prv_cell_height(MenuLayer *menu, MenuIndex *index, void *ctx) {
  if (prv_is_info_section(index->section)) return INFO_ROW_H;
  const CalEvent *e = events_view_get(index->section, index->row);
  int16_t h = 2 * PAD_Y + H_TIME + H_TITLE;
  if (e && e->location[0]) h += H_LOC;
  return h;
}

static void prv_draw_header(GContext *ctx, const Layer *cell, uint16_t section, void *data) {
  GRect bounds = layer_get_bounds(cell);
  char text[32];
  if (prv_is_info_section(section)) {
    format_day(time_day_start(time(NULL)), text, sizeof(text));
  } else {
    format_day(events_view_section_day(section), text, sizeof(text));
  }
  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorLightGray, GColorBlack));
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  graphics_context_set_text_color(ctx, PBL_IF_COLOR_ELSE(GColorBlack, GColorWhite));
  graphics_draw_text(ctx, text, fonts_get_system_font(FONT_TIME),
                     GRect(PAD_X, 0, bounds.size.w - 2 * PAD_X, HEADER_H),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

static void prv_draw_event(GContext *ctx, const Layer *cell, const CalEvent *e) {
  GRect bounds = layer_get_bounds(cell);
  bool highlighted = menu_cell_layer_is_highlighted(cell);
  bool past = e->end <= time(NULL);
  GColor text_color = highlighted ? GColorWhite : (past ? COLOR_PAST : GColorBlack);
  graphics_context_set_text_color(ctx, text_color);

  int16_t w = bounds.size.w - 2 * PAD_X;
  int16_t y = PAD_Y;
  char time_text[24];
  format_event_time(e, time_text, sizeof(time_text));

  graphics_draw_text(ctx, time_text, fonts_get_system_font(FONT_TIME),
                     GRect(PAD_X, y - 2, w, H_TIME), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);
  y += H_TIME;
  graphics_draw_text(ctx, e->title[0] ? e->title : "(ohne Titel)", fonts_get_system_font(FONT_TITLE),
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
  graphics_context_set_text_color(ctx, menu_cell_layer_is_highlighted(cell) ? GColorWhite : GColorBlack);
  graphics_draw_text(ctx, s_empty_text, fonts_get_system_font(FONT_TITLE),
                     GRect(PAD_X, PAD_Y, bounds.size.w - 2 * PAD_X, bounds.size.h - 2 * PAD_Y),
                     GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
}

static void prv_draw_row(GContext *ctx, const Layer *cell, MenuIndex *index, void *data) {
  if (prv_is_info_section(index->section)) {
    prv_draw_info(ctx, cell);
  } else {
    const CalEvent *e = events_view_get(index->section, index->row);
    if (e) prv_draw_event(ctx, cell, e);
  }
}

static void prv_select(MenuLayer *menu, MenuIndex *index, void *data) {
  prv_interaction();
  if (prv_is_info_section(index->section)) {
    return;
  }
  const CalEvent *e = events_view_get(index->section, index->row);
  if (e) ui_detail_show(e);
}

static void prv_selection_changed(MenuLayer *menu, MenuIndex new_index, MenuIndex old_index, void *data) {
  prv_interaction();
}

// ---------------------------------------------------------------------------
// Fenster

static void prv_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);

  s_status_layer = text_layer_create(GRect(0, 0, bounds.size.w, STATUS_H));
  text_layer_set_background_color(s_status_layer, COLOR_BAR);
  text_layer_set_text_color(s_status_layer, GColorWhite);
  text_layer_set_font(s_status_layer, fonts_get_system_font(FONT_TIME));
  text_layer_set_text_alignment(s_status_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_status_layer, GTextOverflowModeTrailingEllipsis);
  text_layer_set_text(s_status_layer, s_status_text);
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
  menu_layer_set_highlight_colors(s_menu_layer, COLOR_ACCENT, GColorWhite);
  menu_layer_set_click_config_onto_window(s_menu_layer, window);
  layer_add_child(root, menu_layer_get_layer(s_menu_layer));
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

void ui_main_push(UiMainCallbacks callbacks) {
  s_callbacks = callbacks;
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .appear = prv_window_appear,
    .unload = prv_window_unload,
  });
  events_rebuild_view();
  window_stack_push(s_window, true);
}

void ui_main_destroy(void) {
  if (s_window) {
    window_destroy(s_window);
    s_window = NULL;
  }
}

void ui_main_reload(bool select_upcoming) {
  events_rebuild_view();
  if (!s_menu_layer) return;
  menu_layer_reload_data(s_menu_layer);
  MenuIndex current = menu_layer_get_selected_index(s_menu_layer);
  bool out_of_range = current.section >= prv_num_sections(s_menu_layer, NULL) ||
                      current.row >= prv_num_rows(s_menu_layer, current.section, NULL);
  if (select_upcoming || out_of_range) {
    MenuIndex index = { .section = 0, .row = 0 };
    events_view_first_upcoming(&index.section, &index.row);
    menu_layer_set_selected_index(s_menu_layer, index, MenuRowAlignTop, false);
  }
}

void ui_main_set_status(const char *status, const char *empty_text) {
  strncpy(s_status_text, status, sizeof(s_status_text) - 1);
  s_status_text[sizeof(s_status_text) - 1] = '\0';
  strncpy(s_empty_text, empty_text, sizeof(s_empty_text) - 1);
  s_empty_text[sizeof(s_empty_text) - 1] = '\0';
  if (s_status_layer) {
    layer_mark_dirty(text_layer_get_layer(s_status_layer));
  }
  if (s_menu_layer) {
    layer_mark_dirty(menu_layer_get_layer(s_menu_layer));
  }
}
