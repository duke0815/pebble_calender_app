#!/bin/sh
# Store screenshots with neutral English sample data (demo mode: no sign-in, no network).
# Usage: sh tools/screenshots.sh [platform ...]   (default: emery basalt diorite flint)
# Builds a demo version, installs it in the emulator and takes a screenshot after each
# step you do in the emulator window (arrow keys: up/down, right = Select, left = Back).
# The app closes itself after 5 minutes without input - then just run the script again.
# At the end the normal version is built again, so build/*.pbw is never the demo.
set -e
cd "$(dirname "$0")/.."
PLATFORMS="${*:-emery basalt diorite flint}"
OUT=store/screenshots
BACKUP=$(mktemp)
cp src/pkjs/index.js "$BACKUP"

restore() {
  cp "$BACKUP" src/pkjs/index.js
  rm -f "$BACKUP"
  echo "Rebuilding the normal (non-demo) version..."
  pebble build >/dev/null && echo "build/*.pbw is the normal version again."
}
trap restore EXIT

sed -i 's/^var DEMO = false;/var DEMO = true;/' src/pkjs/index.js
grep -q '^var DEMO = true;' src/pkjs/index.js || { echo "DEMO switch not found in index.js"; exit 1; }
pebble build
mkdir -p "$OUT"

for p in $PLATFORMS; do
  echo
  echo "=== $p ==="
  if ! pebble install --emulator "$p"; then
    echo "Emulator $p not available - skipped."
    continue
  fi
  n=0
  shot() {
    n=$((n + 1))
    printf '%s\n  -> then press Enter (s = skip) ' "$1"
    read answer
    [ "$answer" = "s" ] && return 0
    pebble screenshot --emulator "$p" --no-open "$OUT/$p-$n-$2.png"
  }
  shot "Wait until 'Updating...' is gone (day view of today)." day
  shot "Press Down 3x (selection on 'Product review')." selected
  shot "Press Select (details with description; wait for it to load)." details
  shot "Press Back, Down 2x to 'Dentist appointment', Select, Select (move options)." move
  shot "Press Back 2x, then hold Down ~1 s (tomorrow; if holding does not work: s)." tomorrow
done

echo
echo "Screenshots in $OUT/"
