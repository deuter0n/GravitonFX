# Spec Delta

## Purpose

Lets users know when a newer Graviton FX release is available on GitHub and takes them to it, without collecting data or downloading anything on its own.

## ADDED Requirements

### Requirement: Release source
The system SHALL determine the latest version from the latest published, non-prerelease GitHub Release of `deuter0n/GravitonFX`, reading its tag (for example `v0.2.0`), and SHALL compare it with the installed version using semantic versioning. Draft and prerelease releases SHALL be ignored.

#### Scenario: Newer release
- **WHEN** the installed version is 0.1.0 and the latest release tag is `v0.2.0`
- **THEN** an update is available

#### Scenario: Prerelease ignored
- **WHEN** the newest release on GitHub is a prerelease `v0.3.0-beta` and the latest regular release is `v0.1.0`
- **THEN** no update is reported to a user on 0.1.0

### Requirement: Automatic check
When "Check for updates automatically" is on (the default), the system SHALL check about 10 seconds after launch and then once every 24 hours while running. When it is off, the system SHALL make no automatic network requests.

#### Scenario: Setting off
- **WHEN** the user turns off "Check for updates automatically" and restarts the app
- **THEN** the app makes no request to GitHub until the user presses Check now

### Requirement: Manual check
Settings › About SHALL offer Check now, which checks immediately regardless of the setting and reports the result: checking, up to date (with the installed version), update available (with both versions), or could not reach GitHub.

#### Scenario: Up to date
- **WHEN** the user presses Check now and no newer release exists
- **THEN** About shows "You're up to date (0.1.0)."

#### Scenario: Offline
- **WHEN** the user presses Check now without an internet connection
- **THEN** About shows "Couldn't reach GitHub. Try again later." and nothing else is affected

### Requirement: Notification
When a newer version is found, the system SHALL show an amber dot on the Settings button, show "Update available: <latest>. You have <installed>." with a View release button in About, and show one Windows notification from the tray. The Windows notification SHALL appear only once per new version. Automatic checks that fail SHALL stay silent.

#### Scenario: First detection
- **WHEN** the daily check finds 0.2.0 for the first time
- **THEN** a tray notification says "Graviton FX 0.2.0 is available", and the Settings dot and About message appear

#### Scenario: No repeat
- **WHEN** later checks still find 0.2.0
- **THEN** no further notification appears, while the dot and About message remain

### Requirement: Opening the release
View release and clicking the notification SHALL open the release page on GitHub in the default browser. The app SHALL NOT download or install updates itself.

#### Scenario: View release
- **WHEN** the user presses View release
- **THEN** `https://github.com/deuter0n/GravitonFX/releases/latest` opens in the default browser

### Requirement: Privacy and safety
Update checks SHALL contact only `api.github.com` over HTTPS, SHALL send no data beyond the request itself and a `User-Agent` of `GravitonFX/<version>`, and SHALL run outside the audio threads so they can never affect audio.

#### Scenario: Slow network
- **WHEN** GitHub takes 20 seconds to respond while the engine is Running
- **THEN** the shaker output is unaffected and the check times out quietly
