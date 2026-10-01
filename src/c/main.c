#include <pebble.h>
#include "protocol.h"
#include "events.h"
#include "settings.h"
#include "comm.h"
#include "format.h"
#include "ui_main.h"
#include "ui_login.h"
#include "ui_detail.h"
#include "i18n.h"

#define SYNC_TIMEOUT_MS        30000
#define WAKEUP_SYNC_TIMEOUT_MS 10000
#define DAY_TIMEOUT_MS         15000
#define IDLE_EXIT_MS           60000

// Erlaubter Bereich für das Blättern, relativ zu heute
#define MIN_DAY_OFFSET -60
#define MAX_DAY_OFFSET 90

// Wischgesten: Mindestweg (Pixel), Zeitfenster für die vom System erzeugte BACK-Taste (ms)
#define SWIPE_MIN_PX        30
#define TOUCH_BACK_WINDOW_MS 600

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

// Einzelner Tag außerhalb des Caches, der gerade vom Handy geladen wird
static bool s_day_loading;
static time_t s_requested_day;
static AppTimer *s_day_timer;

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
      snprintf(status, sizeof(status), "%s", tr(STR_UPDATING));
      break;
    case STATE_OFFLINE:
      if (last) {
        snprintf(status, sizeof(status), tr(STR_OFFLINE_AS_OF), stamp);
      } else {
        snprintf(status, sizeof(status), "%s", tr(STR_OFFLINE));
      }
      break;
    case STATE_ERROR:
      snprintf(status, sizeof(status), "%s", s_error);
      break;
    default:
      status[0] = '\0';  // Normalfall: keine Statuszeile, oben steht nur das Datum
  }

  bool today = events_view_day() == time_day_start(time(NULL));
  const char *empty;
  if (!events_view_available()) {
    if (s_day_loading || s_state == STATE_SYNCING) {
      empty = tr(STR_LOADING_EVENTS);
    } else if (s_state == STATE_ERROR) {
      empty = s_error;
    } else {
      empty = tr(STR_NO_PHONE);
    }
  } else if (s_state == STATE_ERROR && today) {
    empty = s_error;
  } else {
    empty = tr(today ? STR_NO_EVENTS_TODAY : STR_NO_EVENTS);
  }
  if (s_day_loading) {
    snprintf(status, sizeof(status), "%s", tr(STR_LOADING_DAY));
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

// Cache (heute + morgen) neu anfordern – nur beim Briefing bei bereits offener App.
static void prv_request_refresh(void) {
  if (!connection_service_peek_pebble_app_connection() || !comm_request(0)) {
    prv_cancel_sync_timer();
    prv_set_state(STATE_OFFLINE);
    return;
  }
  prv_set_state(STATE_SYNCING);
  prv_arm_sync_timer(s_wakeup_launch ? WAKEUP_SYNC_TIMEOUT_MS : SYNC_TIMEOUT_MS);
}

// ---------------------------------------------------------------------------
// Tage blättern

static AppTimer *s_day_send_timer;
static int s_day_send_tries;

static void prv_day_done(void) {
  s_day_loading = false;
  if (s_day_timer) {
    app_timer_cancel(s_day_timer);
    s_day_timer = NULL;
  }
  if (s_day_send_timer) {
    app_timer_cancel(s_day_send_timer);
    s_day_send_timer = NULL;
  }
}

// Anfrage senden; ist der Sendepuffer noch belegt (schnelles Wischen), kurz später erneut.
static void prv_day_send(void *data) {
  s_day_send_timer = NULL;
  if (!s_day_loading) return;
  if (comm_request(s_requested_day)) return;
  if (++s_day_send_tries < 8) {
    s_day_send_timer = app_timer_register(250, prv_day_send, NULL);
  }
}

static void prv_day_timeout(void *data) {
  s_day_timer = NULL;
  prv_day_done();
  prv_update_status();
  ui_main_reload(false);
}

static void prv_change_day(int delta) {
  time_t today = time_day_start(time(NULL));
  time_t day = time_make_local(events_view_day(), delta, 0, 0);
  if (day < time_make_local(today, MIN_DAY_OFFSET, 0, 0) ||
      day > time_make_local(today, MAX_DAY_OFFSET, 0, 0)) {
    return;
  }
  events_set_view_day(day);
  prv_day_done();
  if (!events_view_available() && connection_service_peek_pebble_app_connection()) {
    s_day_loading = true;
    s_requested_day = day;
    s_day_send_tries = 0;
    s_day_timer = app_timer_register(DAY_TIMEOUT_MS, prv_day_timeout, NULL);
    prv_day_send(NULL);
  }
  prv_update_status();
  ui_main_reload(true);
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

static void prv_list_updated(bool complete, time_t list_day, int list_days) {
  s_login_active = false;
  ui_login_hide();
  if (list_days < 2) {
    // Einzelner Tag (Blättern)
    if (list_day == s_requested_day) {
      prv_day_done();
    }
    prv_update_status();
    ui_main_reload(list_day == events_view_day());
    return;
  }
  prv_cancel_sync_timer();
  if (complete) {
    prv_set_state(STATE_IDLE);
  } else {
    snprintf(s_error, sizeof(s_error), "%s", tr(STR_TRANSFER_INCOMPLETE));
    prv_set_state(STATE_ERROR);
  }
  ui_main_reload(true);
}

static void prv_status(int code, const char *text) {
  s_login_active = false;
  ui_login_hide();
  if (code != STATUS_SYNCING) {
    prv_day_done();
  }
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
  snprintf(s_error, sizeof(s_error), "%s", text[0] ? text : tr(STR_ERROR));
  prv_set_state(STATE_ERROR);
  ui_main_reload(false);
}

static void prv_login_code(const char *url, const char *code) {
  s_login_active = true;
  prv_cancel_sync_timer();
  ui_login_show(url, code);
}

static void prv_settings_changed(void) {
  ui_main_apply_theme();
  prv_update_status();
  ui_main_reload(true);
}

static void prv_send_failed(void) {
  if (s_day_loading) {
    prv_day_done();
    prv_update_status();
    ui_main_reload(false);
  }
  if (s_state == STATE_SYNCING && !s_login_active) {
    prv_cancel_sync_timer();
    prv_set_state(STATE_OFFLINE);
  }
}

// ---------------------------------------------------------------------------
// Touch (Pebble Time 2): links/rechts = Tag wechseln, hoch/runter = scrollen

// Vertikales Scrollen (flüssig, mit Schwung), Tippen = Termin öffnen und Wischen nach rechts in
// der Detailansicht übernimmt die Touch-Navigation von PebbleOS (app_touch_navigation_enable).
// Selbst ausgewertet wird nur das horizontale Wischen in der Tagesansicht.
#if defined(PBL_TOUCH)
static bool s_touch_active;
static bool s_finger_down;
static int16_t s_touch_x;
static int16_t s_touch_y;
static uint32_t s_last_liftoff_ms;

static uint32_t prv_now_ms(void) {
  time_t sec;
  uint16_t ms;
  time_ms(&sec, &ms);
  return (uint32_t)sec * 1000 + ms;
}

static void prv_touch_handler(const TouchEvent *event, void *context) {
  switch (event->type) {
    case TouchEvent_Touchdown:
      s_finger_down = true;
      s_touch_active = !event->non_navigational;
      s_touch_x = event->x;
      s_touch_y = event->y;
      return;
    case TouchEvent_PositionUpdate:
      return;
    case TouchEvent_Liftoff:
      s_finger_down = false;
      s_last_liftoff_ms = prv_now_ms();
      break;
    default:
      return;
  }
  if (!s_touch_active) return;
  s_touch_active = false;

  int dx = event->x - s_touch_x;
  int dy = event->y - s_touch_y;
  int adx = dx < 0 ? -dx : dx;
  int ady = dy < 0 ? -dy : dy;
  prv_interaction();
  if (adx < SWIPE_MIN_PX || adx <= ady || !ui_main_is_top()) {
    return;  // Tippen/vertikal: macht das System; Detail-/Login-Fenster: nichts
  }
  prv_change_day(dx < 0 ? 1 : -1);  // nach links wischen = nächster Tag
}

// Die Touch-Navigation meldet "nach rechts wischen" als BACK-Taste. In der Tagesansicht
// heißt das "vorheriger Tag" (macht prv_touch_handler) – nur echte Tastendrücke beenden die App.
static bool prv_back_is_from_touch(void) {
  return s_finger_down || (prv_now_ms() - s_last_liftoff_ms) < TOUCH_BACK_WINDOW_MS;
}
#endif

static void prv_back(void) {
#if defined(PBL_TOUCH)
  if (prv_back_is_from_touch()) {
    return;
  }
#endif
  window_stack_pop_all(true);
}

// ---------------------------------------------------------------------------
// Wakeup & Tick

// Beim App-Start synchronisiert PebbleKit JS genau einmal von selbst ("ready").
// Die Uhr wartet nur darauf und zeigt bis dahin den Cache.
static void prv_await_startup_sync(void) {
  if (!connection_service_peek_pebble_app_connection()) {
    prv_set_state(STATE_OFFLINE);
    return;
  }
  prv_set_state(STATE_SYNCING);
  prv_arm_sync_timer(s_wakeup_launch ? WAKEUP_SYNC_TIMEOUT_MS : SYNC_TIMEOUT_MS);
}

static void prv_briefing_start(void) {
  s_wakeup_launch = true;
  vibes_short_pulse();
  prv_interaction();
  prv_await_startup_sync();
}

static void prv_wakeup_handler(WakeupId id, int32_t cookie) {
  // App war beim Briefing-Zeitpunkt bereits offen.
  settings_schedule_wakeup();
  vibes_short_pulse();
  events_set_view_day(time(NULL));
  ui_main_reload(true);
  prv_request_refresh();
}

static void prv_tick(struct tm *tick_time, TimeUnits changed) {
  // Um Mitternacht auf den neuen Tag springen, wenn "heute" angezeigt wurde
  static time_t s_last_today;
  time_t today = time_day_start(time(NULL));
  if (s_last_today && today != s_last_today && events_view_day() == s_last_today) {
    events_set_view_day(today);
  }
  s_last_today = today;
  ui_main_reload(false);
  prv_update_status();
}

// ---------------------------------------------------------------------------

static void prv_init(void) {
  settings_load();
  events_load_cache();
  events_set_view_day(time(NULL));

  comm_init((CommHandlers) {
    .list_updated = prv_list_updated,
    .status = prv_status,
    .login_code = prv_login_code,
    .settings_changed = prv_settings_changed,
    .send_failed = prv_send_failed,
  });

  ui_main_push((UiMainCallbacks) {
    .interaction = prv_interaction,
    .back = prv_back,
  });
  ui_main_reload(true);

  wakeup_service_subscribe(prv_wakeup_handler);
#if defined(PBL_TOUCH)
  touch_service_subscribe(prv_touch_handler, NULL);
  app_touch_navigation_enable(true);
#endif
  tick_timer_service_subscribe(MINUTE_UNIT, prv_tick);

  // Immer den nächsten Briefing-Zeitpunkt planen (auch nach einem Wakeup).
  settings_schedule_wakeup();

  if (launch_reason() == APP_LAUNCH_WAKEUP) {
    prv_briefing_start();
  } else {
    prv_await_startup_sync();
  }
}

static void prv_deinit(void) {
#if defined(PBL_TOUCH)
  touch_service_unsubscribe();
#endif
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
