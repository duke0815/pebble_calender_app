#pragma once
#include <pebble.h>

// Language of the watch texts (Settings.lang)
#define LANG_DE 0
#define LANG_EN 1

typedef enum {
  STR_TODAY,
  STR_TOMORROW,
  STR_ALL_DAY,
  STR_NO_TITLE,
  STR_UPDATING,
  STR_LOADING_DAY,
  STR_LOADING_EVENTS,
  STR_OFFLINE,
  STR_OFFLINE_AS_OF,     // "%s" = time of the last sync
  STR_NO_PHONE,
  STR_NO_EVENTS_TODAY,
  STR_NO_EVENTS,
  STR_TRANSFER_INCOMPLETE,
  STR_ERROR,
  STR_LOGIN_TITLE,
  STR_LOGIN_OPEN,
  STR_LOGIN_ENTER,
  STR_LOGIN_HINT,
  STR_NOTES_LOADING,
  STR_NOTES_NEED_PHONE,
  STR_NOTES_NONE,
  STR_MOVE_HINT,
  STR_MOVE_TITLE,
  STR_MOVE_OPT_HOUR,
  STR_MOVE_OPT_DAY,
  STR_MOVE_OPT_2DAYS,
  STR_MOVE_OPT_WEEK,
  STR_MOVE_OPT_MONDAY,
  STR_MOVE_CONFIRM,      // "%s" = target
  STR_MOVE_CONFIRM_HINT,
  STR_MOVING,
  STR_MOVED,             // "%s" = target
  STR_MOVE_FAILED,
  STR_MOVE_NO_SETTINGS,
  STR_MOVE_NOT_ORGANIZER,
  STR_MOVE_HAS_ATTENDEES,
  STR_MOVE_NO_OPTION,
  STR_RESPOND_HINT,
  STR_RESPOND_TITLE,
  STR_RESP_ACCEPT,
  STR_RESP_TENTATIVE,
  STR_RESP_DECLINE,
  STR_RESP_CURRENT,
  STR_RESP_CONFIRM,      // "%s" = option (Accept/...)
  STR_RESPONDING,
  STR_RESPONDED_ACCEPT,
  STR_RESPONDED_TENTATIVE,
  STR_RESPONDED_DECLINE,
  STR_RESPOND_FAILED,
  STR_RESPONSE_STATUS,   // "%s" = response (Accepted/...)
  STR_RESPONSE_NONE,
  STR_COUNT
} StrId;

const char *tr(StrId id);
// Short weekday name, wday as in tm_wday (0 = Sunday)
const char *tr_weekday(int wday);
// Short month name (0 = January), used for English only
const char *tr_month(int mon);
// Default language from the watch's system language
uint8_t i18n_default_lang(void);
