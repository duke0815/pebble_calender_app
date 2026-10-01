#include "i18n.h"
#include "settings.h"

static const char *const STRINGS[2][STR_COUNT] = {
  [LANG_DE] = {
    [STR_TODAY] = "Heute",
    [STR_TOMORROW] = "Morgen",
    [STR_ALL_DAY] = "Ganztägig",
    [STR_NO_TITLE] = "(ohne Titel)",
    [STR_UPDATING] = "Aktualisiere...",
    [STR_LOADING_DAY] = "Lade Tag...",
    [STR_LOADING_EVENTS] = "Lade Termine...",
    [STR_OFFLINE] = "Offline",
    [STR_OFFLINE_AS_OF] = "Offline - Stand %s",
    [STR_NO_PHONE] = "Keine Verbindung zum Handy.",
    [STR_NO_EVENTS_TODAY] = "Keine Termine heute",
    [STR_NO_EVENTS] = "Keine Termine",
    [STR_TRANSFER_INCOMPLETE] = "Übertragung unvollständig",
    [STR_ERROR] = "Fehler",
    [STR_LOGIN_TITLE] = "Anmeldung",
    [STR_LOGIN_OPEN] = "Am PC/Handy öffnen:",
    [STR_LOGIN_ENTER] = "und Code eingeben:",
    [STR_LOGIN_HINT] = "App bis zum Abschluss offen lassen.",
    [STR_NOTES_LOADING] = "Lade Beschreibung...",
    [STR_NOTES_NEED_PHONE] = "Die Beschreibung benötigt die Nähe zum Handy.",
    [STR_NOTES_NONE] = "Keine Beschreibung",
    [STR_MOVE_HINT] = "Select: Verschieben",
    [STR_MOVE_TITLE] = "Verschieben",
    [STR_MOVE_OPT_HOUR] = "1 Stunde später",
    [STR_MOVE_OPT_DAY] = "1 Tag später",
    [STR_MOVE_OPT_2DAYS] = "2 Tage später",
    [STR_MOVE_OPT_WEEK] = "1 Woche später",
    [STR_MOVE_CONFIRM] = "Verschieben auf\n%s?",
    [STR_MOVE_CONFIRM_HINT] = "Select = Ja\nZurück = Nein",
    [STR_MOVING] = "Verschiebe...",
    [STR_MOVED] = "Verschoben auf\n%s",
    [STR_MOVE_FAILED] = "Verschieben fehlgeschlagen",
  },
  [LANG_EN] = {
    [STR_TODAY] = "Today",
    [STR_TOMORROW] = "Tomorrow",
    [STR_ALL_DAY] = "All day",
    [STR_NO_TITLE] = "(no title)",
    [STR_UPDATING] = "Updating...",
    [STR_LOADING_DAY] = "Loading day...",
    [STR_LOADING_EVENTS] = "Loading events...",
    [STR_OFFLINE] = "Offline",
    [STR_OFFLINE_AS_OF] = "Offline - as of %s",
    [STR_NO_PHONE] = "No connection to phone.",
    [STR_NO_EVENTS_TODAY] = "No events today",
    [STR_NO_EVENTS] = "No events",
    [STR_TRANSFER_INCOMPLETE] = "Transfer incomplete",
    [STR_ERROR] = "Error",
    [STR_LOGIN_TITLE] = "Sign in",
    [STR_LOGIN_OPEN] = "On PC/phone open:",
    [STR_LOGIN_ENTER] = "and enter code:",
    [STR_LOGIN_HINT] = "Keep the app open until done.",
    [STR_NOTES_LOADING] = "Loading description...",
    [STR_NOTES_NEED_PHONE] = "The description needs the phone nearby.",
    [STR_NOTES_NONE] = "No description",
    [STR_MOVE_HINT] = "Select: Move",
    [STR_MOVE_TITLE] = "Move",
    [STR_MOVE_OPT_HOUR] = "1 hour later",
    [STR_MOVE_OPT_DAY] = "1 day later",
    [STR_MOVE_OPT_2DAYS] = "2 days later",
    [STR_MOVE_OPT_WEEK] = "1 week later",
    [STR_MOVE_CONFIRM] = "Move to\n%s?",
    [STR_MOVE_CONFIRM_HINT] = "Select = yes\nBack = no",
    [STR_MOVING] = "Moving...",
    [STR_MOVED] = "Moved to\n%s",
    [STR_MOVE_FAILED] = "Move failed",
  },
};

static const char *const WEEKDAYS[2][7] = {
  [LANG_DE] = { "So", "Mo", "Di", "Mi", "Do", "Fr", "Sa" },
  [LANG_EN] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" },
};

static const char *const MONTHS[12] = {
  "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static int prv_lang(void) {
  return settings_get()->lang == LANG_EN ? LANG_EN : LANG_DE;
}

const char *tr(StrId id) {
  return ((unsigned)id < STR_COUNT) ? STRINGS[prv_lang()][id] : "";
}

const char *tr_weekday(int wday) {
  return WEEKDAYS[prv_lang()][((wday % 7) + 7) % 7];
}

const char *tr_month(int mon) {
  return MONTHS[((mon % 12) + 12) % 12];
}

uint8_t i18n_default_lang(void) {
  const char *locale = i18n_get_system_locale();
  return (locale && strncmp(locale, "de", 2) == 0) ? LANG_DE : LANG_EN;
}
