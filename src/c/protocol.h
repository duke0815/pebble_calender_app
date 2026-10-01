#pragma once

// Gemeinsame Konstanten des AppMessage-Protokolls (siehe CLAUDE.md).

// MSG_TYPE: Handy -> Uhr
#define MSG_EVENT        1
#define MSG_END          2
#define MSG_STATUS       3
#define MSG_LOGIN_CODE   4
#define MSG_SETTINGS     5
#define MSG_NOTE         6
// MSG_TYPE: Uhr -> Handy
#define MSG_REQUEST      10
#define MSG_NOTE_REQUEST 12

// STATUS_CODE
#define STATUS_SYNCING        1
#define STATUS_NEED_LOGIN     2
#define STATUS_ADMIN_CONSENT  3
#define STATUS_POLICY_BLOCKED 4
#define STATUS_NETWORK        5
#define STATUS_ERROR          6
#define STATUS_CONFIG         7

// Puffergrößen inkl. Nullterminator (JS kürzt auf 95 bzw. 47 Byte UTF-8)
#define TITLE_LEN     96
#define LOCATION_LEN  48
#define STATUS_LEN    80
#define LOGIN_URL_LEN 48
#define LOGIN_CODE_LEN 16
// Beschreibung (JS kürzt auf 400 Byte), nur im RAM für die Detailansicht
#define NOTE_LEN      404

// Termine pro Tag in der Ansicht
#define MAX_DAY_EVENTS 12
// Persist-Cache (heute + morgen): 160 Byte pro Termin -> 16 * 160 = ~2,6 KB von 4 KB
#define MAX_CACHE_EVENTS 16
