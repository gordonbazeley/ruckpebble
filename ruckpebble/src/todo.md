* Get notified if walk < ruck calories
>   - If you want the simplest thing that works: Discord webhook.
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

