python3 "$(dirname "$0")/../resources/create_screenshots.py" \
    --profile-screenshot /tmp/ruck_profile.png \
    --tracking-screenshot /tmp/ruck_tracking_raw.png \
    --settings-screenshot "$(dirname "$0")/../resources/settings_screenshot_raw.png" \
    --output-dir "$(dirname "$0")/../resources/explainer_screenshots"
