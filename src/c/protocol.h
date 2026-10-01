#pragma once

// Shared constants of the AppMessage protocol (see CLAUDE.md).

// MSG_TYPE: phone -> watch
#define MSG_EVENT        1
#define MSG_END          2
#define MSG_STATUS       3
#define MSG_LOGIN_CODE   4
#define MSG_SETTINGS     5
#define MSG_NOTE         6
#define MSG_MOVE_RESULT  7
// MSG_TYPE: watch -> phone
#define MSG_REQUEST      10
#define MSG_NOTE_REQUEST 12
#define MSG_MOVE         13

// EVT_FLAGS
#define EVT_FLAG_MOVABLE       1
#define EVT_FLAG_NOT_ORGANIZER 2   // invitation from someone else
#define EVT_FLAG_HAS_ATTENDEES 4
#define EVT_FLAG_MOVE_ENABLED  8   // moving is enabled on the phone

// Move options (MOVE_OPT, bits in SET_MOVE_OPTS) - same as MOVE_OPTIONS in lib.js
#define MOVE_OPT_HOUR  0
#define MOVE_OPT_DAY   1
#define MOVE_OPT_2DAYS 2
#define MOVE_OPT_WEEK  3
#define MOVE_OPT_MONDAY 4   // next Monday (event on a Monday: Monday a week later)
#define MOVE_OPT_COUNT 5

// STATUS_CODE
#define STATUS_SYNCING        1
#define STATUS_NEED_LOGIN     2
#define STATUS_ADMIN_CONSENT  3
#define STATUS_POLICY_BLOCKED 4
#define STATUS_NETWORK        5
#define STATUS_ERROR          6
#define STATUS_CONFIG         7

// Buffer sizes incl. null terminator (JS truncates to 95 / 47 bytes UTF-8)
#define TITLE_LEN     96
#define LOCATION_LEN  48
#define STATUS_LEN    80
#define LOGIN_URL_LEN 48
#define LOGIN_CODE_LEN 16
// Description (JS truncates to 400 bytes), RAM only, for the detail view
#define NOTE_LEN      404

// Events per day in the view
#define MAX_DAY_EVENTS 12
// Persist cache (today + tomorrow): 160 bytes per event -> 16 * 160 = ~2.6 KB of 4 KB
#define MAX_CACHE_EVENTS 16
