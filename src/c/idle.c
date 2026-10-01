#include "idle.h"

static uint32_t s_timeout_ms;
static bool s_paused;
static AppTimer *s_timer;

static void prv_fire(void *data) {
  s_timer = NULL;
  window_stack_pop_all(false);
}

void idle_kick(void) {
  if (s_timeout_ms == 0 || s_paused) {
    if (s_timer) {
      app_timer_cancel(s_timer);
      s_timer = NULL;
    }
    return;
  }
  if (!s_timer || !app_timer_reschedule(s_timer, s_timeout_ms)) {
    s_timer = app_timer_register(s_timeout_ms, prv_fire, NULL);
  }
}

void idle_configure(uint32_t timeout_ms) {
  s_timeout_ms = timeout_ms;
  idle_kick();
}

void idle_pause(bool paused) {
  s_paused = paused;
  idle_kick();
}
