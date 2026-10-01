#include "settings.h"
#include "events.h"

#define PERSIST_KEY_SETTINGS 4
#define WAKEUP_COOKIE_BRIEFING 1
#define WAKEUP_RETRIES 5

static Settings s_settings;

static void prv_defaults(void) {
  s_settings = (Settings) {
    .brief_enabled = 0,
    .brief_hour = 7,
    .brief_minute = 0,
    .brief_days = 0x3E,  // Mo-Fr
  };
}

void settings_load(void) {
  prv_defaults();
  if (persist_exists(PERSIST_KEY_SETTINGS)) {
    Settings stored;
    if (persist_read_data(PERSIST_KEY_SETTINGS, &stored, sizeof(stored)) == (int)sizeof(stored)) {
      s_settings = stored;
    }
  }
}

void settings_save(void) {
  persist_write_data(PERSIST_KEY_SETTINGS, &s_settings, sizeof(s_settings));
}

Settings *settings_get(void) {
  return &s_settings;
}

// Naechster Zeitpunkt > now mit passender Uhrzeit und erlaubtem Wochentag.
static time_t prv_next_briefing(time_t now) {
  for (int day = 0; day <= 7; day++) {
    time_t candidate = time_make_local(now, day, s_settings.brief_hour, s_settings.brief_minute);
    if (candidate <= now) {
      continue;
    }
    struct tm *check = localtime(&candidate);
    if (s_settings.brief_days & (1 << check->tm_wday)) {
      return candidate;
    }
  }
  return 0;
}

void settings_schedule_wakeup(void) {
  wakeup_cancel_all();
  if (!s_settings.brief_enabled || s_settings.brief_days == 0) {
    return;
  }
  time_t when = prv_next_briefing(time(NULL));
  if (when == 0) {
    return;
  }
  for (int i = 0; i < WAKEUP_RETRIES; i++) {
    WakeupId id = wakeup_schedule(when, WAKEUP_COOKIE_BRIEFING, true);
    if (id >= 0) {
      APP_LOG(APP_LOG_LEVEL_INFO, "Briefing geplant: %d", (int)when);
      return;
    }
    if (id != E_RANGE) {
      APP_LOG(APP_LOG_LEVEL_ERROR, "wakeup_schedule fehlgeschlagen: %d", (int)id);
      return;
    }
    // Ein anderer Wakeup liegt zu nah dran -> eine Minute spaeter versuchen.
    when += 60;
  }
  APP_LOG(APP_LOG_LEVEL_ERROR, "Briefing konnte nicht geplant werden");
}
