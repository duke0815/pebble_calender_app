#include "comm.h"
#include "events.h"
#include "settings.h"
#include "protocol.h"
#include "i18n.h"

static CommHandlers s_handlers;

static int32_t prv_int(DictionaryIterator *iter, uint32_t key, int32_t fallback) {
  Tuple *t = dict_find(iter, key);
  if (!t) {
    return fallback;
  }
  switch (t->type) {
    case TUPLE_INT:
    case TUPLE_UINT:
      if (t->length == 1) return t->type == TUPLE_INT ? t->value->int8 : t->value->uint8;
      if (t->length == 2) return t->type == TUPLE_INT ? t->value->int16 : t->value->uint16;
      return t->value->int32;
    default:
      return fallback;
  }
}

static void prv_str(DictionaryIterator *iter, uint32_t key, char *buffer, size_t size) {
  buffer[0] = '\0';
  Tuple *t = dict_find(iter, key);
  if (t && t->type == TUPLE_CSTRING) {
    strncpy(buffer, t->value->cstring, size - 1);
    buffer[size - 1] = '\0';
  }
}

static void prv_handle_event(DictionaryIterator *iter) {
  CalEvent event;
  memset(&event, 0, sizeof(event));
  int index = prv_int(iter, MESSAGE_KEY_EVT_INDEX, -1);
  event.start = prv_int(iter, MESSAGE_KEY_EVT_START, 0);
  event.end = prv_int(iter, MESSAGE_KEY_EVT_END, 0);
  event.all_day = prv_int(iter, MESSAGE_KEY_EVT_ALLDAY, 0) ? 1 : 0;
  event.key = prv_int(iter, MESSAGE_KEY_EVT_KEY, 0);
  prv_str(iter, MESSAGE_KEY_EVT_TITLE, event.title, sizeof(event.title));
  prv_str(iter, MESSAGE_KEY_EVT_LOCATION, event.location, sizeof(event.location));
  events_receive(index, &event);
}

static void prv_handle_settings(DictionaryIterator *iter) {
  Settings *s = settings_get();
  s->brief_enabled = prv_int(iter, MESSAGE_KEY_SET_BRIEF_ENABLED, s->brief_enabled) ? 1 : 0;
  s->brief_hour = prv_int(iter, MESSAGE_KEY_SET_BRIEF_HOUR, s->brief_hour) % 24;
  s->brief_minute = prv_int(iter, MESSAGE_KEY_SET_BRIEF_MINUTE, s->brief_minute) % 60;
  s->brief_days = prv_int(iter, MESSAGE_KEY_SET_BRIEF_DAYS, s->brief_days) & 0x7F;
  s->lang = prv_int(iter, MESSAGE_KEY_SET_LANG, s->lang) == LANG_EN ? LANG_EN : LANG_DE;
  s->touch_off = prv_int(iter, MESSAGE_KEY_SET_TOUCH, !s->touch_off) ? 0 : 1;
  s->color_accent = (uint32_t)prv_int(iter, MESSAGE_KEY_SET_COLOR_ACCENT, s->color_accent) & 0xFFFFFF;
  s->color_text = (uint32_t)prv_int(iter, MESSAGE_KEY_SET_COLOR_TEXT, s->color_text) & 0xFFFFFF;
  s->color_bg = (uint32_t)prv_int(iter, MESSAGE_KEY_SET_COLOR_BG, s->color_bg) & 0xFFFFFF;
  settings_save();
  settings_schedule_wakeup();
}

static void prv_inbox_received(DictionaryIterator *iter, void *context) {
  int type = prv_int(iter, MESSAGE_KEY_MSG_TYPE, 0);
  switch (type) {
    case MSG_EVENT:
      prv_handle_event(iter);
      break;
    case MSG_END: {
      time_t list_day = prv_int(iter, MESSAGE_KEY_LIST_DAY, 0);
      int list_days = prv_int(iter, MESSAGE_KEY_LIST_DAYS, 1);
      bool complete = events_commit(prv_int(iter, MESSAGE_KEY_EVT_COUNT, 0), list_day, list_days);
      if (s_handlers.list_updated) s_handlers.list_updated(complete, list_day, list_days);
      break;
    }
    case MSG_STATUS: {
      static char text[STATUS_LEN];
      prv_str(iter, MESSAGE_KEY_STATUS_TEXT, text, sizeof(text));
      if (s_handlers.status) s_handlers.status(prv_int(iter, MESSAGE_KEY_STATUS_CODE, STATUS_ERROR), text);
      break;
    }
    case MSG_LOGIN_CODE: {
      static char url[LOGIN_URL_LEN];
      static char code[LOGIN_CODE_LEN];
      prv_str(iter, MESSAGE_KEY_LOGIN_URL, url, sizeof(url));
      prv_str(iter, MESSAGE_KEY_LOGIN_CODE, code, sizeof(code));
      if (s_handlers.login_code) s_handlers.login_code(url, code);
      break;
    }
    case MSG_NOTE: {
      static char note[NOTE_LEN];
      prv_str(iter, MESSAGE_KEY_NOTE_TEXT, note, sizeof(note));
      if (s_handlers.note) s_handlers.note(prv_int(iter, MESSAGE_KEY_EVT_KEY, 0), note);
      break;
    }
    case MSG_SETTINGS:
      prv_handle_settings(iter);
      if (s_handlers.settings_changed) s_handlers.settings_changed();
      break;
    default:
      APP_LOG(APP_LOG_LEVEL_WARNING, "Unbekannter MSG_TYPE %d", type);
  }
}

static void prv_inbox_dropped(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "Nachricht verworfen: %d", (int)reason);
}

static void prv_outbox_failed(DictionaryIterator *iter, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "Senden fehlgeschlagen: %d", (int)reason);
  if (s_handlers.send_failed) s_handlers.send_failed();
}

void comm_init(CommHandlers handlers) {
  s_handlers = handlers;
  app_message_register_inbox_received(prv_inbox_received);
  app_message_register_inbox_dropped(prv_inbox_dropped);
  app_message_register_outbox_failed(prv_outbox_failed);
  app_message_open(640, 64);
}

void comm_deinit(void) {
  app_message_deregister_callbacks();
}

bool comm_request_note(int32_t key) {
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK) {
    return false;
  }
  dict_write_int32(iter, MESSAGE_KEY_MSG_TYPE, MSG_NOTE_REQUEST);
  dict_write_int32(iter, MESSAGE_KEY_EVT_KEY, key);
  return app_message_outbox_send() == APP_MSG_OK;
}

bool comm_request(time_t day) {
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK) {
    return false;
  }
  dict_write_int32(iter, MESSAGE_KEY_MSG_TYPE, MSG_REQUEST);
  dict_write_int32(iter, MESSAGE_KEY_REQ_DAY, (int32_t)day);
  return app_message_outbox_send() == APP_MSG_OK;
}
