# Spec Delta

## Purpose

Defines how captured audio becomes the shaker signal (mono sum, subsonic filter, low-pass filter, gain, limiter), the level and spectrum measurements the UI displays, and the pulse and sweep test signals.

## ADDED Requirements

### Requirement: Mono sum
The system SHALL sum all captured channels, including the LFE channel of surround formats, into one mono signal before filtering.

#### Scenario: Stereo bass on one side
- **WHEN** a bass effect plays only on the left channel of the source
- **THEN** it is present in the shaker signal

#### Scenario: Surround LFE
- **WHEN** the captured device outputs 5.1 and an explosion plays only on the LFE channel
- **THEN** it is present in the shaker signal

### Requirement: Low-pass filter
The system SHALL apply a Butterworth low-pass filter with a cutoff adjustable from 20 Hz to 200 Hz in 1 Hz steps and a slope selectable from 12, 24, 36 or 48 dB/octave. At the cutoff frequency the response SHALL be -3 dB, relative to the passband, within ±0.5 dB.

#### Scenario: Content above cutoff is attenuated
- **WHEN** cutoff is 80 Hz and slope is 24 dB/oct, and a 320 Hz tone is captured
- **THEN** the tone is attenuated by at least 45 dB relative to the passband

#### Scenario: Content below cutoff passes
- **WHEN** cutoff is 80 Hz, the subsonic filter is at 20 Hz, and a 45 Hz tone is captured
- **THEN** the tone passes with less than 1 dB attenuation

### Requirement: Subsonic filter
The system SHALL apply a Butterworth high-pass ("subsonic") filter to everything sent to the shaker. Its frequency SHALL be selectable as Off, 10, 15, 20 or 30 Hz (default 20 Hz), and its slope as Gentle · 12 dB/oct (default) or Steep · 24 dB/oct. It is a device-protection setting, not part of presets.

#### Scenario: Default protects a 20–200 Hz shaker
- **WHEN** the subsonic filter is at its defaults and a 10 Hz tone is captured
- **THEN** the tone is attenuated by at least 11 dB

#### Scenario: Steep slope
- **WHEN** the slope is Steep and a 10 Hz tone is captured
- **THEN** the tone is attenuated by at least 23 dB

#### Scenario: Off
- **WHEN** the subsonic filter is Off
- **THEN** no high-pass filtering is applied and the slope option is disabled

### Requirement: Output gain
The system SHALL apply an output gain adjustable from -12 dB to +12 dB in 0.5 dB steps after the filters.

#### Scenario: Gain applied
- **WHEN** gain is set to +6 dB
- **THEN** the shaker signal level is 6 dB higher than at 0 dB gain for the same input

### Requirement: Output limiter
When the limiter is enabled, the system SHALL keep the shaker signal at or below -1 dBFS without adding latency. The limiter SHALL be enabled by default and SHALL be switchable in Settings.

#### Scenario: Loud peak with limiter on
- **WHEN** the limiter is on and the gain would push a peak to +3 dBFS
- **THEN** the output peak does not exceed -1 dBFS

#### Scenario: Limiter off
- **WHEN** the limiter is off and the gain pushes the signal above 0 dBFS
- **THEN** the output clips and the UI reports clipping

### Requirement: Click-free parameter changes
Changing cutoff, slope, gain, subsonic frequency or slope, or the limiter while audio is flowing SHALL take effect within 50 ms without producing an audible or tactile click, thump or dropout.

#### Scenario: Slope switched during playback
- **WHEN** the low-pass slope is changed from 12 to 48 dB/oct while bass is playing
- **THEN** the change takes effect smoothly with no transient spike in the output

#### Scenario: Cutoff dragged quickly
- **WHEN** the cutoff is swept from 20 Hz to 200 Hz in under one second while bass is playing
- **THEN** the output changes continuously with no clicks

### Requirement: Level measurement
The system SHALL measure the peak levels of the captured input and of the shaker output and provide them to the UI at least 25 times per second, along with whether the limiter engaged or the output clipped.

#### Scenario: Meters follow audio
- **WHEN** a loud bass hit occurs
- **THEN** the level history and the IN/OUT readout show it within 50 ms

### Requirement: No-signal detection
While the engine is Running, the system SHALL report "no signal" when the captured input has stayed below -60 dBFS for 2 seconds, and SHALL clear it as soon as input returns.

#### Scenario: Game paused
- **WHEN** the selected app goes quiet for 3 seconds
- **THEN** the UI shows "No signal", and it shows "Active" again as soon as audio returns

### Requirement: Spectrum measurement
The system SHALL provide the UI with the spectrum of the captured input and of the shaker output as 64 log-spaced bands from 10 Hz to 2 kHz, in dBFS, at least 25 times per second. Producing this data SHALL NOT add latency to the shaker signal.

#### Scenario: Filter visible in spectrum
- **WHEN** music plays with cutoff at 80 Hz
- **THEN** the output spectrum falls away above about 80 Hz while the input spectrum continues to show content up to 2 kHz

### Requirement: Test signals pass through the protection stages
The pulse and sweep test signals SHALL be generated by the app (never captured), SHALL bypass the low-pass filter, and SHALL pass through the subsonic filter, gain and limiter. They SHALL appear on the OUT meter and output spectrum only. They SHALL start and stop with short fades so they never click.

#### Scenario: Test signal not shown as input
- **WHEN** a test signal plays
- **THEN** the input level and input spectrum do not show it

### Requirement: Hold-to-play pulse
The user SHALL be able to play a 40 Hz test pulse for as long as they hold the Pulse control (pointer, touch, Space or Enter), stopping on release, with an automatic stop after 10 seconds.

#### Scenario: Hold and release
- **WHEN** the user holds Pulse for 2 seconds and releases
- **THEN** the shaker vibrates at 40 Hz for about 2 seconds and stops on release

#### Scenario: Safety limit
- **WHEN** the release is never received
- **THEN** the pulse stops after 10 seconds

### Requirement: Stepped sweep
The user SHALL be able to run a sweep that steps from 20 Hz to 200 Hz in 5 Hz steps, holding each step for 0.5 seconds, with phase-continuous transitions between steps. Pressing Sweep again SHALL stop it early. The current step frequency SHALL be reported to the UI.

#### Scenario: Finding the chair's resonance
- **WHEN** the user starts the sweep
- **THEN** the shaker plays 20, 25, 30 … 200 Hz for half a second each, about 18.5 seconds in total, and the UI shows the current frequency

### Requirement: Test signals work while stopped
When the engine is stopped, starting a test signal SHALL open only the shaker output, without capture, and SHALL release it about 2 seconds after the test ends; a new test within that time reuses it, so pressing Pulse repeatedly never reopens the device. Only one test signal SHALL play at a time. If the shaker output cannot be opened, the reason SHALL be shown under the shaker device selector for about 4 seconds.

#### Scenario: Checking the wiring before starting
- **WHEN** the engine is stopped and the user holds Pulse
- **THEN** the shaker vibrates, the engine status stays Stopped, and the device is released about 2 seconds after release

#### Scenario: Device unavailable
- **WHEN** the engine is stopped, the shaker device is unplugged, and the user presses Sweep
- **THEN** no sound plays and the message under the shaker device selector reads "Not connected."
