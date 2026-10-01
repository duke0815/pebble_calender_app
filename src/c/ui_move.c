#include "ui_move.h"
#include "ui_detail.h"
#include "comm.h"
#include "format.h"
#include "i18n.h"
#include "settings.h"
#include "theme.h"
#include "idle.h"

#if defined(PBL_PLATFORM_EMERY)
  #define FONT_TEXT FONT_KEY_GOTHIC_24_BOLD
  #define FONT_HINT FONT_KEY_GOTHIC_18
#else
  #define FONT_TEXT FONT_KEY_GOTHIC_18_BOLD
  #define FONT_HINT FONT_KEY_GOTHIC_14
#endif

#define PAD 8
#define SEND_RETRY_MS 250
#define SEND_TRIES 8
#define RESULT_TIMEOUT_MS 12000
#define CLOSE_AFTER_MS 1500

static const StrId OPTION_LABELS[MOVE_OPT_COUNT] = {
  STR_MOVE_OPT_HOUR, STR_MOVE_OPT_DAY, STR_MOVE_OPT_2DAYS, STR_MOVE_OPT_WEEK, STR_MOVE_OPT_MONDAY
};

static CalEvent s_event;
static int s_options[MOVE_OPT_COUNT];
static int s_option_count;
static int s_chosen;
static char s_targets[MOVE_OPT_COUNT][32];

// Selection
static Window *s_menu_window;
static MenuLayer *s_menu;

// Confirmation / result
static Window *s_confirm_window;
static TextLayer *s_text_layer;
static TextLayer *s_hint_layer;
static char s_text[96];
static bool s_sending;
static bool s_done;
static int s_tries;
static AppTimer *s_send_timer;
static AppTimer *s_timeout;
static AppTimer *s_close_timer;

// ---------------------------------------------------------------------------
// Compute the target (like lib.shiftEvent: days keep the local time of day)

static bool prv_option_fits(int opt) {
  if (!(settings_get()->move_opts & (1 << opt))) return false;
  return !(opt == MOVE_OPT_HOUR && s_event.all_day);
}

static time_t prv_target_start(int opt) {
  if (opt == MOVE_OPT_HOUR) return s_event.start + 3600;
  time_t start = s_event.start;
  struct tm *tm = localtime(&start);
  int days;
  if (opt == MOVE_OPT_MONDAY) {
    days = ((1 - tm->tm_wday + 6) % 7) + 1;  // 1..7 days until next Monday
  } else {
    days = opt == MOVE_OPT_DAY ? 1 : (opt == MOVE_OPT_2DAYS ? 2 : 7);
  }
  return time_make_local(start, days, tm->tm_hour, tm->tm_min);
}

static void prv_format_target(int opt, char *buf, size_t size) {
  time_t t = prv_target_start(opt);
  char date[16];
  format_date_short(t, date, sizeof(date));
  if (s_event.all_day) {
    snprintf(buf, size, "%s", date);
  } else {
    char clock[10];
    format_clock(t, clock, sizeof(clock));
    snprintf(buf, size, "%s %s", date, clock);
  }
}

bool ui_move_available(const CalEvent *event) {
  if (!(event->flags & EVT_FLAG_MOVABLE) || event->key == 0) return false;
  for (int opt = 0; opt < MOVE_OPT_COUNT; opt++) {
    if ((settings_get()->move_opts & (1 << opt)) && !(opt == MOVE_OPT_HOUR && event->all_day)) {
      return true;
    }
  }
  return false;
}

const char *ui_move_reason(const CalEvent *event) {
  if (ui_move_available(event)) return NULL;
  if (!(event->flags & EVT_FLAG_MOVE_ENABLED)) return NULL;  // feature off: no hint
  if (settings_get()->move_opts == 0) return tr(STR_MOVE_NO_SETTINGS);
  if (event->flags & EVT_FLAG_NOT_ORGANIZER) return tr(STR_MOVE_NOT_ORGANIZER);
  if (event->flags & EVT_FLAG_HAS_ATTENDEES) return tr(STR_MOVE_HAS_ATTENDEES);
  return tr(STR_MOVE_NO_OPTION);
}

// ---------------------------------------------------------------------------
// Confirmation, sending, result

static void prv_cancel_timers(void) {
  if (s_send_timer) { app_timer_cancel(s_send_timer); s_send_timer = NULL; }
  if (s_timeout) { app_timer_cancel(s_timeout); s_timeout = NULL; }
  if (s_close_timer) { app_timer_cancel(s_close_timer); s_close_timer = NULL; }
}

static void prv_set_text(const char *text, const char *hint) {
  snprintf(s_text, sizeof(s_text), "%s", text);
  if (s_text_layer) layer_mark_dirty(text_layer_get_layer(s_text_layer));
  if (s_hint_layer) text_layer_set_text(s_hint_layer, hint);
}

static void prv_close_all(void *data) {
  s_close_timer = NULL;
  if (s_confirm_window) window_stack_remove(s_confirm_window, false);
  if (s_menu_window) window_stack_remove(s_menu_window, false);
  ui_detail_close();
}

static void prv_timeout(void *data) {
  s_timeout = NULL;
  prv_cancel_timers();
  s_sending = false;
  s_done = true;
  prv_set_text(tr(STR_NO_PHONE), "");
}

static void prv_send(void *data) {
  s_send_timer = NULL;
  if (!s_sending) return;
  if (comm_request_move(s_event.key, s_chosen)) return;
  if (++s_tries < SEND_TRIES) {
    s_send_timer = app_timer_register(SEND_RETRY_MS, prv_send, NULL);
  }
}

static void prv_confirm_select(ClickRecognizerRef recognizer, void *context) {
  idle_kick();
  if (s_sending || s_done) return;
  if (!connection_service_peek_pebble_app_connection()) {
    s_done = true;
    prv_set_text(tr(STR_NO_PHONE), "");
    return;
  }
  s_sending = true;
  s_tries = 0;
  prv_set_text(tr(STR_MOVING), "");
  s_timeout = app_timer_register(RESULT_TIMEOUT_MS, prv_timeout, NULL);
  prv_send(NULL);
}

static void prv_confirm_back(ClickRecognizerRef recognizer, void *context) {
  if (s_sending) return;  // do not cancel while sending
  window_stack_remove(s_confirm_window, true);
}

static void prv_confirm_click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_confirm_select);
  window_single_click_subscribe(BUTTON_ID_BACK, prv_confirm_back);
}

static void prv_confirm_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  int16_t w = bounds.size.w - 2 * PAD;
  int16_t hint_h = 44;  // two lines of hint

  s_text_layer = text_layer_create(GRect(PAD, PAD, w, bounds.size.h - hint_h - 2 * PAD));
  text_layer_set_text(s_text_layer, s_text);
  text_layer_set_font(s_text_layer, fonts_get_system_font(FONT_TEXT));
  text_layer_set_text_alignment(s_text_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(s_text_layer, GTextOverflowModeWordWrap);
  text_layer_set_background_color(s_text_layer, GColorClear);
  text_layer_set_text_color(s_text_layer, theme_text());
  layer_add_child(root, text_layer_get_layer(s_text_layer));

  s_hint_layer = text_layer_create(GRect(PAD, bounds.size.h - hint_h - PAD / 2, w, hint_h));
  text_layer_set_text(s_hint_layer, tr(STR_MOVE_CONFIRM_HINT));
  text_layer_set_font(s_hint_layer, fonts_get_system_font(FONT_HINT));
  text_layer_set_text_alignment(s_hint_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_hint_layer, GColorClear);
  text_layer_set_text_color(s_hint_layer, theme_text());
  layer_add_child(root, text_layer_get_layer(s_hint_layer));
}

static void prv_confirm_unload(Window *window) {
  prv_cancel_timers();
  s_sending = false;
  text_layer_destroy(s_text_layer);
  text_layer_destroy(s_hint_layer);
  s_text_layer = NULL;
  s_hint_layer = NULL;
  window_destroy(s_confirm_window);
  s_confirm_window = NULL;
}

static void prv_show_confirm(int opt) {
  s_chosen = opt;
  s_sending = false;
  s_done = false;
  snprintf(s_text, sizeof(s_text), tr(STR_MOVE_CONFIRM), s_targets[opt]);
  s_confirm_window = window_create();
  window_set_background_color(s_confirm_window, theme_bg());
  window_set_click_config_provider(s_confirm_window, prv_confirm_click_config);
  window_set_window_handlers(s_confirm_window, (WindowHandlers) {
    .load = prv_confirm_load,
    .unload = prv_confirm_unload,
  });
  window_stack_push(s_confirm_window, true);
}

void ui_move_result(int32_t key, bool ok, const char *text) {
  if (!s_confirm_window || !s_sending || key != s_event.key) return;
  prv_cancel_timers();
  s_sending = false;
  s_done = true;
  if (ok) {
    char msg[96];
    snprintf(msg, sizeof(msg), tr(STR_MOVED), s_targets[s_chosen]);
    prv_set_text(msg, "");
    vibes_short_pulse();
    s_close_timer = app_timer_register(CLOSE_AFTER_MS, prv_close_all, NULL);
  } else {
    prv_set_text(text[0] ? text : tr(STR_MOVE_FAILED), "");
    vibes_double_pulse();
  }
}

// ---------------------------------------------------------------------------
// Selection

static uint16_t prv_num_rows(MenuLayer *menu, uint16_t section, void *ctx) {
  return s_option_count;
}

static int16_t prv_header_height(MenuLayer *menu, uint16_t section, void *ctx) {
  return MENU_CELL_BASIC_HEADER_HEIGHT;
}

static void prv_draw_header(GContext *ctx, const Layer *cell, uint16_t section, void *data) {
  menu_cell_basic_header_draw(ctx, cell, tr(STR_MOVE_TITLE));
}

static void prv_draw_row(GContext *ctx, const Layer *cell, MenuIndex *index, void *data) {
  int opt = s_options[index->row];
  menu_cell_basic_draw(ctx, cell, tr(OPTION_LABELS[opt]), s_targets[opt], NULL);
}

static void prv_selection_changed(MenuLayer *menu, MenuIndex new_index, MenuIndex old_index, void *data) {
  idle_kick();
}

static void prv_select(MenuLayer *menu, MenuIndex *index, void *data) {
  idle_kick();
  prv_show_confirm(s_options[index->row]);
}

static void prv_menu_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_menu = menu_layer_create(layer_get_bounds(root));
  menu_layer_set_callbacks(s_menu, NULL, (MenuLayerCallbacks) {
    .get_num_rows = prv_num_rows,
    .get_header_height = prv_header_height,
    .draw_header = prv_draw_header,
    .draw_row = prv_draw_row,
    .select_click = prv_select,
    .selection_changed = prv_selection_changed,
  });
  menu_layer_set_normal_colors(s_menu, theme_bg(), theme_text());
  menu_layer_set_highlight_colors(s_menu, theme_accent(), theme_on_accent());
  menu_layer_set_click_config_onto_window(s_menu, window);
  layer_add_child(root, menu_layer_get_layer(s_menu));
}

static void prv_menu_unload(Window *window) {
  menu_layer_destroy(s_menu);
  s_menu = NULL;
  window_destroy(s_menu_window);
  s_menu_window = NULL;
}

void ui_move_show(const CalEvent *event) {
  if (s_menu_window || !ui_move_available(event)) return;
  s_event = *event;
  s_option_count = 0;
  for (int opt = 0; opt < MOVE_OPT_COUNT; opt++) {
    if (prv_option_fits(opt)) {
      s_options[s_option_count++] = opt;
      prv_format_target(opt, s_targets[opt], sizeof(s_targets[opt]));
    }
  }
  s_menu_window = window_create();
  window_set_background_color(s_menu_window, theme_bg());
  window_set_window_handlers(s_menu_window, (WindowHandlers) {
    .load = prv_menu_load,
    .unload = prv_menu_unload,
  });
  window_stack_push(s_menu_window, true);
}
