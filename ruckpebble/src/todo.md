## Multi profile
The current design has profiles hardcoded as a fixed-size 3-element C struct array (profiles[PROFILE_COUNT] where PROFILE_COUNT = 3), with matching
fixed keys in the pkjs message protocol (profile1_*, profile2_*, profile3_*). Making this dynamic is a significant change across four layers:

1. C struct: Replace profiles[3] with a dynamic count field + a flat array. The persistent storage schema would need versioning because
sizeof(Settings) changes.
2. C message keys: The Pebble AppMessage protocol uses static keys defined in package.json. You'd need to either pre-allocate keys for N max
profiles (e.g., profile4_* … profile8_*) or redesign the protocol to pack all profiles into one JSON blob string.
3. pkjs/index.js: defaults, normalizeSettings, syncSettingsToWatch, and openConfig all iterate profiles 1–3 explicitly. These would need to loop
over a dynamic array.
4. C profile menu: menu_layer callbacks (get_num_rows, draw_row) currently return/use PROFILE_COUNT. These would use s_settings.profile_count.

The least-invasive approach is to pre-allocate up to N (say 8) profiles in the C struct and add a profile_count field, while pre-allocating the
corresponding message keys in package.json. That avoids a protocol redesign and keeps persistent storage compatible (just expand the array and bump
a settings version byte).

## Get notified if walk < ruck calories
- If you want the simplest thing that works: Discord webhook.
- If you want the cleanest alerting UX: Pushover.
- If you want minimal infrastructure and can tolerate a tiny backend: Cloudflare Worker that forwards to Discord/ntfy/email.

What to send
Only send the first hit per session, otherwise you’ll spam yourself. Include:

- app version
- watch platform
- timestamp
- profile name
- load, body weight, speed
- walk kcal/h and ruck kcal/h
- the exact branch condition that triggered

Important constraint
Because this is a user-installed watch app, you should treat this as telemetry and make it opt-in or at least disclose it. If you want to notify
yourself from production installs, you need a clear privacy story.

Minimal-cost implementation

- Add a JS fetch() to a webhook URL
- Trigger it from the companion when the watch sends walk > ruck event
- Use a tiny Cloudflare Worker only if you don’t want your webhook URL exposed in the JS bundle

