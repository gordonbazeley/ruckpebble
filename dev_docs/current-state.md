# RuckPebble Current State

App version: **1.4.2**  
Target platform: **emery** (Pebble Time Round)  
Settings key: `ruck_settings_v2`  
Schema version: **2**

---

## What works

### Watch app (C)
- Profile selection screen with up to 3 profiles; touch selects profile, centre button starts ruck
- Rucking screen shows: profile name, session pace, current pace (60s rolling), steps (session + today), current time, distance, heart rate, elapsed time, calories (ruck + walk comparison)
- Pause / resume (up button)
- Save ruck (down button) — writes totals to persistent storage, sends activity data to phone
- Discard (back/left button)
- Resume in-progress session on launch: RESTORE prompt appears, accepting goes straight to the rucking screen (not the profile screen)
- Step counting from Pebble Health, updated roughly every 10 seconds
- Lifetime totals accumulate correctly across sessions
- Timeline pins inserted on save (via phone companion)

### Phone companion JS
- Settings synced to watch on `ready`
- Lifetime totals requested from watch on `ready` (3 attempts, 1.2s timeout each)
- Config page opens GitHub Pages URL with current settings pre-populated
- `webviewclosed` correctly merges saved settings without overwriting live totals with stale data
- Timeline pins sent to Pebble timeline API on new ruck saves

### Settings page (GitHub Pages)
- Three tabs: Profiles, Calories, History
- Profiles tab: "About you" section (body weight, units, stride length) + 3 profile cards
- Profile cards show ruck weight, terrain, grade summary; pencil button opens inline editor
- Profile editor: name, pack weight (with unit suffix), grade, terrain dropdown
- Calorie chart (Calories tab) shows estimated kcal/hr at four pace options for all 3 profiles + walking baseline
- History tab: lifetime distance/calories + last ruck details (read-only)
- Info (ⓘ) buttons on: Units, Ruck weight units, Grade (per profile), Terrain (per profile)
- Save changes button hidden on Calories and History tabs (visible only on Profiles tab)
- Calorie values ≥ 10,000 formatted with comma separator
- Settings persist across browser sessions (passed in URL, returned on save)

### Ruck weight unit handling
- Ruck weight always stored internally as kg-tenths
- `ruck_weight_unit` setting is independent of general `measurement_unit`
- Default is lb (value=1) since ruck plates are sold in lb
- Display converts correctly in both directions

### Dev tooling
- `open_config.js`: local dev server for testing settings page without deploying to GitHub Pages
- `create_screenshots.py`: generates 6 annotated explainer images from real screenshots
  - `ruck_profile_screen_annotated.png`
  - `ruck_tracking_screen_annotated.png`
  - `ruck_settings_about_you_annotated.png`
  - `ruck_settings_profiles_annotated.png`
  - `ruck_settings_calories_annotated.png`
  - `ruck_settings_history_annotated.png`

---

## Recent changes (session history)

### Bug fixes
- **Ruck weight doubled in display:** `profileKcal` was applying `×0.453592` to a value already in kg-tenths. `profileDisplayName` was missing `/10` after `kgTenthsToLbTenths`. Both fixed.
- **Resume goes to profile screen instead of rucking screen:** RESTORE mode only removed the prompt window. Fixed: also remove `s_profile_window` (no animation) before removing the prompt window (animated).
- **Info button double-toggle (click shows then immediately hides):** Static `querySelectorAll(".info-btn")` at init was wiring a handler to ALL `.info-btn` elements including the ones that `renderProfileList()` would later create. After re-render, those dynamic buttons had two handlers (one from init, one from render). Fixed with event delegation on `#profile_list` + scoped static selector excludes profile list buttons.
- **`open_config.js` "Unexpected config URL format":** The phone app now sends an `https://` URL (GitHub Pages). Script only handled `data:` URIs. Fixed: added HTTPS branch that reads local `docs/config.html` and injects `?data=` from the URL params.

### Features added
- Grade info button per profile (tooltip: grade calculation guidance)
- Terrain info button per profile (tooltip: terrain effect explanation)
- "Save changes" bar hidden on Calories and History tabs
- Comma formatting for calorie values ≥ 10,000
- Settings page label change: "General units" → "Units"
- Ruck weight info text updated: "Units for ruck weight. Ruck plates are commonly sold in lb."
- Info button styling: lowercase italic, 15×15px circle

### Reverted
- **Ponytail audit removed `ruckUnit()`, `convertRuckWeightValues()`, and two label assignments from `setSharedLabels()`** — these were identified as "dead code" but the removal broke the GitHub Pages settings page. All four changes reverted; the info button and event delegation work from the same commit is kept.

### Screenshot tooling
- `screenshot_descriptions.md` restructured: settings page sections are now separate `---`-delimited documents (one per settings tab) rather than sections within one document
- `create_screenshots.py` updated accordingly: each settings section generates its own output image; `parse_document` now handles top-level key-values (no `##` heading required)

---

## Known issues / rough edges

- The Pebble SDK Python venv can break when macOS upgrades Python. Fix: `ln -sf $(which python3.13) /path/to/sdk/.venv/bin/python`. Must point to 3.13 specifically (not `python3` which may be 3.14+).
- Timeline pins use `Date.now() + 120000` as the pin time (2 minutes in the future) as a workaround for Pebble timeline ordering — the exact timestamp isn't used for display but must be in the future.
- Step counts are from Pebble Health and lag by ~10 seconds. The rucking screen shows "NB. Steps are updated every ten seconds or so" in the explainer.
- Heart rate shows `--` when no recent reading exists (Pebble Health doesn't guarantee continuous HR).
- `convertRuckWeightValues` in the settings page converts in-flight input field values when ruck weight unit changes, but `renderProfileList()` immediately rebuilds from `draftProfiles` (which already has the correct kg-tenths value), so `convertRuckWeightValues` is technically redundant. It's kept because removing it broke the live page (likely a timing or edge-case dependency not yet identified).
