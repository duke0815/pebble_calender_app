#pragma once
#include <pebble.h>

// Sprache der Uhr-Texte (Settings.lang)
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
  STR_OFFLINE_AS_OF,     // "%s" = Uhrzeit des letzten Syncs
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
  STR_MOVE_CONFIRM,      // "%s" = Ziel
  STR_MOVE_CONFIRM_HINT,
  STR_MOVING,
  STR_MOVED,             // "%s" = Ziel
  STR_MOVE_FAILED,
  STR_MOVE_NO_SETTINGS,
  STR_MOVE_NOT_ORGANIZER,
  STR_MOVE_HAS_ATTENDEES,
  STR_MOVE_NO_OPTION,
  STR_COUNT
} StrId;

const char *tr(StrId id);
// Kurzer Wochentag, wday wie tm_wday (0 = Sonntag)
const char *tr_weekday(int wday);
// Kurzer Monatsname (0 = Januar), nur Englisch genutzt
const char *tr_month(int mon);
// Standardsprache aus der Systemsprache der Uhr
uint8_t i18n_default_lang(void);
