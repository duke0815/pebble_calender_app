#!/bin/sh
# Store screenshots with neutral English sample data (demo mode: no sign-in, no network).
# Usage: sh tools/screenshots.sh [platform ...]   (default: emery basalt diorite flint)
# Builds a demo version, installs it in the emulator, presses the buttons itself
# (pebble emu-button) and saves 5 screenshots per platform to store/screenshots/.
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
pebble build >/dev/null
mkdir -p "$OUT"

for p in $PLATFORMS; do
  echo "=== $p ==="
  if ! pebble install --emulator "$p"; then
    echo "Emulator $p not available - skipped."
    continue
  fi
  btn() { pebble emu-button --emulator "$p" "$@" >/dev/null; }
  shot() { pebble screenshot --emulator "$p" --no-open "$OUT/$p-$1.png" >/dev/null && echo "  $OUT/$p-$1.png"; }

  sleep 6                                  # demo sync
  shot 1-day
  btn click down; btn click down; btn click down   # first press only shows the selection
  sleep 1; shot 2-selected                 # "Product review"
  btn click select; sleep 3; shot 3-details
  btn click select; sleep 1; shot 4-move   # move options
  btn click back; btn click back; sleep 1
  btn push down; sleep 1; btn release down; sleep 3
  shot 5-tomorrow
done

echo "Screenshots in $OUT/"
