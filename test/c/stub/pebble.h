/* Minimal stub of the Pebble SDK API (signatures as in SDK 3/4) for syntax checking only. */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

typedef struct Window Window;
typedef struct Layer Layer;
typedef struct TextLayer TextLayer;
typedef struct MenuLayer MenuLayer;
typedef struct ScrollLayer ScrollLayer;
typedef struct GContext GContext;
typedef struct GBitmap GBitmap;
typedef struct AppTimer AppTimer;
typedef struct GTextAttributes GTextAttributes;
typedef void *GFont;

typedef union { struct { uint8_t a:2, b:2, g:2, r:2; }; uint8_t argb; } GColor8;
typedef GColor8 GColor;
#define GColorBlack ((GColor8){.argb=0xC0})
#define GColorWhite ((GColor8){.argb=0xFF})
#define GColorClear ((GColor8){.argb=0x00})
#define GColorCobaltBlue ((GColor8){.argb=0xC6})
#define GColorOxfordBlue ((GColor8){.argb=0xC1})
#define GColorDarkGray ((GColor8){.argb=0xD5})
#define GColorLightGray ((GColor8){.argb=0xEA})
#define GColorFromRGB(red, green, blue) ((GColor8){.a = 3, .r = (uint8_t)(red) >> 6, .g = (uint8_t)(green) >> 6, .b = (uint8_t)(blue) >> 6})
#define GColorFromHEX(v) GColorFromRGB((((v) >> 16) & 0xff), (((v) >> 8) & 0xff), ((v) & 0xff))
#ifdef PBL_COLOR
#define PBL_IF_COLOR_ELSE(a,b) (a)
#else
#define PBL_IF_COLOR_ELSE(a,b) (b)
#endif

typedef struct { int16_t x, y; } GPoint;
typedef struct { int16_t w, h; } GSize;
typedef struct { GPoint origin; GSize size; } GRect;
#define GRect(x,y,w,h) ((GRect){{(x),(y)},{(w),(h)}})
#define GSize(w,h) ((GSize){(w),(h)})
typedef enum { GCornerNone = 0 } GCornerMask;
typedef enum { GTextAlignmentLeft, GTextAlignmentCenter, GTextAlignmentRight } GTextAlignment;
typedef enum { GTextOverflowModeWordWrap, GTextOverflowModeTrailingEllipsis, GTextOverflowModeFill } GTextOverflowMode;

#define FONT_KEY_GOTHIC_14 "RESOURCE_ID_GOTHIC_14"
#define FONT_KEY_GOTHIC_14_BOLD "RESOURCE_ID_GOTHIC_14_BOLD"
#define FONT_KEY_GOTHIC_18 "RESOURCE_ID_GOTHIC_18"
#define FONT_KEY_GOTHIC_18_BOLD "RESOURCE_ID_GOTHIC_18_BOLD"
#define FONT_KEY_GOTHIC_24_BOLD "RESOURCE_ID_GOTHIC_24_BOLD"
#define FONT_KEY_GOTHIC_28_BOLD "RESOURCE_ID_GOTHIC_28_BOLD"
GFont fonts_get_system_font(const char *font_key);

typedef void (*WindowHandler)(Window *window);
typedef struct { WindowHandler load, appear, disappear, unload; } WindowHandlers;
Window *window_create(void);
void window_destroy(Window *window);
void window_set_background_color(Window *window, GColor background_color);
void window_set_window_handlers(Window *window, WindowHandlers handlers);
Layer *window_get_root_layer(const Window *window);
void window_stack_push(Window *window, bool animated);
bool window_stack_remove(Window *window, bool animated);
Window *window_stack_get_top_window(void);
Window *window_stack_pop(bool animated);

typedef enum { BUTTON_ID_BACK = 0, BUTTON_ID_UP, BUTTON_ID_SELECT, BUTTON_ID_DOWN, NUM_BUTTONS } ButtonId;
typedef void *ClickRecognizerRef;
typedef void (*ClickHandler)(ClickRecognizerRef recognizer, void *context);
typedef void (*ClickConfigProvider)(void *context);
void window_set_click_config_provider(Window *window, ClickConfigProvider click_config_provider);
void window_single_click_subscribe(ButtonId button_id, ClickHandler handler);
void window_single_repeating_click_subscribe(ButtonId button_id, uint16_t repeat_interval_ms, ClickHandler handler);
void window_stack_pop_all(const bool animated);

GRect layer_get_bounds(const Layer *layer);
void layer_add_child(Layer *parent, Layer *child);
void layer_mark_dirty(Layer *layer);

TextLayer *text_layer_create(GRect frame);
void text_layer_destroy(TextLayer *text_layer);
Layer *text_layer_get_layer(TextLayer *text_layer);
void text_layer_set_text(TextLayer *text_layer, const char *text);
void text_layer_set_font(TextLayer *text_layer, GFont font);
void text_layer_set_text_alignment(TextLayer *text_layer, GTextAlignment alignment);
void text_layer_set_overflow_mode(TextLayer *text_layer, GTextOverflowMode line_mode);
void text_layer_set_background_color(TextLayer *text_layer, GColor color);
void text_layer_set_text_color(TextLayer *text_layer, GColor color);

ScrollLayer *scroll_layer_create(GRect frame);
void scroll_layer_destroy(ScrollLayer *scroll_layer);
Layer *scroll_layer_get_layer(const ScrollLayer *scroll_layer);
void scroll_layer_add_child(ScrollLayer *scroll_layer, Layer *child);
void scroll_layer_set_click_config_onto_window(ScrollLayer *scroll_layer, Window *window);
void scroll_layer_set_content_size(ScrollLayer *scroll_layer, GSize size);
void scroll_layer_set_shadow_hidden(ScrollLayer *scroll_layer, bool hidden);
void scroll_layer_set_content_offset(ScrollLayer *scroll_layer, GPoint offset, bool animated);
GPoint scroll_layer_get_content_offset(ScrollLayer *scroll_layer);

typedef struct { uint16_t section; uint16_t row; } MenuIndex;
typedef enum { MenuRowAlignNone, MenuRowAlignCenter, MenuRowAlignTop, MenuRowAlignBottom } MenuRowAlign;
typedef uint16_t (*MenuLayerGetNumberOfSectionsCallback)(MenuLayer *menu_layer, void *callback_context);
typedef uint16_t (*MenuLayerGetNumberOfRowsInSectionsCallback)(MenuLayer *menu_layer, uint16_t section_index, void *callback_context);
typedef int16_t (*MenuLayerGetCellHeightCallback)(MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context);
typedef int16_t (*MenuLayerGetHeaderHeightCallback)(MenuLayer *menu_layer, uint16_t section_index, void *callback_context);
typedef void (*MenuLayerDrawRowCallback)(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *callback_context);
typedef void (*MenuLayerDrawHeaderCallback)(GContext *ctx, const Layer *cell_layer, uint16_t section_index, void *callback_context);
typedef void (*MenuLayerSelectCallback)(MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context);
typedef void (*MenuLayerSelectionChangedCallback)(MenuLayer *menu_layer, MenuIndex new_index, MenuIndex old_index, void *callback_context);
typedef struct {
  MenuLayerGetNumberOfSectionsCallback get_num_sections;
  MenuLayerGetNumberOfRowsInSectionsCallback get_num_rows;
  MenuLayerGetCellHeightCallback get_cell_height;
  MenuLayerGetHeaderHeightCallback get_header_height;
  MenuLayerDrawRowCallback draw_row;
  MenuLayerDrawHeaderCallback draw_header;
  MenuLayerSelectCallback select_click;
  MenuLayerSelectCallback select_long_click;
  MenuLayerSelectionChangedCallback selection_changed;
} MenuLayerCallbacks;
MenuLayer *menu_layer_create(GRect frame);
void menu_layer_destroy(MenuLayer *menu_layer);
Layer *menu_layer_get_layer(const MenuLayer *menu_layer);
void menu_layer_set_callbacks(MenuLayer *menu_layer, void *callback_context, MenuLayerCallbacks callbacks);
void menu_layer_set_click_config_onto_window(MenuLayer *menu_layer, Window *window);
void menu_layer_reload_data(MenuLayer *menu_layer);
MenuIndex menu_layer_get_selected_index(const MenuLayer *menu_layer);
void menu_layer_set_selected_index(MenuLayer *menu_layer, MenuIndex index, MenuRowAlign scroll_align, bool animated);
void menu_layer_set_selected_next(MenuLayer *menu_layer, bool up, MenuRowAlign scroll_align, bool animated);
void menu_layer_set_normal_colors(MenuLayer *menu_layer, GColor background, GColor foreground);
void menu_layer_set_highlight_colors(MenuLayer *menu_layer, GColor background, GColor foreground);
bool menu_cell_layer_is_highlighted(const Layer *cell_layer);
void menu_cell_basic_draw(GContext *ctx, const Layer *cell_layer, const char *title, const char *subtitle, GBitmap *icon);

void graphics_context_set_fill_color(GContext *ctx, GColor color);
void graphics_context_set_text_color(GContext *ctx, GColor color);
void graphics_fill_rect(GContext *ctx, GRect rect, uint16_t corner_radius, GCornerMask corner_mask);
void graphics_draw_text(GContext *ctx, const char *text, GFont const font, const GRect box, const GTextOverflowMode overflow_mode, const GTextAlignment alignment, GTextAttributes *text_attributes);
GSize graphics_text_layout_get_content_size(const char *text, GFont const font, const GRect box, const GTextOverflowMode overflow_mode, const GTextAlignment alignment);

typedef enum { TUPLE_BYTE_ARRAY = 0, TUPLE_CSTRING = 1, TUPLE_UINT = 2, TUPLE_INT = 3 } TupleType;
typedef struct __attribute__((__packed__)) {
  uint32_t key; TupleType type:8; uint16_t length;
  union { uint8_t data[0]; char cstring[0]; uint8_t uint8; uint16_t uint16; uint32_t uint32; int8_t int8; int16_t int16; int32_t int32; } value[];
} Tuple;
typedef struct DictionaryIterator DictionaryIterator;
typedef enum { DICT_OK = 0 } DictionaryResult;
Tuple *dict_find(const DictionaryIterator *iter, const uint32_t key);
DictionaryResult dict_write_int32(DictionaryIterator *iter, const uint32_t key, const int32_t value);

typedef enum { APP_MSG_OK = 0 } AppMessageResult;
typedef void (*AppMessageInboxReceived)(DictionaryIterator *iterator, void *context);
typedef void (*AppMessageInboxDropped)(AppMessageResult reason, void *context);
typedef void (*AppMessageOutboxFailed)(DictionaryIterator *iterator, AppMessageResult reason, void *context);
AppMessageInboxReceived app_message_register_inbox_received(AppMessageInboxReceived cb);
AppMessageInboxDropped app_message_register_inbox_dropped(AppMessageInboxDropped cb);
AppMessageOutboxFailed app_message_register_outbox_failed(AppMessageOutboxFailed cb);
void app_message_deregister_callbacks(void);
AppMessageResult app_message_open(const uint32_t size_inbound, const uint32_t size_outbound);
AppMessageResult app_message_outbox_begin(DictionaryIterator **iterator);
AppMessageResult app_message_outbox_send(void);

extern uint32_t MESSAGE_KEY_MSG_TYPE, MESSAGE_KEY_EVT_INDEX, MESSAGE_KEY_EVT_COUNT, MESSAGE_KEY_EVT_START,
  MESSAGE_KEY_EVT_END, MESSAGE_KEY_EVT_ALLDAY, MESSAGE_KEY_EVT_TITLE, MESSAGE_KEY_EVT_LOCATION,
  MESSAGE_KEY_STATUS_CODE, MESSAGE_KEY_STATUS_TEXT, MESSAGE_KEY_LOGIN_URL, MESSAGE_KEY_LOGIN_CODE,
  MESSAGE_KEY_SET_BRIEF_ENABLED, MESSAGE_KEY_SET_BRIEF_HOUR, MESSAGE_KEY_SET_BRIEF_MINUTE,
  MESSAGE_KEY_SET_BRIEF_DAYS, MESSAGE_KEY_REQ_DAY, MESSAGE_KEY_LIST_DAY, MESSAGE_KEY_LIST_DAYS,
  MESSAGE_KEY_SET_LANG, MESSAGE_KEY_SET_COLOR_ACCENT, MESSAGE_KEY_SET_COLOR_TEXT, MESSAGE_KEY_SET_COLOR_BG;

typedef int32_t status_t;
typedef enum { S_SUCCESS = 0, E_ERROR = -1, E_RANGE = -8 } StatusCode;
bool persist_exists(const uint32_t key);
int32_t persist_read_int(const uint32_t key);
status_t persist_write_int(const uint32_t key, const int32_t value);
int persist_read_data(const uint32_t key, void *buffer, const size_t buffer_size);
int persist_write_data(const uint32_t key, const void *data, const size_t size);
status_t persist_delete(const uint32_t key);

typedef int32_t WakeupId;
typedef void (*WakeupHandler)(WakeupId wakeup_id, int32_t cookie);
WakeupId wakeup_schedule(time_t timestamp, int32_t cookie, bool notify_if_missed);
void wakeup_cancel_all(void);
void wakeup_service_subscribe(WakeupHandler handler);
typedef enum { APP_LAUNCH_SYSTEM, APP_LAUNCH_USER, APP_LAUNCH_PHONE, APP_LAUNCH_WAKEUP } AppLaunchReason;
AppLaunchReason launch_reason(void);

typedef void (*AppTimerCallback)(void *data);
AppTimer *app_timer_register(uint32_t timeout_ms, AppTimerCallback callback, void *callback_data);
bool app_timer_reschedule(AppTimer *timer_handle, uint32_t new_timeout_ms);
void app_timer_cancel(AppTimer *timer_handle);

typedef enum { SECOND_UNIT = 1, MINUTE_UNIT = 2 } TimeUnits;
typedef void (*TickHandler)(struct tm *tick_time, TimeUnits units_changed);
void tick_timer_service_subscribe(TimeUnits tick_units, TickHandler handler);
void tick_timer_service_unsubscribe(void);

void vibes_short_pulse(void);

/* Touch (PebbleOS: fw/applib/touch_service.h, pbl/services/touch/touch_event.h) */
#ifdef PBL_TOUCH
typedef enum TouchEventType { TouchEvent_Touchdown, TouchEvent_Liftoff, TouchEvent_PositionUpdate } TouchEventType;
typedef struct TouchEvent { TouchEventType type : 8; bool non_navigational; int16_t x; int16_t y; } TouchEvent;
typedef void (*TouchServiceHandler)(const TouchEvent *event, void *context);
void touch_service_subscribe(TouchServiceHandler handler, void *context);
void touch_service_unsubscribe(void);
bool touch_service_is_enabled(void);
void app_touch_navigation_enable(bool enable);
#endif
bool connection_service_peek_pebble_app_connection(void);
bool clock_is_24h_style(void);
const char *i18n_get_system_locale(void);
uint16_t time_ms(time_t *t_utc, uint16_t *out_ms);
void app_event_loop(void);

typedef enum { APP_LOG_LEVEL_ERROR = 1, APP_LOG_LEVEL_WARNING = 50, APP_LOG_LEVEL_INFO = 100, APP_LOG_LEVEL_DEBUG = 200 } AppLogLevel;
void app_log(uint8_t log_level, const char *src_filename, int src_line_number, const char *fmt, ...) __attribute__((format(printf, 4, 5)));
#define APP_LOG(level, fmt, args...) app_log(level, __FILE__, __LINE__, fmt, ## args)
