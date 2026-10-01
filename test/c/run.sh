#!/bin/sh
# Prüft den C-Code gegen den Stub-Header (Syntax für alle Plattformen) und testet die Logik nativ.
set -e
cd "$(dirname "$0")/../.."
# Wie das echte Pebble SDK (arm-none-eabi-gcc): -std=c99 -Os -Wall -Wextra -Werror ...
# -fshort-enums: Enums sind auf ARM vorzeichenlos (sonst fallen z. B. -Wtype-limits-Fehler nicht auf)
FLAGS="-std=c99 -Os -fshort-enums -Wall -Wextra -Werror -Wno-unused-parameter \
  -Wno-error=unused-function -Wno-error=unused-variable -Wno-error=format-truncation \
  -Wno-error=unused-value -Wno-error=cast-function-type -Itest/c/stub"
for defs in "-DPBL_COLOR -DPBL_TOUCH -DPBL_PLATFORM_EMERY" "-DPBL_COLOR -DPBL_PLATFORM_BASALT" "-DPBL_BW -DPBL_PLATFORM_DIORITE"; do
  OBJ=$(mktemp -d)
  for f in src/c/*.c; do
    gcc $FLAGS $defs -c "$f" -o "$OBJ/$(basename "$f" .c).o"
  done
  rm -rf "$OBJ"
done
echo "Syntax ok (emery, basalt, diorite)"
OUT=$(mktemp)
gcc -std=gnu99 -Wall -Itest/c/stub -DPBL_COLOR -Dtime=fake_time \
  test/c/logic_test.c src/c/events.c src/c/settings.c src/c/i18n.c src/c/theme.c src/c/format.c -o "$OUT"
"$OUT"
rm -f "$OUT"
