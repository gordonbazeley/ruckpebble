# Repository Guidelines

## Project Structure & Module Organization
- `src/c/`: Pebble app source in C (entry point is `src/c/myfirstproject.c`).
- `src/pkjs/` and `src/common/`: optional JavaScript companion code if added later (referenced by `wscript`).
- `wscript`: Pebble SDK build rules and bundle configuration.
- `package.json`: Pebble metadata (targets, UUID, resources).

## Build, Test, and Development Commands
- `pebble build`: Compile the watchapp using the Pebble SDK and `wscript`.
- `pebble install --emulator <platform>`: Build and install to an emulator (e.g., `basalt`).
- `pebble clean`: Remove `build/` artifacts.
- `pebble logs`: Stream device/emulator logs for runtime debugging.

## Coding Style & Naming Conventions
- Language: C for watchapp logic; follow Pebble SDK APIs (`pebble.h`).
- Indentation: 2 spaces, K&R-style braces as seen in `src/c/myfirstproject.c`.
- Naming: `s_` prefix for static globals, `prv_` prefix for internal functions.
- Keep functions small and event-driven (handlers, load/unload, init/deinit).

## Testing Guidelines
- No automated test framework is configured in this repository.
- Validate behavior by running in the emulator and checking `pebble logs`.
- If you add tests, document the runner and required commands here.

## Commit & Pull Request Guidelines
- Git history only shows “Initial …” commits; no formal convention established.
- Use short, imperative summaries (e.g., “Add button handlers”).
- PRs should include a clear description, affected platforms, and emulator screenshots for UI changes.

## Configuration & Targets
- Supported platforms live in `package.json` under `pebble.targetPlatforms`.
- Update `messageKeys` and `resources.media` when adding app messages or assets.
