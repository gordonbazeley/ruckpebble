# Profile Screen
## Header
- title: Profile chooser
- subtitle: Pick the active ruck profile before starting a session

## Profile 1
- description: First selectable profile. Shows the profile name on the top line and the weight, terrain, and grade settings below it.

## Profile 2
- description: Second selectable profile with the same three settings. Use Up or Down to move the highlight here and Select to start with it.

## Profile 3
- description: Third selectable profile. The same row layout keeps the list easy to scan and compare.

## Footer
- title: Select starts tracking
- subtitle: Use Up and Down to move, Select to begin rucking with the highlighted profile, and Back to exit without starting.

---

# Tracking Screen
## Header
- title: RuckPebble Tracking Screen
- subtitle: What each live field means while a ruck is in progress

## Active Profile
- description: Selected ruck profile. These settings drive ruck weight, terrain factor, grade, calorie maths and profile name.

## Current Pace
- description: Live pace per kilometre or mile. Shows --:-- until distance has moved beyond zero.

## Pace Tile
- description: Large pace readout for the current session, repeated for scanability while moving.

## Steps
- description: Top number is steps since this ruck started. Smaller number underneath is the watch's total steps for today.

## Watch Time
- description: Current watch time, kept visible so the tracking screen can replace your normal watchface during a ruck.

## Distance
- description: Total distance for this ruck, calculated from session steps multiplied by configured stride length.

## Heart Rate
- description: Latest Pebble Health heart-rate reading when available. Shows -- when no recent reading exists.

## Elapsed Time
- description: Timer for the active ruck session.

## Calories
- description: Top number is estimated loaded-ruck calories. Smaller number below is normal walking calories for comparison.

## Footer
- title: Down button save
- subtitle: Down button saves the current ruck snapshot and asks the phone companion to create a timeline pin.

---

# JavaScript Settings Page
## Header
- title: Ruck Settings
- subtitle: Shared watch settings, three profiles, totals, and the last activity summary

## Shared
- description: Phone-side settings for body weight, ruck weight unit, and stride length. These drive the calculations sent to the watch.

## Profile 1
- description: First profile card. Sets the name, ruck weight, terrain, and grade used when this profile is selected on the watch.

## Profile 2
- description: Second profile card with the same fields as Profile 1. Use it for an alternate ruck setup such as trail or hilly terrain.

## Profile 3
- description: Third profile card with the same fields as the other profiles. Keep it for another named setup or a default fallback.

## Tracked Totals
- description: Phone-side lifetime totals for distance and calories accumulated from all saved rucks.

## Last Activity
- description: The most recent ruck saved from the watch, including date, distance, pace, and calories.

## Footer
- title: Save and reset controls
- subtitle: Save writes the settings back to the watch companion. Reset restores the default settings before saving.
