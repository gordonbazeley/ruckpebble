#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
OUT_DIR="$SCRIPT_DIR/screenshots"
PLATFORMS=(emery gabbro)

mkdir -p "$OUT_DIR"
cd "$PROJECT_ROOT"

unset PYTHONPATH
unset PYTHONHOME

install_with_timeout() {
  local timeout_s="$1"
  pebble install &
  local install_pid=$!

  for ((i = 0; i < timeout_s; i += 1)); do
    if ! kill -0 "$install_pid" 2>/dev/null; then
      wait "$install_pid"
      return $?
    fi
    sleep 1
  done

  echo "Install timed out after ${timeout_s}s." >&2
  kill "$install_pid" 2>/dev/null || true
  wait "$install_pid" 2>/dev/null || true
  return 124
}

reset_emulator_state() {
  pebble kill 2>/dev/null || true
  pkill -f "qemu-pebble" 2>/dev/null || true
  pkill -f "pypkjs" 2>/dev/null || true
  sleep 2
  pebble wipe
}

capture_platform() {
  local platform="$1"
  export PEBBLE_EMULATOR="$platform"
  echo "=== $platform ==="

  reset_emulator_state
  if ! install_with_timeout 45; then
    echo "Retrying install with clean emulator state..."
    reset_emulator_state
    install_with_timeout 60
  fi

  sleep 3 # let the profile picker settle
  pebble screenshot --no-open "$OUT_DIR/${platform}_profile.png"

  pebble emu-button click select
  sleep 1
  pebble screenshot --no-open "$OUT_DIR/${platform}_rucking.png"

  pebble kill 2>/dev/null || true
}

pebble clean
pebble build

for platform in "${PLATFORMS[@]}"; do
  capture_platform "$platform"
done

echo "Screenshots saved to $OUT_DIR"
