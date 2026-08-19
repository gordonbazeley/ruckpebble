# RuckPebble Architecture

## Overview

RuckPebble is a Pebble smartwatch app for tracking rucking (weighted walking/hiking). It targets the **Pebble Time Round (emery)** platform using Pebble SDK 3. The app has three distinct layers that communicate at runtime.

```
┌─────────────────────┐      AppMessage       ┌──────────────────────┐
│   Pebble Watch      │ ◄──────────────────── │  Phone Companion JS  │
│   (C / ruckpebble.c)│ ──────────────────►   │  (pkjs/index.js)     │
└─────────────────────┘                       └──────────┬───────────┘
                                                         │  openURL / webviewclosed
                                                         ▼
                                               ┌──────────────────────┐
                                               │  GitHub Pages        │
                                               │  (docs/config.html)  │
                                               └──────────────────────┘
```

---

## Layer 1: Watch App (C)

**File:** `ruckpebble/src/c/ruckpebble.c`

### Window Stack

```
s_profile_window        ← shown on launch; profile selection menu
  └─ s_ruck_prompt_window  ← shown on down/back from the rucking screen, on
                             restore-on-launch, or on a check-in timeout.
                             Modes: BACK, DOWN, RESTORE, CHECKIN
       └─ s_window          ← main rucking/tracking screen
```

On a RESTORE (in-progress session found on launch), the window stack starts as `s_profile_window → s_ruck_prompt_window`. When the user accepts resume, both are popped (profile without animation, prompt with animation) to reveal the rucking screen underneath.

### Ruck Prompt Modes

`s_ruck_prompt_window` is one shared `Window` + custom-drawn `Layer` (not a native `ActionMenu`/`MenuLayer` — no SDK title slot exists for this style, so the "RuckPebble" heading below is hand-drawn). One draw function, one click-config, one select function serve all 4 modes; mode only selects which title array and row→action mapping applies (`prv_ruck_prompt_layer_update_proc`, `prv_ruck_prompt_select`, `ruckpebble.c`).

Every mode shows a "RuckPebble" title heading above the items. Item labels are single words (the title supplies the "ruck" context). Each mode keeps its own item order — they are **not** unified:

| Mode | Trigger | Item order |
|---|---|---|
| DOWN | Down button pressed on the rucking screen | Save, Resume, Discard |
| BACK | Back button pressed on the rucking screen | Discard, Save, Resume |
| CHECKIN | App not opened for 1 min (wakeup-scheduled relaunch), or no step-count change for 2 min while foregrounded (stillness check) | Resume, Discard, Save |
| RESTORE | In-progress session found on launch | Resume, New |

CHECKIN has two distinct triggers that share one mode and one item order:
- **1-minute app-not-opened**: on `prv_deinit` mid-session, a `wakeup_schedule` is set for `RUCK_CHECKIN_INTERVAL_S` (60s); on relaunch, if `launch_reason() == APP_LAUNCH_WAKEUP`, mode is set to CHECKIN instead of RESTORE.
- **2-minute no-steps**: `prv_check_ruck_stillness`, polled every second from the tick handler, pushes the CHECKIN prompt once step count hasn't changed for `RUCK_STILLNESS_TIMEOUT_S` (120s) while a session is active and foregrounded.

Because CHECKIN can fire right after a relaunch (no live session in memory yet), its Resume and Save actions first reload the persisted in-progress session before acting — Discard doesn't need to.

Row-index meaning differs per mode (e.g. row 0 is Save in DOWN but Discard in BACK) — the default highlighted row and the physical Back-button-inside-the-prompt shortcut (`prv_ruck_prompt_resume_row`) both account for this per mode.

### Persistent Storage Keys

| Key | Persist Key | Contents |
|-----|-------------|----------|
| Settings struct | 1 | All user settings |
| Lifetime distance (m) | 2 | Running total |
| Lifetime calories | 3 | Running total |
| Last activity distance (m) | 4 | Most recent ruck |
| Last activity calories | 5 | Most recent ruck |
| Last activity pace (sec/km) | 6 | Most recent ruck |
| Last activity duration (sec) | 7 | Most recent ruck |
| Last activity timestamp | 8 | Unix timestamp |
| Schema version | 9 | Currently 2 |
| Session in-progress flag | 10 | 0/1 |
| Session resume: start time | 11 | |
| Session resume: distance (m) | 12 | |
| Session resume: calories | 13 | |
| Session resume: elapsed (s) | 14 | |
| Session resume: profile index | 15 | |
| Session resume: steps | 16 | |

### Settings Struct

```c
typedef struct {
  int32_t weight_value;       // body weight in tenths (800 = 80.0 kg)
  int32_t weight_unit;        // 0=kg, 1=lb
  int32_t ruck_weight_unit;   // 0=kg, 1=lb (independent of weight_unit)
  int32_t stride_value;       // stride length in tenths (780 = 78.0 cm)
  int32_t stride_unit;        // 0=cm, 1=in
  int32_t sim_steps_enabled;  // 0/1 (emulator step simulation)
  int32_t sim_steps_spm;      // steps/min for simulation (default 122)
  int32_t active_profile;     // 0..2
  ProfileSettings profiles[3];
  char profile_names[3][33];
  char profile_terrain_types[3][16];
} Settings;

typedef struct {
  int32_t ruck_weight_value;  // kg-tenths (136 = 13.6 kg ≈ 30 lb)
  int32_t terrain_factor;     // hundredths (100=road, 120=gravel, 130=mixed, 150=sand/snow)
  int32_t grade_percent;      // tenths (20 = 2.0%)
} ProfileSettings;
```

**Critical invariant:** `ruck_weight_value` is always stored internally in **kg-tenths**, regardless of `ruck_weight_unit`. Conversion to display units happens at render time only.

### Rucking Screen Layout

The main screen (`s_window`) is a fixed-grid layout:

```
┌─────────────────────────────┐
│  [time]         [pace/hr]   │  top row: profile name + pace top-right
│  [profile name]  [session]  │
├──────────┬─────────┬────────┤
│  👟 steps│ 🏃 dist │ ❤️ hr  │  middle row: 3 columns
│  session │         │        │
│  today   │         │        │
├──────────┴─────────┴────────┤
│  🔥 kcal           ⏱ elapsed│  bottom row: 2 columns
│  ruck kcal                  │
│  walk kcal                  │
└─────────────────────────────┘
```

### Step Counting

Steps come from `HealthEventMovementUpdate` (Pebble Health). The app maintains a 60-second rolling history (`s_step_history[]`) to compute current pace. Steps are updated roughly every 10 seconds. Cadence logging is always active.

### Calorie Formula

Based on a modified Pandolf load-carriage equation:

```
V = speed (m/s)
G = grade (fraction, e.g. 0.02 for 2%)
T = terrain factor (e.g. 1.0 for road, 1.2 for gravel)
w = body weight (kg)
l = ruck weight (kg)
tot = w + l

inner = 1.5·V² + 0.35·V·G
mult  = (1 + √(0.3·V²)/7 + (V·l/w)²/4) × 1.1
k     = (1.5·w + 2·tot·(l/w)² + T·tot·inner·mult) × 3600 / 4184  [kcal/hr]

walk_kcal_hr = (3.5 + 0.1·(speed_m_per_min)) × w × 60 / 200
```

If the ruck result is lower than walking baseline (can happen at very low loads), it's clamped: `k = walk + walk·(l/w)`.

### AppMessage Keys

Keys are assigned numerically from 10000 upward in the order they appear in `package.json`. String keys (names, terrain types) are sent as strings; all others as integers.

---

## Layer 2: Phone Companion JS

**File:** `ruckpebble/src/pkjs/index.js`

Runs in the Pebble phone app's JavaScript runtime (PebbleKit JS).

### Responsibilities

- **`ready` event:** Load settings from localStorage, normalize, sync to watch. Request lifetime totals from watch.
- **`showConfiguration` event:** Open the GitHub Pages config URL with current settings serialized as `?data=URL_ENCODED_JSON`.
- **`webviewclosed` event:** Parse returned settings, merge with current (protecting live totals from being overwritten), save to localStorage, sync to watch.
- **`appmessage` event:** Handle watch→phone messages: save activity data, trigger timeline pin insert on new saves.

### Settings Storage

Key: `ruck_settings_v2` in Pebble's `localStorage`.

Settings are always stored **normalized** — all values are integers, terrain types are canonical strings, and unit fields are consistent.

### Lifetime Totals Protocol

On `ready`, the phone sends `request_lifetime_totals: 1` to the watch (up to 3 attempts, 1.2s timeout each). The watch responds with current totals via `appmessage`. This allows the settings page to show accurate history when opened.

### Timeline Pins

When the watch saves a ruck (sends `insert_timeline_pin: 1`), the JS creates a Pebble Timeline pin via the Timeline API (`timeline-api.getpebble.com`) with the ruck summary (distance, calories, pace, duration).

---

## Layer 3: Settings Page

**File:** `ruckpebble/docs/config.html`

Hosted on **GitHub Pages** at `https://gordonbazeley.github.io/ruckpebble/config.html`.

### URL Protocol

- **Opened with:** `?data=URL_ENCODED_JSON`; `return_to=URL_ENCODED_CALLBACK` is optional
- **Saves via:** Navigates to `return_to + URL_ENCODED_JSON_PAYLOAD` when supplied, otherwise `pebblejs://close#URL_ENCODED_JSON_PAYLOAD`
- Production uses the default `pebblejs://close#` path. `open_config.js` injects `return_to` so saves round-trip through its local HTTP server.

### Tabs

| Tab | Contents |
|-----|----------|
| Profiles | "About you" (body weight, units, stride) + 3 profile cards |
| Calories | Bar chart of estimated kcal/hr per profile vs. walking baseline |
| History | Lifetime totals + last ruck summary (read-only) |

### Settings Data Flow in the Page

```
URL ?data= param
     ↓
DEFAULTS merged with loaded state → INITIAL_STATE
     ↓
draftProfiles[] (array of 3 objects, modified in-place as user edits)
     ↓
saveAll() → serializes back to JSON → navigates to return_to or pebblejs://close#
```

**All ruck weights** in `draftProfiles` are stored as **kg-tenths** internally. The display unit (lb/kg) is controlled by `ruck_weight_unit` and conversion happens only at render/sync time via `syncProfile()`.

---

## Dev Tooling

### `open_config.js`

Node.js script that simulates the Pebble phone environment locally:
- Loads the built JS bundle (`build/pebble-js-app.js`)
- Fires `ready` and `showConfiguration` events
- When `showConfiguration` calls `Pebble.openURL()` with the GitHub Pages URL, it reads the local `docs/config.html` instead
- Serves on a local HTTP port, opens in Brave
- On save, fires `webviewclosed`, persists settings to localStorage file, optionally syncs to watch via `pebble send-app-message`

### `create_screenshots.py`

Python (Pillow) script that generates annotated explainer images for the app store / README:
- Reads `resources/screenshot_descriptions.md` (multi-document YAML-ish format, `---` separated)
- Watch screens (Profile, Tracking): places actual screenshot in a watch bezel frame, draws callout boxes with connector lines
- Settings sections (About you, Profiles, Calories, History): each section is its own output image, real browser screenshots centered with callout on alternating sides
- Output: `resources/explainer_screenshots/`

### `run.sh` / `logs.sh`

Shell helpers for building and running in the Pebble emulator.

---

## Build System

- Pebble SDK 3, platform: `emery` (Pebble Time Round)
- Build command: `pebble build`
- The SDK venv requires `python3.13` — if broken, symlink: `.venv/bin/python → python3.13`
- `wscript` is the waf build configuration (SDK-generated, not manually edited)
