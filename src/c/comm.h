#pragma once
#include <pebble.h>

typedef struct {
  void (*list_updated)(bool complete);
  void (*status)(int code, const char *text);
  void (*login_code)(const char *url, const char *code);
  void (*settings_changed)(void);
  void (*send_failed)(void);
} CommHandlers;

void comm_init(CommHandlers handlers);
void comm_deinit(void);

// Sendet MSG_REQUEST bzw. MSG_LOGIN ans Handy. false, wenn sofort fehlgeschlagen.
bool comm_send(int msg_type);
