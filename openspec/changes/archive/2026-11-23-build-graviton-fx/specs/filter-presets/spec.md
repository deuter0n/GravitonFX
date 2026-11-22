# Spec Delta

## Purpose

Lets users save and switch between named sets of filter settings (cutoff, slope and gain), such as one for racing and one for movies, starting from a locked built-in Default.

## ADDED Requirements

### Requirement: Preset contents
A preset SHALL store exactly the low-pass cutoff, low-pass slope and gain. Device, driver, buffer size, source, subsonic filter, limiter and other engine options SHALL NOT be part of a preset, so switching presets never reopens devices or changes hardware protection.

#### Scenario: Switching presets while running
- **WHEN** the user switches from one preset to another while the engine is Running
- **THEN** cutoff, slope and gain change smoothly and no device is reopened

### Requirement: Built-in Default preset
A preset named "Default" (cutoff 80 Hz, slope 24 dB/oct, gain 0 dB) SHALL always exist, always be listed first, and be marked as built in. It SHALL NOT be overwritten, renamed or deleted.

#### Scenario: Modifying Default
- **WHEN** Default is active and the user changes the cutoff
- **THEN** the Save action offers "Save as new" instead of overwriting Default

### Requirement: Create preset
The user SHALL be able to save the current filter settings as a new preset with a name of 1–24 characters. Names SHALL be unique, ignoring case. The new preset SHALL become active.

#### Scenario: Save as new
- **WHEN** the user chooses "Save current as new preset" and enters "Sim racing"
- **THEN** a preset "Sim racing" with the current values is created and becomes active

#### Scenario: Duplicate name rejected
- **WHEN** the user tries to name a new preset "default"
- **THEN** the name is rejected with a message that a preset with that name already exists

### Requirement: Modified state
When the current cutoff, slope or gain differs from the active preset, the UI SHALL mark the preset as modified and offer Save (or Save as new for Default) and Revert.

#### Scenario: Revert
- **WHEN** the active preset is modified and the user presses Revert
- **THEN** cutoff, slope and gain return to the preset's stored values and the modified mark disappears

#### Scenario: Save user preset
- **WHEN** a user preset is modified and the user presses Save
- **THEN** the preset stores the current values and the modified mark disappears

### Requirement: Manage presets
From the Settings page the user SHALL be able to use, rename, duplicate and delete user presets. Deleting SHALL require a second confirming click. Deleting the active preset SHALL activate Default. Default SHALL support only use and duplicate.

#### Scenario: Delete active preset
- **WHEN** the user deletes the active preset and confirms
- **THEN** the preset is removed and Default becomes active with its values applied

#### Scenario: Duplicate
- **WHEN** the user duplicates "Sim racing"
- **THEN** a preset "Sim racing copy" with the same values is created

### Requirement: Preset persistence
Presets and the active preset SHALL be saved with the application settings and restored on launch.

#### Scenario: Restart
- **WHEN** the user creates "Movies", makes it active and restarts the app
- **THEN** "Movies" exists and is active with its stored values
