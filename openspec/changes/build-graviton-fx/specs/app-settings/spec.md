# Spec Delta

## Purpose

Defines how Graviton FX remembers its configuration between runs and how it integrates with Windows: automatic saving, startup options, the system tray and single-instance behavior.

## ADDED Requirements

### Requirement: Automatic saving
The system SHALL save all settings and presets to `%APPDATA%\GravitonFX\settings.json`, creating the folder if needed, automatically after each change. The UI SHALL NOT expose the file location; the About section states that settings are saved automatically.

#### Scenario: Change is saved
- **WHEN** the user changes the buffer size and the app is then terminated unexpectedly
- **THEN** the new buffer size is restored on next launch

### Requirement: Restore on launch
On launch the system SHALL restore every saved setting: source mode, capture device or app, shaker driver and device, buffer size, subsonic filter and slope, limiter, the filter values, the active preset, the presets, the application options (including the update check), and the last version the user was notified about. The automatic safety reserve SHALL NOT be saved; it starts at 3 ms each time.

#### Scenario: Full restore
- **WHEN** the user configures the app, quits and relaunches it
- **THEN** every saved setting matches what it was before quitting

### Requirement: Safe file handling
A missing, unreadable or invalid settings file SHALL NOT prevent the app from starting. The system SHALL start with defaults, keep a copy of the invalid file, and keep any valid parts it can read. Saving SHALL NOT leave a partially written file if the app stops during the write.

#### Scenario: Corrupted file
- **WHEN** settings.json contains invalid JSON at launch
- **THEN** the app starts with defaults and the bad file is kept as settings.json.bak

### Requirement: Defaults
The defaults SHALL be: System mix on Windows default, WASAPI Exclusive, 64-sample buffer, subsonic filter 20 Hz with Gentle · 12 dB/oct slope, limiter on, Default preset (80 Hz, 24 dB/oct, 0 dB), start with Windows on, start in the tray with Windows off, start engine on launch off, close button hides to tray on, check for updates automatically on.

#### Scenario: First run
- **WHEN** the app runs for the first time
- **THEN** all settings have these default values

### Requirement: Reset to defaults
Reset to defaults SHALL require a confirming second click, SHALL restore every setting to its default value and make Default the active preset, and SHALL keep user presets.

#### Scenario: Reset keeps presets
- **WHEN** the user has two presets and resets to defaults
- **THEN** both presets still exist and Default is active

### Requirement: Start with Windows
When "Start with Windows" is on, the system SHALL register itself to launch when the current user signs in. When it is off, it SHALL remove that registration. No administrator rights SHALL be required.

#### Scenario: Toggle on
- **WHEN** the user turns on "Start with Windows" and signs out and back in
- **THEN** Graviton FX launches automatically

### Requirement: Startup behavior
"Start in the tray with Windows" SHALL make the app show only its tray icon when Windows starts it at sign-in; opening it by hand SHALL always show the window. "Start engine on launch" SHALL start the engine automatically with the restored settings.

#### Scenario: Silent autostart
- **WHEN** both options are on and Windows starts
- **THEN** the app starts in the tray and the engine starts without the window appearing

### Requirement: App icon
The application SHALL use the "well and ball" icon (concept A: an amber ball sitting inside concentric amber rings, on a dark rounded tile) for its executable, window, taskbar and Alt-Tab, provided at 16, 20, 24, 32, 48, 64, 128 and 256 px in one icon file. Sizes of 24 px and below SHALL use the simplified drawing (two thick rings instead of three).

#### Scenario: High-DPI display
- **WHEN** the display is set to 150% or 200% scaling
- **THEN** the taskbar and Alt-Tab icons are crisp, with no visible upscaling blur

#### Scenario: Small size
- **WHEN** the icon is shown at 16 px
- **THEN** the simplified drawing is used and the rings and ball remain distinguishable

### Requirement: Tray icon shows the engine state
The tray icon SHALL use the well-and-ball drawing without its tile and SHALL change with the engine state: amber when Running, Starting or Switching, grey when Stopped, amber with a blue dot when Waiting, and amber with a red dot in Error or Check output. It SHALL update as soon as the state changes.

#### Scenario: Engine waiting
- **WHEN** the engine enters Waiting because the shaker DAC is switched off
- **THEN** the tray icon shows the blue dot, and returns to plain amber when the engine is Running again

#### Scenario: Engine stopped
- **WHEN** the user stops the engine from the tray menu
- **THEN** the tray icon turns grey

### Requirement: System tray
The system SHALL show a tray icon whose tooltip includes the engine state. Clicking the icon SHALL show the window. The tray menu SHALL offer Open, Start/Stop and Quit. Minimizing the window SHALL hide it to the tray.

#### Scenario: Restore from tray
- **WHEN** the window is hidden and the user clicks the tray icon
- **THEN** the window is shown and focused

#### Scenario: Quit from tray
- **WHEN** the user chooses Quit from the tray menu
- **THEN** the engine stops, settings are saved and the app exits

### Requirement: Close behavior
When "Close button hides to tray" is on, closing the window SHALL hide it and leave the engine running. When it is off, closing the window SHALL quit the app.

#### Scenario: Close with hide to tray on
- **WHEN** the option is on and the user clicks the window's close button while Running
- **THEN** the window hides and the shaker keeps running

### Requirement: Single instance
Only one instance of the app SHALL run at a time. Launching it again SHALL show the existing window.

#### Scenario: Second launch
- **WHEN** the app is running in the tray and the user launches it again
- **THEN** no second instance starts and the existing window is shown
