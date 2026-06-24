# RuckPebble Todo

---

## High priority

### Diagnose and fix the ponytail revert
The ponytail audit removed `convertRuckWeightValues()` from `config.html` and this broke the live GitHub Pages settings page. The revert was applied but the root cause is unknown — `renderProfileList()` should overwrite any values `convertRuckWeightValues` set, so theoretically it's dead code. Investigate with the actual live page to find the real failure mode before removing it again.

### Understand settings page breakage mode
Before the next ponytail-style cleanup, test the settings page end-to-end on the real GitHub Pages URL (not just `open_config.js`) to establish a baseline. The live page and the local dev server differ in subtle ways (CSP, caching, mobile Pebble app webview vs desktop browser).

---

## Features

### More than 3 profiles
Currently hardcoded as 3 everywhere. Full analysis in `src/todo.md`. Least-invasive approach:

1. Pre-allocate keys for up to N (e.g. 8) profiles in `package.json` (`profile4_*` … `profile8_*`)
2. Add `profile_count` field to the C `Settings` struct
3. Add a settings schema version bump (currently v2 → v3)
4. Expand defaults/normalization/sync in `pkjs/index.js` to loop over dynamic count
5. Add/remove profile buttons in the settings page UI
6. The C menu layer becomes dynamic: `get_num_rows` returns `s_settings.profile_count`

### Anomaly notification when walk kcal > ruck kcal
If the formula produces a ruck calorie burn lower than an unweighted walk, something is wrong with the profile setup (weight too low, grade negative, etc.). Options per `src/todo.md`:
- Discord webhook (simplest)
- Pushover (cleanest UX)
- Cloudflare Worker forwarding to Discord/ntfy/email (cleanest infrastructure)

Must be opt-in — this is a user-installed app and any outbound call is telemetry.

Minimum payload: app version, watch platform, timestamp, profile name, load, body weight, speed, walk kcal/hr, ruck kcal/hr, branch condition that triggered. Send first hit per session only.

### Pace over last N minutes (configurable)
Current pace is a 60-second rolling window. Some users want 5-minute pace to smooth out GPS jitter. Could be a setting or a long-press action to cycle window size.

### Map / route tracking
Currently no GPS; distance is step-count × stride-length. Pebble has no built-in GPS but can receive location via `Pebble.addEventListener('appmessage')` from the phone app. Would require the companion JS to poll phone GPS and forward coordinates.

---

## Settings page

### About you tab is missing from the tab bar
The current settings page shows three tabs: Profiles, Calories, History. "About you" content (body weight, units, stride) lives inside the Profiles tab. Consider whether "About you" should be its own tab, or at least whether the current placement is intuitive — a new user opening the settings page first sees profiles, not where to enter their body weight.

### Grade input validation
Grade percent can be entered as any number. No validation prevents nonsensical values (e.g. 200%, negative numbers). A sensible range is 0–30%.

### Profile name character limit
Currently 32 characters, enforced by `maxlength="32"`. The Pebble display can only show ~12–15 characters in the profile name on the rucking screen before truncation. Consider adding a visible character counter or warning.

### Settings page offline support
If opened without internet (unusual but possible), the GitHub Pages page won't load at all. A service worker could cache the page after first load.

---

## Code health

### Remove `measurement_unit` / `weight_unit` / `stride_length_unit` duplication
`normalizeSettings()` copies `measurement_unit` into `weight_unit` and `stride_length_unit` before syncing. These three fields always have the same value. The C side reads `weight_unit` for body weight display and `stride_unit` for stride display — both could read `measurement_unit` directly. Removing the redundant fields would simplify the message payload.

### Consistent terrain type canonical set
Terrain types `road`, `gravel`, `mixed`, `sand`, `snow` appear in: the C code, `pkjs/index.js` (`terrainTypeFromSettings`, `terrainFactorFromType`), and `config.html` (`terrainOpts`, `terrainFactor`, `terrainLabel`). There is no shared definition — all three need to be kept in sync manually. A single source of truth (e.g. generated from `package.json` or a shared JSON config) would help.

### `open_config.js` error handling for missing `pebble` CLI
`syncToWatch()` calls `pebble send-app-message` via `execFileSync`. If the pebble CLI isn't on PATH, it throws. The error is caught and logged as a warning, but the user gets no actionable message. Could check for `pebble` presence at startup and print a clear "install pebble CLI or add to PATH" message.

### C: unused `s_post_save_stay` flag
`s_post_save_stay` is set but its effect should be audited — confirm it's still needed post the resume/restore flow refactor.

---

## Explainer screenshots

### Screenshot automation
Currently screenshots of the watch emulator and settings page are taken manually and dropped into `resources/screenshots/`. Consider scripting: `pebble screenshot --emulator emery` for the watch screens, and a headless browser (Playwright/Puppeteer) for the settings page screenshots.

### `settings_about_you.png` is not cropped to just the "About you" section
The current `settings_about_you.png` screenshot shows the full settings page with all tabs visible. It could be cropped to just the "About you" section content to better match the annotated image's intent.

---

## Infrastructure

### GitHub Actions for build
No CI currently. A GitHub Actions workflow that runs `pebble build` would catch C compile errors on push. The Pebble SDK can be installed in a container.

### App store listing update
The store `.pbw` file in `resources/store/` should be updated whenever the version in `package.json` changes. Check whether the current `ruckpebble.pbw` matches version 1.4.2.

### Changelog
No changelog exists. Consider a `CHANGELOG.md` at the repo root or updates to the Pebble app store description when releasing.
