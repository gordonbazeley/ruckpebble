#!/usr/bin/env bash
set -euo pipefail

# Force the Pebble config page to open in Brave instead of the default browser.
BROWSER='open -a "Brave Browser" %s' pebble emu-app-config --emulator emery
