#!/usr/bin/env bash
set -euo pipefail

# Include pypkjs logs so config save/open events are visible.
pebble logs --emulator emery --pypkjs --platform emery
