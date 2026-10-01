#pragma once
#include <pebble.h>

typedef struct {
  // list_days >= 2: cache (today + tomorrow), otherwise a single day
  void (*list_updated)(bool complete, time_t list_day, int list_days);
  void (*status)(int code, const char *text);
  void (*login_code)(const char *url, const char *code);
  void (*settings_changed)(void);
  void (*send_failed)(void);
  void (*note)(int32_t key, const char *text);
  void (*move_result)(int32_t key, bool ok, const char *text);
} CommHandlers;

void comm_init(CommHandlers handlers);
void comm_deinit(void);

// Requests data: day = 0 -> cache (today + tomorrow), otherwise local midnight
// of a single day. false if it failed immediately.
bool comm_request(time_t day);
// Requests an event's description. false if it failed immediately.
bool comm_request_note(int32_t key);
// Move an event (opt = MOVE_OPT_*). false if it failed immediately.
bool comm_request_move(int32_t key, int opt);
