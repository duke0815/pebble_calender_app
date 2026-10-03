#include "ui_detail.h"
#include "comm.h"
#include "format.h"
#include "i18n.h"
#include "theme.h"
#include "ui_action.h"
#include "idle.h"

#if defined(PBL_PLATFORM_EMERY)
  #define FONT_SMALL FONT_KEY_GOTHIC_18
  #define FONT_TIME  FONT_KEY_GOTHIC_18_BOLD
  #define FONT_TITLE FONT_KEY_GOTHIC_24_BOLD
#else
  #define FONT_SMALL FONT_KEY_GOTHIC_14
  #define FONT_TIME  FONT_KEY_GOTHIC_14_BOLD
  #define FONT_TITLE FONT_KEY_GOTHIC_18_BOLD
#endif

#define PAD 6
#define NOTES_GAP 8
#define NOTE_TIMEOUT_MS 8000
#define NOTE_SEND_RETRY_MS 250
#define NOTE_SEND_TRIES 8

enum { L_DATE, L_TIME, L_TITLE, L_LOCATION, L_RESPONSE, L_MOVE_HINT, L_NOTES, L_COUNT };

static Window *s_window;
static ScrollLayer *s_scroll;
static TextLayer *s_layers[L_COUNT];
static CalEvent s_event;
static char s_date[40];
static char s_time[24];

// Description: loaded from the phone when opened
static char s_note[NOTE_LEN];
static bool s_note_loading;
static int s_note_tries;
static AppTimer *s_note_send_timer;
static AppTimer *s_note_timeout;
static int16_t s_notes_y;
static int16_t s_width;

static int16_t prv_text_height(const char *text, const char *font_key, int16_t width) {
  GSize size = graphics_text_layout_get_content_size(text, fonts_get_system_font(font_key),
                                                     GRect(0, 0, width - 2 * PAD, 2000),
                                                     GTextOverflowModeWordWrap, GTextAlignmentLeft);
  return size.h + 6;
}

static int16_t prv_add_text(int idx, const char *text, const char *font_key, int16_t y) {
  int16_t h = prv_text_height(text, font_key, s_width);
  s_layers[idx] = text_layer_create(GRect(PAD, y, s_width - 2 * PAD, h));
  text_layer_set_text(s_layers[idx], text);
  text_layer_set_font(s_layers[idx], fonts_get_system_font(font_key));
  text_layer_set_overflow_mode(s_layers[idx], GTextOverflowModeWordWrap);
  text_layer_set_background_color(s_layers[idx], GColorClear);
  text_layer_set_text_color(s_layers[idx], theme_text());
  scroll_layer_add_child(s_scroll, text_layer_get_layer(s_layers[idx]));
  return y + h;
}

// Fit the description height to the current text
static void prv_layout_notes(void) {
  if (!s_layers[L_NOTES]) return;
  int16_t h = prv_text_height(s_note, FONT_SMALL, s_width);
  layer_set_frame(text_layer_get_layer(s_layers[L_NOTES]), GRect(PAD, s_notes_y, s_width - 2 * PAD, h));
  text_layer_set_text(s_layers[L_NOTES], s_note);
  scroll_layer_set_content_size(s_scroll, GSize(s_width, s_notes_y + h + PAD));
}

static void prv_set_note_text(const char *text) {
  snprintf(s_note, sizeof(s_note), "%s", text);
  prv_layout_notes();
}

static void prv_note_stop(void) {
  s_note_loading = false;
  if (s_note_send_timer) {
    app_timer_cancel(s_note_send_timer);
    s_note_send_timer = NULL;
  }
  if (s_note_timeout) {
    app_timer_cancel(s_note_timeout);
    s_note_timeout = NULL;
  }
}

static void prv_note_timeout(void *data) {
  s_note_timeout = NULL;
  prv_note_stop();
  prv_set_note_text(tr(STR_NOTES_NEED_PHONE));
}

// Send the request; if the outbox is busy, retry shortly after
static void prv_note_send(void *data) {
  s_note_send_timer = NULL;
  if (!s_note_loading) return;
  if (comm_request_note(s_event.key)) return;
  if (++s_note_tries < NOTE_SEND_TRIES) {
    s_note_send_timer = app_timer_register(NOTE_SEND_RETRY_MS, prv_note_send, NULL);
  }
}

static void prv_note_request(void) {
  if (!connection_service_peek_pebble_app_connection()) {
    snprintf(s_note, sizeof(s_note), "%s", tr(STR_NOTES_NEED_PHONE));
    return;
  }
  snprintf(s_note, sizeof(s_note), "%s", tr(STR_NOTES_LOADING));
  s_note_loading = true;
  s_note_tries = 0;
  s_note_timeout = app_timer_register(NOTE_TIMEOUT_MS, prv_note_timeout, NULL);
  prv_note_send(NULL);
}

// Select opens the action menu (move/respond, if allowed); up/down still scroll
static void prv_select_click(ClickRecognizerRef recognizer, void *context) {
  idle_kick();
  if (ui_action_available(&s_event)) {
    ui_action_show(&s_event);
  }
}

static void prv_scrolled(ScrollLayer *scroll_layer, void *context) {
  idle_kick();
}

static void prv_click_config(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click);
}

static void prv_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  s_width = bounds.size.w;

  s_scroll = scroll_layer_create(bounds);
  scroll_layer_set_callbacks(s_scroll, (ScrollLayerCallbacks) {
    .click_config_provider = prv_click_config,
    .content_offset_changed_handler = prv_scrolled,
  });
  scroll_layer_set_click_config_onto_window(s_scroll, window);
  scroll_layer_set_shadow_hidden(s_scroll, true);
  layer_add_child(root, scroll_layer_get_layer(s_scroll));

  time_t start_day = time_day_start(s_event.start);
  time_t last_day = s_event.all_day ? time_day_start(s_event.end - 1) : time_day_start(s_event.end);
  if (last_day > start_day) {
    char from[16];
    char to[16];
    format_date_short(s_event.start, from, sizeof(from));
    format_date_short(s_event.all_day ? s_event.end - 1 : s_event.end, to, sizeof(to));
    snprintf(s_date, sizeof(s_date), "%s - %s", from, to);
  } else {
    format_day(start_day, s_date, sizeof(s_date));
  }
  format_event_time(&s_event, s_time, sizeof(s_time));

  for (int i = 0; i < L_COUNT; i++) s_layers[i] = NULL;
  int16_t y = PAD;
  y = prv_add_text(L_DATE, s_date, FONT_SMALL, y);
  y = prv_add_text(L_TIME, s_time, FONT_TIME, y);
  y = prv_add_text(L_TITLE, s_event.title[0] ? s_event.title : tr(STR_NO_TITLE), FONT_TITLE, y);
  if (s_event.location[0]) {
    y = prv_add_text(L_LOCATION, s_event.location, FONT_SMALL, y);
  }
  if (ui_action_response_text(&s_event)) {
    y = prv_add_text(L_RESPONSE, ui_action_response_text(&s_event), FONT_SMALL, y);
  }
  if (ui_action_hint(&s_event)) {
    y = prv_add_text(L_MOVE_HINT, ui_action_hint(&s_event), FONT_SMALL, y + 2);
    text_layer_set_text_color(s_layers[L_MOVE_HINT], PBL_IF_COLOR_ELSE(theme_accent(), theme_text()));
  } else if (ui_action_reason(&s_event)) {
    // Moving is on, but not for this event -> show the reason
    y = prv_add_text(L_MOVE_HINT, ui_action_reason(&s_event), FONT_SMALL, y + 2);
    text_layer_set_text_color(s_layers[L_MOVE_HINT], theme_muted());
  }
  scroll_layer_set_content_size(s_scroll, GSize(s_width, y + PAD));

  if (s_event.key != 0) {
    s_notes_y = y + NOTES_GAP;
    prv_add_text(L_NOTES, s_note, FONT_SMALL, s_notes_y);
    prv_layout_notes();
  }
}

static void prv_window_unload(Window *window) {
  prv_note_stop();
  for (int i = 0; i < L_COUNT; i++) {
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
  s_note[0] = '\0';
  if (s_event.key != 0) {
    prv_note_request();
  }
  s_window = window_create();
  window_set_background_color(s_window, theme_bg());
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  window_stack_push(s_window, true);
}

void ui_detail_set_note(int32_t key, const char *text) {
  if (!s_window || key != s_event.key || !s_note_loading) return;
  prv_note_stop();
  prv_set_note_text(text[0] ? text : tr(STR_NOTES_NONE));
}

bool ui_detail_is_open(void) {
  return s_window != NULL;
}

void ui_detail_close(void) {
  if (s_window) {
    window_stack_remove(s_window, true);
  }
}
