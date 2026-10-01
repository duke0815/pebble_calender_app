#pragma once

// Gemeinsame Konstanten des AppMessage-Protokolls (siehe CLAUDE.md).

// MSG_TYPE: Handy -> Uhr
#define MSG_EVENT        1
#define MSG_END          2
#define MSG_STATUS       3
#define MSG_LOGIN_CODE   4
#define MSG_SETTINGS     5
// MSG_TYPE: Uhr -> Handy
#define MSG_REQUEST      10

// STATUS_CODE
#define STATUS_SYNCING        1
#define STATUS_NEED_LOGIN     2
#define STATUS_ADMIN_CONSENT  3
#define STATUS_POLICY_BLOCKED 4
#define STATUS_NETWORK        5
#define STATUS_ERROR          6
#define STATUS_CONFIG         7

// Puffergrößen inkl. Nullterminator (JS kürzt auf 63 bzw. 39 Byte UTF-8)
#define TITLE_LEN     64
#define LOCATION_LEN  40
#define STATUS_LEN    80
#define LOGIN_URL_LEN 48
#define LOGIN_CODE_LEN 16

// Speicherbudget: ~116 Byte pro Termin, 4 KB Persist gesamt
#define MAX_EVENTS 16
