# RuckPebble Decision Log

Decisions that aren't obvious from the code, with the reasoning behind them.

---

## Settings hosted on GitHub Pages (not embedded in the app)

**Decision:** The settings page is served from `https://gordonbazeley.github.io/ruckpebble/config.html` rather than embedded in the JS bundle as a `data:text/html,` URI.

**Why:** The Pebble AppMessage payload limit meant that a sufficiently rich settings UI (3 profiles, calorie chart, history) could not fit in a base64-encoded data URI within the `Pebble.openURL()` limit (~37 KB was too large). GitHub Pages gives unlimited HTML/CSS/JS with zero backend infrastructure and automatic HTTPS, which is required for `Pebble.openURL()`.

**Trade-off:** Requires internet connection to open settings. Local `open_config.js` dev tool reads `docs/config.html` directly to avoid this during development.

---

## Ruck weight always stored in kg-tenths internally

**Decision:** `ruck_weight_value` is always stored as an integer in kg-tenths (e.g. 136 = 13.6 kg ≈ 30 lb), regardless of the user's `ruck_weight_unit` preference.

**Why:** Eliminates lossy round-trip conversions. If you store in display units, switching between lb and kg repeatedly introduces rounding drift. The calorie formula works in SI (kg), so kg-tenths is the natural canonical form. Conversion to the display unit happens only at render time.

**Implication:** When `open_config.js` reads old settings that were stored in lb-tenths (before this decision was enforced), the weights appear doubled. The fix was in `profileKcal` (removed the `×0.453592` that assumed lb-tenths input) and `profileDisplayName` (added the missing `/10` after `kgTenthsToLbTenths`).

---

## Ruck weight unit is independent of general units

**Decision:** `ruck_weight_unit` is a separate setting from `measurement_unit` (body weight / distance / pace units).

**Why:** Ruck plates are almost universally sold in lb, even in metric countries. A user who otherwise uses kg for body weight and km for distance will typically still think of their ruck plate as "30 lb". Coupling ruck weight to general units would force metric users to do mental maths every time.

---

## Three fixed profiles, not dynamic

**Decision:** Exactly 3 profiles, hardcoded everywhere (C struct, message keys, JS, settings page).

**Why:** The Pebble AppMessage protocol uses statically declared numeric keys. Supporting N profiles would require either pre-allocating keys for a max count or packing all profiles into a JSON blob string. Three is enough for the common case (light road, heavy road, trail) and keeps every layer simple.

**If this needs to change:** See `src/todo.md`. Pre-allocating up to 8 profiles in the C struct and `package.json` is the least-invasive path — avoids a protocol redesign and keeps persistent storage mostly compatible.

---

## grade_percent stored in tenths

**Decision:** `grade_percent` is stored as tenths of a percent (integer), not as a float or whole percent.

**Why:** Consistent with the rest of the codebase's approach to avoiding floats in persistent storage (all physical quantities are stored as integer tenths or hundredths). The Pebble C runtime has limited float performance, and integer arithmetic is cheaper and avoids serialization ambiguity. Display rounds to whole percent.

---

## terrain_factor stored in hundredths

**Decision:** `terrain_factor` is stored as hundredths (100 = 1.0×, 120 = 1.2×).

**Why:** Same reason as grade_percent — avoids floats in storage. The Pandolf formula uses the factor as a multiplier (T = terrain_factor / 100) so hundredths give one decimal place of precision, which is more than enough.

---

## Terrain represented as both type string and numeric factor

**Decision:** Both `terrain_type` (e.g. `"road"`) and `terrain_factor` (e.g. `100`) are stored.

**Why:** The Pebble C side only understands the numeric factor (it uses it directly in the formula). The JS/settings side needs the string to drive the `<select>` UI. Storing both avoids a lookup table on the watch side and a reverse-mapping on the JS side. `normalizeSettings()` ensures they stay in sync on every save.

---

## Calorie formula: modified Pandolf, not simple MET table

**Decision:** The app uses a modified Pandolf load-carriage formula rather than a simple MET-based estimate.

**Why:** MET tables for "walking with load" give a single multiplier that doesn't account for ruck weight, terrain, or grade independently. Pandolf's equation was developed specifically for military load carriage and accounts for all three variables in a physically motivated way. The modification (`mult` factor involving speed and load ratio) was added to produce more realistic numbers at higher speeds and higher loads.

**Limitation:** The formula assumes constant grade and constant terrain, which isn't true on real rucks. The recommended approach for grade input is "energy equivalent grade" = Total ascent (m) / (0.5 × Distance (km)), which averages out the gradient effect over the whole route.

---

## Walk calorie floor

**Decision:** If the formula produces ruck kcal/hr < walk kcal/hr, the result is clamped to `walk + walk*(l/w)`.

**Why:** At very low ruck weights (e.g. < 5 lb) the Pandolf formula can fall below the walking baseline due to model approximation. This shouldn't be surfaced to users — a ruck is always at least as hard as walking unloaded.

---

## Settings page saves via URL navigation, not fetch/XHR

**Decision:** The settings page saves by navigating to `return_to + payload` rather than using `fetch()` or `XMLHttpRequest`.

**Why:** This is the Pebble SDK's mandated protocol for configurable watch apps. The `webviewclosed` event on the phone fires when the webview navigates to the `pebblejs://close#...` scheme. GitHub Pages can't receive POSTs anyway.

---

## Lifetime totals owned by the watch, not the phone

**Decision:** The watch is the source of truth for `lifetime_distance_m_total`, `lifetime_calories_total`, and `last_activity_*`. The phone only stores them in localStorage as a cache.

**Why:** The watch increments these directly from session data at the moment of save. The phone receives them via `appmessage` and caches them so they can be displayed in the settings page History tab. The `webviewclosed` handler explicitly protects against the config form overwriting a fresher value with a stale one (takes `Math.max` for totals, keeps current if incoming timestamp is older).

---

## Event delegation for profile editor info buttons

**Decision:** Info buttons inside the dynamically-generated `#profile_list` HTML are handled via a single delegated listener on `#profile_list`, not per-element listeners.

**Why:** `renderProfileList()` replaces `innerHTML`, destroying all previously attached listeners. Per-element listeners added at render time would be the obvious approach, but the static-DOM `querySelectorAll(".info-btn")` call at init time was also wiring up buttons before `renderProfileList` ran — causing the dynamic buttons to get double-handled (two toggles cancel out). The delegation pattern survives re-renders and doesn't require cleanup.

---

## Schema version for persistent storage

**Decision:** A `APP_STATE_SCHEMA_VERSION` constant (currently 2) is stored at persist key 9. On load, if the stored version doesn't match, the C app can migrate or reset.

**Why:** The `Settings` struct size changes whenever fields are added. Pebble's persistent storage is raw binary. Without a version byte, old data loaded into a new struct layout causes silent corruption.

---

## `open_config.js` reads local `docs/config.html` for HTTPS URLs

**Decision:** When `Pebble.openURL()` fires with an `https://` URL (the production GitHub Pages URL), `open_config.js` reads `docs/config.html` from disk rather than fetching it over the network.

**Why:** The developer can test settings changes against their local `config.html` without deploying to GitHub Pages first. The script injects `return_to` pointing to its local HTTP server so saves still round-trip correctly through the mock environment.
