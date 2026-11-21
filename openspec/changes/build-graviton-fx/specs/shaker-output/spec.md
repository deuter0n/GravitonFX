# Spec Delta

## Purpose

Defines how Graviton FX drives the shaker device (driver choice, device list, format, buffer size and output channels) and how it keeps latency low and stable between two independently clocked devices without user tuning.

## ADDED Requirements

### Requirement: Output drivers
The system SHALL support shaker output through WASAPI in exclusive mode and through ASIO. The user SHALL choose the driver, and the device list SHALL show the devices or ASIO drivers available for that driver.

#### Scenario: Switching to ASIO
- **WHEN** the user switches the driver from WASAPI Exclusive to ASIO
- **THEN** the device list shows the installed ASIO drivers and the first available one is selected

### Requirement: Shaker device list contents
The shaker device list SHALL show currently available devices. A device that is not connected SHALL be listed only while it is the current selection, by its plain name. The list SHALL NOT show a history of past devices.

#### Scenario: Selected shaker device unplugged
- **WHEN** the selected shaker device is unplugged
- **THEN** it stays listed and selected by its plain name, and the message under it reads "Not connected. Output starts when it's back." with a blue dot

### Requirement: Exclusive use of the shaker device
While running, or while a test signal plays, the system SHALL hold the shaker device exclusively, so that no other application or system sound can play on it.

#### Scenario: System notification sound
- **WHEN** the engine is running and Windows plays a notification sound
- **THEN** the sound does not play on the shaker device

### Requirement: Native output format
The system SHALL open the shaker device at the capture source's sample rate when the device supports it, so only clock drift is corrected, and otherwise at the device's native rate, converting the rate internally. It SHALL use the device's native sample format. A source change while running SHALL NOT reopen the output just to match a new rate.

#### Scenario: Device takes the capture rate
- **WHEN** capture runs at 48 kHz, the shaker device's native rate is 44.1 kHz and it also supports 48 kHz
- **THEN** the device is opened at 48 kHz and the health panel's Rate shows "48 → 48"

#### Scenario: Different rates
- **WHEN** capture runs at 48 kHz and the shaker device supports only 44.1 kHz
- **THEN** the device is opened at 44.1 kHz and the health panel's Rate shows "48 → 44.1"

#### Scenario: Matching rates
- **WHEN** capture and shaker both run at 48 kHz
- **THEN** the health panel's Rate shows "48 native"

### Requirement: Output format display
The Shaker panel SHALL show the device's sample rate, sample format and channel layout as tags (small pills in one row), using "stereo" for 2 channels, "5.1" or "7.1" for surround layouts, and "N ch" otherwise.

#### Scenario: Stereo USB DAC
- **WHEN** the shaker device is a 44.1 kHz 24-bit stereo DAC
- **THEN** the Shaker panel shows the tags "44.1 kHz", "24-bit" and "stereo"

### Requirement: Output channels
The mono shaker signal SHALL always be sent to output channels 1 and 2 (left and right) with no user setting. All other channels SHALL be silent. For a device with more than 2 channels, the Shaker panel SHALL add an "out 1–2" tag after the format tags, with the tooltip "Shaker signal on outputs 1–2 of N".

#### Scenario: One shaker on the right amp channel
- **WHEN** a single shaker is connected to the right channel of a stereo amp
- **THEN** it receives the full shaker signal

#### Scenario: Eight-output interface
- **WHEN** the shaker device is an 8-output ASIO interface
- **THEN** only outputs 1 and 2 carry the signal and the Shaker panel shows the tags "8 ch" and "out 1–2", the latter explaining "Shaker signal on outputs 1–2 of 8" on hover

### Requirement: Buffer size
The user SHALL be able to choose an output buffer size of 32, 64, 128 or 256 samples, and the UI SHALL show the resulting buffer time in milliseconds.

#### Scenario: Buffer time display
- **WHEN** 64 samples is selected on a 44.1 kHz device
- **THEN** the buffer selector shows "64 samples · 1.45 ms"

### Requirement: Clock drift compensation
The system SHALL compensate for clock differences between the capture and output devices, so that end-to-end latency stays within ±1 ms of its settled value during continuous operation of at least 4 hours, without periodic dropouts or repeated audio.

#### Scenario: Long sim-racing session
- **WHEN** the engine runs for 4 hours with two devices whose clocks differ by 50 ppm
- **THEN** latency stays within ±1 ms of its settled value and no underruns are caused by drift

#### Scenario: Settling after start
- **WHEN** the engine starts or switches devices
- **THEN** the health panel shows "Settling" until drift compensation locks, then "Locked", typically within 10 seconds

### Requirement: Automatic safety reserve
The system SHALL keep a safety reserve of audio between capture and output that starts at 3 ms each time the engine starts, grows by 1 ms after each underrun up to 6 ms, and is not user-configurable. Growing the reserve SHALL NOT interrupt output.

#### Scenario: Busy PC
- **WHEN** an underrun occurs while running
- **THEN** the reserve grows by 1 ms without a further dropout, and the latency in the Engine health panel rises accordingly

#### Scenario: Fresh start
- **WHEN** the engine is stopped and started again
- **THEN** the reserve is 3 ms

### Requirement: Latency and health reporting
While running, the system SHALL report end-to-end latency (ms), clock drift (ppm), ring-buffer fill as a percentage (target: half), the dropout count since the engine started, and the lock state to the UI at least 25 times per second.

#### Scenario: Health while running
- **WHEN** the engine is Running and locked
- **THEN** the UI receives latency, drift, fill, dropouts and the lock state at least 25 times per second

### Requirement: Dropout counting
The system SHALL count as one dropout each moment the shaker signal glitched: an underrun (the reserve ran empty), an overrun (the ring buffer was full and captured audio was discarded), or a capture discontinuity reported by Windows. The planned, faded catch-up after silence SHALL NOT be counted. Only underruns SHALL grow the automatic reserve.

#### Scenario: Underrun
- **WHEN** the output runs out of buffered audio once
- **THEN** Dropouts increases by one, is shown in red, and the reserve grows by 1 ms

#### Scenario: Overrun
- **WHEN** the output stalls long enough for the ring buffer to fill and captured audio is discarded
- **THEN** Dropouts increases by one and the reserve does not grow

#### Scenario: Capture discontinuity
- **WHEN** Windows flags a data discontinuity on the capture stream
- **THEN** Dropouts increases by one

#### Scenario: Silence is not a dropout
- **WHEN** audio resumes after a minute of silence and the engine performs its faded catch-up
- **THEN** Dropouts does not change
