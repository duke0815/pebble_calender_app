#pragma once
#include <pebble.h>

// Exits the app after a time without user interaction.
void idle_configure(uint32_t timeout_ms);  // 0 = never
void idle_pause(bool paused);              // e.g. during sign-in
void idle_kick(void);                      // call on every user interaction
