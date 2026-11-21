# Spec Delta

## Purpose

Defines which audio Graviton FX listens to and how capture behaves when the chosen source appears, disappears, changes or falls silent.

## ADDED Requirements

### Requirement: Source modes
The system SHALL offer two capture source modes, labelled "System mix" and "App". System mix captures everything playing on one output device. App captures the audio of a single application and its child processes, regardless of which output device that application uses.

#### Scenario: System mix captures an output device
- **WHEN** the source mode is System mix and audio plays on the selected output device
- **THEN** that audio is captured, including audio from every application playing on that device

#### Scenario: App captures only that app
- **WHEN** the source mode is App with a music player selected, and a game is also playing audio
- **THEN** only the music player's audio is captured

### Requirement: System mix follows the Windows default
When System mix is set to "Windows default", the system SHALL capture the device that is currently the Windows default output and SHALL switch to the new default device when the user or Windows changes it.

#### Scenario: Default device changes while running
- **WHEN** the engine is running with System mix on "Windows default" and the Windows default output changes from speakers to a headset
- **THEN** capture moves to the headset without the user restarting the engine

### Requirement: System mix fixed device
The user SHALL be able to select a specific output device for System mix. The selection SHALL persist by the device's stable Windows endpoint identity and SHALL NOT change when the Windows default output changes.

#### Scenario: Fixed device ignores default changes
- **WHEN** System mix is set to "Rig Speakers" and the Windows default changes to a Bluetooth headset
- **THEN** capture stays on Rig Speakers

### Requirement: Device list contents
The capture device list SHALL show "Windows default" first, without a device name; hovering it, or the selector while it is chosen, SHALL show a tooltip "Current default: <name>". It SHALL be followed by every currently active output device, including the shaker's device. A device that is not connected SHALL be listed only while it is the current selection, by its plain name. The list SHALL NOT show a history of past devices.

#### Scenario: Normal list
- **WHEN** the user opens the capture device list and the selection is connected
- **THEN** it shows "Windows default" and the active output devices only, and hovering "Windows default" shows "Current default: <name>"

#### Scenario: Selected device unplugged
- **WHEN** the selected fixed device is unplugged
- **THEN** it stays listed and selected by its plain name, and it disappears from the list once the user selects another device

### Requirement: App list contents
In App mode the system SHALL list running applications that have an audio session, showing each one's name and executable and marking those that are currently silent, and SHALL let the user refresh this list. The selection SHALL persist by executable name. An app that is not running SHALL be listed only while it is the current selection.

#### Scenario: Selecting an app
- **WHEN** the user selects "Race Sim · RaceSim.exe"
- **THEN** the source is saved as RaceSim.exe and its audio is captured

#### Scenario: Refreshing the list
- **WHEN** a new application starts playing audio and the user presses refresh
- **THEN** that application appears in the list

#### Scenario: Switching to App mode for the first time
- **WHEN** no app has been chosen yet and the user switches to App mode
- **THEN** an app that is playing audio is selected (or the first listed app), or, with none running, the selector reads "Pick an app" and the message under it reads "Pick an app to capture."

### Requirement: Waiting for an unavailable source
When the selected app is not running or the selected fixed device is not connected, the system SHALL wait for it instead of failing, and SHALL resume capture automatically when it becomes available. A message under the source selector SHALL say what is being waited for, in a blue box, only in this situation, and the selector SHALL get a blue outline.

#### Scenario: App not running yet
- **WHEN** the engine starts with App set to RallyGame.exe and that game is not running
- **THEN** the engine enters the Waiting state and the message under the app selector reads "Not running. Capture starts when it opens."

#### Scenario: App starts later
- **WHEN** the engine is Waiting for RallyGame.exe and the game launches and plays audio
- **THEN** capture begins and the engine becomes Running without user action

#### Scenario: Fixed device unplugged and replugged
- **WHEN** the fixed capture device is unplugged while running and later plugged back in
- **THEN** the message under the device selector reads "Not connected. Capture resumes when it's back." and capture resumes when it returns

#### Scenario: Source available
- **WHEN** the selected source is available
- **THEN** no message shows under the source selector

### Requirement: Capture format display
In System mix the Source panel SHALL show the captured device's format as tags (small pills in one row): sample rate, sample format and channel layout, using "stereo", "5.1", "7.1" or "N ch", each with a tooltip naming it. In App mode the format tags SHALL NOT be shown.

#### Scenario: System mix format
- **WHEN** System mix captures a 48 kHz float stereo device
- **THEN** the Source panel shows the tags "48 kHz", "32-bit float" and "2 ch"

#### Scenario: App mode
- **WHEN** the source mode is App
- **THEN** no format tags are shown

### Requirement: Silence handling
When the source produces no audio, the system SHALL treat the gap as silence, keep the shaker output running with silence, and remain in the Running state. Silence SHALL NOT be reported as an error and SHALL NOT change latency once audio resumes. No user setting controls this behavior, and the system SHALL NOT play anything on the captured device to keep capture active.

#### Scenario: Nothing playing
- **WHEN** the engine is Running and all audio on the source stops for a minute, then resumes
- **THEN** the shaker receives silence during the gap, the status stays Running, and end-to-end latency after resuming matches latency before the gap

### Requirement: Per-app capture availability
If the operating system does not support per-application capture, the system SHALL report this clearly when App mode is used, and System mix SHALL remain fully available.

#### Scenario: Unsupported Windows version
- **WHEN** the user selects App on a Windows version older than 10 version 2004
- **THEN** the engine shows an error explaining that capturing one app needs Windows 10 version 2004 or later
