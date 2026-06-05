# Profile Screen
## Header
- title: Profile Selection
- subtitle: You'll use more calories depending on the weight of your ruck,
            the terrain you're moving across and the gradient of your ruck.
            Select a profile which matches today's ruck.

## Profiles
- description: You can define up to three profiles which define ruck weight, terrain and gradient.
            They'll impact the number of calories burnt when rucking.

## Footer
- title: How to use
- subtitle: ↑ ↓ Move between profiles
            •   Start ruck with selected profile
            ←   Exit to Pebble menu

---

# Tracking Screen
## Header
- title: Rucking Screen
- subtitle: There's a lot happening here, this screenshot explains everything.

## Active Profile
- description: Name of the profile selected on the profile screen.

## Session Pace
- description: Overall pace for the session per kilometre or mile.

## Current Pace
- description: Pace per kilometre or mile over last minute.

## Steps
- description: The first line shows steps for the current ruck session. 
               The second line shows total steps for today.
               NB. Steps are from Pebble health and are updated every ten seconds or so.

## Watch Time
- description: Current time.

## Distance
- description: Total distance for this ruck session.

## Heart Rate
- description: Latest Pebble Health heart-rate reading when available. Shows -- when no recent reading exists.

## Elapsed Time
- description: Time elapsed for the active ruck session.

## Calories
- description: The first line shows the estimated calories for selected profile. 
               The second line shows calories for an unweighted walk for comparison.

## Footer
- title: How to use
- subtitle: ↑ Pauses the current ruck
            ↓ Saves the current ruck session and creates a timeline pin
            ← Discard current ruck session and return to profile screen

---

# JavaScript Settings Page
## Header
- title: Ruck Settings
- subtitle: Set up your ruck profiles.

## Shared
- description: Settings for body weight, ruck weight unit, and stride length.

## Profile 1
- description: Sets the name, ruck weight, terrain, and grade used when this profile is selected on the watch.
               * Ruck weight - The more weight you carry the more calories you burn.
               * Terrain - The trickier the terrain the more calories you burn.
               * Grade - The steeper the terrain the more calories you burn. 
                 * But … on a ruck you won't be going up a constant grade. 
                 * Instead if your mapping software includes an "energy equivalent grade" you can use that.
                 * Or calculate it yourself as Total ascent (metres) / (0.5 × Distance (kilometres)

## Profile 2
- description: Second profile with the same fields as Profile 1. Use it for an alternate ruck setup such as a different ruck weight or terrain.

## Profile 3
- description: Third profile card with the same fields as the other profiles. 

## Tracked Totals
- description: Lifetime totals for distance and calories accumulated from all saved rucks.

## Last Activity
- description: The most recent ruck saved from the watch, including date, distance, pace, and calories.

## Footer
- title: How to use
- subtitle: Save - Write the settings back to the watch
            Reset - Restores the default settings
