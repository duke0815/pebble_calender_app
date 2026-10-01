#include <pebble.h>
#include "protocol.h"
#include "events.h"
#include "settings.h"
#include "comm.h"
#include "format.h"
#include "ui_main.h"
#include "ui_login.h"

#define SYNC_TIMEOUT_MS        30000
#define WAKEUP_SYNC_TIMEOUT_MS 10000
#define IDLE_EXIT_MS           60000

typedef enum {
  STATE_IDLE,      // Daten aktuell (oder noch nie synchronisiert)
  STATE_SYNCING,
  STATE_OFFLINE,   // Handy/Internet nicht erreichbar -> Cache
  STATE_ERROR,     // Fehlertext vom Handy
} SyncState;

static SyncState s_state = STATE_IDLE;
static char s_error[STATUS_LEN];
static bool s_login_active;
static bool s_wakeup_launch;
static AppTimer *s_sync_timer;
static AppTimer *s_idle_timer;

// ---------------------------------------------------------------------------
// Statusanzeige

static void prv_update_status(void) {
  char status[64];
  char stamp[24];
  time_t last = events_last_sync();
  stamp[0] = '\0';
  if (last) {
    char clock[10];
    format_clock(last, clock, sizeof(clock));
    if (time_day_start(last) == time_day_start(time(NULL))) {
      snprintf(stamp, sizeof(stamp), "%s", clock);
    } else {
      struct tm *tm = localtime(&last);
      snprintf(stamp, sizeof(stamp), "%02d.%02d. %s", tm->tm_mday, tm->tm_mon + 1, clock);
    }
  }

  switch (s_state) {
    case STATE_SYNCING:
      snprintf(status, sizeof(status), "Aktualisiere...");
      break;
    case STATE_OFFLINE:
      if (last) {
        snprintf(status, sizeof(status), "Offline - Stand %s", stamp);
      } else {
        snprintf(status, sizeof(status), "Offline");
      }
      break;
    case STATE_ERROR:
      snprintf(status, sizeof(status), "%s", s_error);
      break;
    default:
      if (last) {
        snprintf(status, sizeof(status), "Stand %s", stamp);
      } else {
        status[0] = '\0';
      }
  }

  const char *empty;
  if (s_state == STATE_ERROR) {
    empty = s_error;
  } else if (!last && s_state == STATE_SYNCING) {
    empty = "Lade Termine...";
  } else if (!last && s_state == STATE_OFFLINE) {
    empty = "Keine Verbindung zum Handy.";
  } else {
    empty = settings_get()->mode == MODE_TODAY ? "Keine Termine heute" : "Keine anstehenden Termine";
  }
  ui_main_set_status(status, empty);
}

static void prv_set_state(SyncState state) {
  s_state = state;
  prv_update_status();
}

// ---------------------------------------------------------------------------
// Synchronisierung

static void prv_cancel_sync_timer(void) {
  if (s_sync_timer) {
    app_timer_cancel(s_sync_timer);
    s_sync_timer = NULL;
  }
}

static void prv_sync_timeout(void *data) {
  s_sync_timer = NULL;
  if (s_state == STATE_SYNCING && !s_login_active) {
    prv_set_state(STATE_OFFLINE);
  }
}

static void prv_arm_sync_timer(uint32_t timeout_ms) {
  prv_cancel_sync_timer();
  s_sync_timer = app_timer_register(timeout_ms, prv_sync_timeout, NULL);
}

static void prv_send(int msg_type, uint32_t timeout_ms) {
  if (!connection_service_peek_pebble_app_connection() || !comm_send(msg_type)) {
    prv_cancel_sync_timer();
    prv_set_state(STATE_OFFLINE);
    return;
  }
  prv_set_state(STATE_SYNCING);
  prv_arm_sync_timer(timeout_ms);
}

static void prv_request_refresh(void) {
  prv_send(MSG_REQUEST, s_wakeup_launch ? WAKEUP_SYNC_TIMEOUT_MS : SYNC_TIMEOUT_MS);
}

static void prv_request_login(void) {
  prv_send(MSG_LOGIN, SYNC_TIMEOUT_MS);
}

// ---------------------------------------------------------------------------
// Inaktivität (nur beim Start durch das Briefing)

static void prv_idle_exit(void *data) {
  s_idle_timer = NULL;
  window_stack_pop_all(false);
}

static void prv_interaction(void) {
  if (!s_wakeup_launch) return;
  if (s_idle_timer) {
    app_timer_reschedule(s_idle_timer, IDLE_EXIT_MS);
  } else {
    s_idle_timer = app_timer_register(IDLE_EXIT_MS, prv_idle_exit, NULL);
  }
}

// ---------------------------------------------------------------------------
// Nachrichten vom Handy

static void prv_list_updated(bool complete) {
  prv_cancel_sync_timer();
  s_login_active = false;
  ui_login_hide();
  if (complete) {
    prv_set_state(STATE_IDLE);
  } else {
    snprintf(s_error, sizeof(s_error), "Übertragung unvollständig");
    prv_set_state(STATE_ERROR);
  }
  ui_main_reload(true);
}

static void prv_status(int code, const char *text) {
  s_login_active = false;
  ui_login_hide();
  if (code == STATUS_SYNCING) {
    prv_set_state(STATE_SYNCING);
    prv_arm_sync_timer(s_wakeup_launch ? WAKEUP_SYNC_TIMEOUT_MS : SYNC_TIMEOUT_MS);
    return;
  }
  prv_cancel_sync_timer();
  if (code == STATUS_NETWORK) {
    prv_set_state(STATE_OFFLINE);
    return;
  }
  snprintf(s_error, sizeof(s_error), "%s", text[0] ? text : "Fehler");
  prv_set_state(STATE_ERROR);
  ui_main_reload(false);
}

static void prv_login_code(const char *url, const char *code) {
  s_login_active = true;
  prv_cancel_sync_timer();
  ui_login_show(url, code);
}

static void prv_settings_changed(void) {
  prv_update_status();
  ui_main_reload(true);
}

static void prv_send_failed(void) {
  if (s_state == STATE_SYNCING && !s_login_active) {
    prv_cancel_sync_timer();
    prv_set_state(STATE_OFFLINE);
  }
}

// ---------------------------------------------------------------------------
// Wakeup & Tick

static void prv_briefing_start(void) {
  s_wakeup_launch = true;
  vibes_short_pulse();
  prv_interaction();
  prv_request_refresh();
}

static void prv_wakeup_handler(WakeupId id, int32_t cookie) {
  // App war beim Briefing-Zeitpunkt bereits offen.
  settings_schedule_wakeup();
  vibes_short_pulse();
  prv_request_refresh();
}

static void prv_tick(struct tm *tick_time, TimeUnits changed) {
  ui_main_reload(false);
  prv_update_status();
}

// ---------------------------------------------------------------------------

static void prv_init(void) {
  settings_load();
  events_load_cache();

  comm_init((CommHandlers) {
    .list_updated = prv_list_updated,
    .status = prv_status,
    .login_code = prv_login_code,
    .settings_changed = prv_settings_changed,
    .send_failed = prv_send_failed,
  });

  ui_main_push((UiMainCallbacks) {
    .refresh = prv_request_refresh,
    .login = prv_request_login,
    .interaction = prv_interaction,
  });
  ui_main_reload(true);

  wakeup_service_subscribe(prv_wakeup_handler);
  tick_timer_service_subscribe(MINUTE_UNIT, prv_tick);

  // Immer den nächsten Briefing-Zeitpunkt planen (auch nach einem Wakeup).
  settings_schedule_wakeup();

  if (launch_reason() == APP_LAUNCH_WAKEUP) {
    prv_briefing_start();
  } else {
    prv_request_refresh();
  }
}

static void prv_deinit(void) {
  tick_timer_service_unsubscribe();
  prv_cancel_sync_timer();
  comm_deinit();
  ui_main_destroy();
}

int main(void) {
  prv_init();
  app_event_loop();
  prv_deinit();
}
