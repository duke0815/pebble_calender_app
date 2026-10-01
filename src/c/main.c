#include <pebble.h>
#include "protocol.h"
#include "events.h"
#include "settings.h"
#include "comm.h"
#include "format.h"
#include "ui_main.h"
#include "ui_login.h"
#include "ui_detail.h"
#include "ui_move.h"
#include "i18n.h"
#include "idle.h"

#define SYNC_TIMEOUT_MS        30000
#define WAKEUP_SYNC_TIMEOUT_MS 10000
#define DAY_TIMEOUT_MS         15000
#define IDLE_EXIT_MS           60000

// Allowed range for paging through days, relative to today
#define MIN_DAY_OFFSET -60
#define MAX_DAY_OFFSET 90

// Swipes: minimum distance (pixels), time window for the system-generated BACK press (ms)
#define SWIPE_MIN_PX        30
#define TOUCH_BACK_WINDOW_MS 600

typedef enum {
  STATE_IDLE,      // data up to date (or never synced)
  STATE_SYNCING,
  STATE_OFFLINE,   // phone/internet unreachable -> cache
  STATE_ERROR,     // error text from the phone
} SyncState;

static SyncState s_state = STATE_IDLE;
static char s_error[STATUS_LEN];
static bool s_login_active;
static bool s_wakeup_launch;
static AppTimer *s_sync_timer;

// Single day outside the cache currently being loaded from the phone
static bool s_day_loading;
static time_t s_requested_day;
static AppTimer *s_day_timer;

// ---------------------------------------------------------------------------
// Status display

static void prv_update_status(void) {
  char status[STATUS_LEN];
  char stamp[40];
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
      status[0] = '\0';  // normal case: no status line, only the date at the top
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
// Synchronization

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

// Re-request the cache (today + tomorrow) - only for the briefing while the app is open.
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
// Paging through days

static AppTimer *s_day_send_timer;

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

// Page through days: by swiping (touch) or long-pressing up/down
static int s_day_send_tries;

static void prv_day_timeout(void *data) {
  s_day_timer = NULL;
  prv_day_done();
  prv_update_status();
  ui_main_reload(false);
}

// Send the request; if the outbox is still busy (fast swiping), retry shortly after.
static void prv_day_send(void *data) {
  s_day_send_timer = NULL;
  if (!s_day_loading) return;
  if (comm_request(s_requested_day)) return;
  if (++s_day_send_tries < 8) {
    s_day_send_timer = app_timer_register(250, prv_day_send, NULL);
  }
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
  ui_main_animate_day(delta);
}


// ---------------------------------------------------------------------------
// Inactivity: briefing launch 60 s, otherwise the "Close automatically" setting

static void prv_apply_idle(void) {
  idle_configure(s_wakeup_launch ? IDLE_EXIT_MS : (uint32_t)settings_get()->auto_close_min * 60000);
}

static void prv_interaction(void) {
  idle_kick();
}

// ---------------------------------------------------------------------------
// Messages from the phone

static void prv_list_updated(bool complete, time_t list_day, int list_days) {
  s_login_active = false;
  idle_pause(false);
  ui_login_hide();
  if (list_days < 2) {
    // Single day (paging)
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
  idle_pause(false);
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
  idle_pause(true);  // sign-in needs the app open
  prv_cancel_sync_timer();
  ui_login_show(url, code);
}


static void prv_apply_touch_setting(void);

static void prv_settings_changed(void) {
  prv_apply_touch_setting();
  prv_apply_idle();
  ui_main_apply_theme();
  prv_update_status();
  ui_main_reload(true);
}

static void prv_change_day(int delta);

// Move result: shown in the move window; on success reload the day
// (the phone reloads the cache after moving anyway).
static void prv_move_result(int32_t key, bool ok, const char *text) {
  ui_move_result(key, ok, text);
  if (ok) {
    events_invalidate_day();
    prv_change_day(0);
  }
}

static void prv_note(int32_t key, const char *text) {
  ui_detail_set_note(key, text);
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
// Touch (Pebble Time 2): left/right = change day, up/down = scroll

// Vertical scrolling (smooth, with momentum), tap = open event, and swipe right in
// the detail view are handled by PebbleOS touch navigation (app_touch_navigation_enable).
// Only horizontal swipes in the day view are evaluated by the app itself.
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
      // Only gestures that start in the day view change the day. Important: the
      // system touch navigation sees the liftoff before us and closes the detail view on
      // "swipe right" - by our liftoff the day view would already be on top.
      s_touch_active = !event->non_navigational && ui_main_is_top();
      s_touch_x = event->x;
      s_touch_y = event->y;
      return;
    case TouchEvent_PositionUpdate:
      return;
    case TouchEvent_Liftoff:
      s_finger_down = false;
      s_last_liftoff_ms = prv_now_ms();
      prv_interaction();
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
  if (adx < SWIPE_MIN_PX || adx <= ady) {
    return;  // tap/vertical: handled by the system
  }
  prv_change_day(dx < 0 ? 1 : -1);  // swipe left = next day
}

// Touch navigation reports "swipe right" as a BACK press. In the day view
// that means "previous day" (done by prv_touch_handler) - only real button presses exit the app.
static bool prv_back_is_from_touch(void) {
  return s_finger_down || (prv_now_ms() - s_last_liftoff_ms) < TOUCH_BACK_WINDOW_MS;
}
#endif

static void prv_apply_touch_setting(void) {
#if defined(PBL_TOUCH)
  if (settings_get()->touch_off) {
    touch_service_unsubscribe();
    app_touch_navigation_enable(false);
    s_finger_down = false;
  } else {
    touch_service_subscribe(prv_touch_handler, NULL);
    app_touch_navigation_enable(true);
  }
#endif
}

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

// On app start PebbleKit JS syncs exactly once on its own ("ready").
// The watch just waits for it and shows the cache until then.
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
  prv_apply_idle();
  prv_await_startup_sync();
}

static void prv_wakeup_handler(WakeupId id, int32_t cookie) {
  // App was already open at briefing time.
  settings_schedule_wakeup();
  vibes_short_pulse();
  events_set_view_day(time(NULL));
  ui_main_reload(true);
  prv_request_refresh();
}

static void prv_tick(struct tm *tick_time, TimeUnits changed) {
  // At midnight jump to the new day if "today" was displayed
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
    .note = prv_note,
    .move_result = prv_move_result,
  });

  ui_main_push((UiMainCallbacks) {
    .interaction = prv_interaction,
    .back = prv_back,
    .change_day = prv_change_day,
  });
  ui_main_reload(true);

  wakeup_service_subscribe(prv_wakeup_handler);
  prv_apply_touch_setting();
  prv_apply_idle();
  tick_timer_service_subscribe(MINUTE_UNIT, prv_tick);

  // Always schedule the next briefing time (also after a wakeup).
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
