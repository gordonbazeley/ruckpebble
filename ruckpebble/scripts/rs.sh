#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

# Reset emulator state so install/control commands target the same fresh instance.
unset PYTHONPATH
unset PYTHONHOME
pebble kill >/dev/null 2>&1 || true
printf '{}' > "${TMPDIR}pb-emulator.json"

pebble build
pebble install --emulator emery
pebble emu-app-config --emulator emery
