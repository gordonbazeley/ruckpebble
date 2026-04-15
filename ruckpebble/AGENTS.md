# Repository Guidelines
* There are two screens to the app
  * Profile - this is the first screen which shows when the app starts where you select Profile
  * Ruck - this is the running ruck screen which shows when you select a profile

## Project Structure & Module Organization
- `src/c/`: Pebble app source in C (entry point is `src/c/ruckpebble.c`).
- `src/pkjs/` and `src/common/`: optional JavaScript companion code if added later (referenced by `wscript`).
- `wscript`: Pebble SDK build rules and bundle configuration.
- `package.json`: Pebble metadata (targets, UUID, resources).

## Build, Test, and Development Commands
You have my permissions to always run commands that start with pebble
- `pebble build`: Compile the watchapp using the Pebble SDK and `wscript`. 
- `pebble install --emulator <platform>`: Build and install to an emulator (e.g., `basalt`).
- `pebble clean`: Remove `build/` artifacts.
- `pebble logs`: Stream device/emulator logs for runtime debugging.
- `./scripts/emu-logs.sh`: Preferred logs command for config debugging (includes pypkjs output).
- `pebble emu-app-config --emulator emery`: Open emulator app config in the default browser (Safari).
- After every successful `pebble build`, always run `pebble install --emulator emery`.

## Runbook (Commands To Run)
- "rs" - run the emulator and settings
- Run emulator app:
  - `cd /Users/gordonbazeley/src/ruckpebble/ruckpebble`
  - `pebble build`
  - `pebble install --emulator emery`
- Open settings (Safari/default browser):
  - `cd /Users/gordonbazeley/src/ruckpebble/ruckpebble`
  - `pebble emu-app-config --emulator emery`
- Review logs (watch + pkjs):
  - `cd /Users/gordonbazeley/src/ruckpebble/ruckpebble`
  - `./scripts/emu-logs.sh`

## Approved Commands
All permissions granted — run any command needed to build, debug, and test the app without asking. This includes:
- Any `pebble` command (build, install, logs, screenshot, emu-button, emu-app-config, clean, etc.)
- Any `git` command on this repo
- Standard shell utilities: `sleep`, `grep`, `cat`, `ps`, `kill`, `pkill`, `open`, `osascript`, `/bin/bash -lc "..."`, etc.
- `./scripts/emu-logs.sh` and any script in the repo

## Coding Style & Naming Conventions
- Language: C for watchapp logic; follow Pebble SDK APIs (`pebble.h`).
- Indentation: 2 spaces, K&R-style braces as seen in `src/c/ruckpebble.c`.
- Naming: `s_` prefix for static globals, `prv_` prefix for internal functions.
- Keep functions small and event-driven (handlers, load/unload, init/deinit).


## Testing Guidelines
- No automated test framework is configured in this repository.
- Validate behavior by running in the emulator and checking `pebble logs`.
- If you add tests, document the runner and required commands here.

## Git
* If I ask you to "commit and push" or "cp" then do the following (without requesting permission)
  * Stage all changed files
  * Add a useful commit message
  * git push origin
- `origin` now points to `https://github.com/gordonbazeley/ruckpebble.git`.
- Git history only shows “Initial …” commits; no formal convention established.
- Use short, imperative summaries (e.g., “Add button handlers”).
- PRs should include a clear description, affected platforms, and emulator screenshots for UI changes.

## Configuration & Targets
- Supported platforms live in `package.json` under `pebble.targetPlatforms`.
- Update `messageKeys` and `resources.media` when adding app messages or assets.
