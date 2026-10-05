# Repository Guidelines
* Two screens: **Profile** (first screen at launch; pick a profile) and **Ruck** (the running ruck, shown after selecting a profile).

## Project Structure
Paths are relative to the Pebble project root (the parent of this `src/` dir, where `package.json` and `wscript` live). Run all `pebble` commands from there.
- `src/c/ruckpebble.c`: watchapp source (C), entry point.
- `src/pkjs/index.js`: phone-side JS companion (config page, messaging).
- `wscript`: build rules; post-build hook minifies phone JS and copies the pbw to `~/Nextcloud/pbws/` (skipped if missing).
- `package.json`: metadata, UUID, `targetPlatforms` (emery, gabbro), `messageKeys`, `resources.media`. Update the last two when adding app messages or assets.
- `../.github/workflows/` (git repo root, one level above the project root): CI builds the pbw and publishes it to a rolling "latest" release.
- `src/dev_docs/`: read the relevant file before changing the area it covers, and keep it current.
  - `architecture.md` — structure/data flow; `decisions.md` — why things are the way they are; `current-state.md` — what works now; `todo.md` — planned work.

## Commands
Run any `pebble` command, `git`, and standard shell utilities without asking.
- `pebble build`, then always `pebble install --emulator emery` (use `gabbro` only when testing that platform).
- `pebble clean`, `pebble logs --emulator emery` (also `src/logs.sh`), `pebble emu-app-config --emulator emery` (opens settings in the default browser).
- `src/run.sh [emery|gabbro]` builds and installs with an install timeout.

## Shortcuts
- `rs` — build, install to the emulator, then open settings.
- `cp` / "commit and push" — stage all changes, commit with a useful message, `git push origin`, no confirmation needed.

## Coding Style
- C with Pebble SDK (`pebble.h`); 2-space indent, K&R braces.
- `s_` prefix for static globals, `prv_` for internal functions.

## Testing
No automated tests. Validate in the emulator and check `pebble logs`.

## Git
- Short, imperative commit summaries (e.g. "Add button handlers").
- PRs: clear description, affected platforms, emulator screenshots for UI changes.
