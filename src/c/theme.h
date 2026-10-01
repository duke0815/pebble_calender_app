#pragma once
#include <pebble.h>

// Farben aus den Einstellungen (Akzent, Schrift, Hintergrund) plus daraus abgeleitete
// Kontrastfarben. Auf Schwarz-Weiß-Uhren feste Farben.
GColor theme_accent(void);      // Datumsleiste, ausgewählte Zeile
GColor theme_on_accent(void);   // Schrift auf der Akzentfarbe (automatisch hell/dunkel)
GColor theme_text(void);
GColor theme_bg(void);
GColor theme_muted(void);       // vergangene Termine
GColor theme_header_bg(void);   // Statuszeile
GColor theme_header_text(void);
