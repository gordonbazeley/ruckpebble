#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

unset PYTHONPATH
unset PYTHONHOME

pebble clean
pebble build

# Install to the running emulator, or start one if needed.
echo "Starting emulator..."
if ! pebble install --emulator emery; then
  echo "Waiting for emulator to boot..."
  sleep 30
  pebble install --emulator emery
fi

pebble emu-app-config --emulator emery &
config_pid=$!
for _ in {1..10}; do
  if ! kill -0 "$config_pid" 2>/dev/null; then
    wait "$config_pid"
    exit $?
  fi
  sleep 1
done

echo "App installed. Config browser did not finish opening; leaving emulator running."
kill "$config_pid" 2>/dev/null || true
