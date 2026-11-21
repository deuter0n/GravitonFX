# Spec Delta

## Purpose

Defines the engine's states and transitions: starting and stopping, changing devices while running, waiting for missing sources and devices, failure detection and automatic recovery, feedback protection, and the engine's independence from the UI.

## ADDED Requirements

### Requirement: Engine states
The engine SHALL always be in exactly one of these states, and the header status SHALL show it: Stopped, Starting, Running, Waiting, Switching, Error, or Check output.

#### Scenario: Normal start
- **WHEN** the user presses Start with a valid source and output
- **THEN** the status shows STARTING while devices open, then RUNNING

#### Scenario: Stop
- **WHEN** the user presses Stop while the engine is in any active state
- **THEN** output fades out, devices are released, and the status shows STOPPED

### Requirement: Missing means Waiting
The engine SHALL be Waiting, not Error, when it is active but something selected is missing and expected to come back: the selected app is not running, the selected capture device is not connected, or the selected shaker device is not connected. It SHALL leave Waiting automatically when the missing item returns.

#### Scenario: Shaker amp off at boot
- **WHEN** the app starts the engine at sign-in while the shaker's USB DAC is switched off
- **THEN** the status shows WAITING and output starts automatically when the DAC is switched on

#### Scenario: App closed
- **WHEN** the selected app exits while the engine is Running
- **THEN** the status changes to WAITING, and back to RUNNING when the app starts again

### Requirement: Running includes silence
The engine SHALL be Running whenever the source is available and the output is streaming, even if the source is silent.

#### Scenario: Silent app
- **WHEN** the selected app is running but not playing anything
- **THEN** the status is RUNNING

### Requirement: Failing means Error
The engine SHALL enter Error when it cannot stream for a reason that may need the user to act: device in use by another application, unsupported format, buffer size refused, ASIO driver failure, output stall, or per-app capture unsupported. A message under the affected selector SHALL name the cause and suggest a fix, and the selector SHALL be outlined in red; for a refused buffer size the buffer slider's track turns red instead.

#### Scenario: Shaker device in use
- **WHEN** another application already holds the shaker device in exclusive mode and the user starts the engine
- **THEN** the status shows ERROR and the message under the shaker device selector reads "In use by another app. Close it or pick another device."

### Requirement: Automatic retry
While in Error, the engine SHALL retry every 3 seconds and immediately when Windows reports that a relevant device became available, and SHALL recover without user action once the cause is gone.

#### Scenario: Recovery without user action
- **WHEN** the engine is in Error because the device was in use, and the other application releases it
- **THEN** the engine recovers to RUNNING within 3 seconds without user action

### Requirement: Output stall watchdog
The engine SHALL detect when the output device stops requesting audio for more than 500 ms while Running, report it as an error, and restart the output.

#### Scenario: Driver hangs
- **WHEN** the shaker driver stops calling back for one second
- **THEN** the engine shows ERROR with "Stopped responding." under the shaker device selector, and restarts the output

### Requirement: Live changes without restart
Every setting SHALL be changeable while the engine is active. Filter, gain, subsonic, limiter and preset changes SHALL apply without interrupting output. Source changes SHALL interrupt output for no more than 100 ms. Shaker output changes (device, driver, buffer size) SHALL interrupt output for no more than 1 second. During source and output changes the status SHALL show SWITCHING.

#### Scenario: Changing the source while running
- **WHEN** the user changes the capture device while Running
- **THEN** the status shows SWITCHING, output fades out briefly and back in, and the status returns to RUNNING

#### Scenario: Changing the shaker device while running
- **WHEN** the user selects a different shaker device while Running
- **THEN** the status shows SWITCHING, the old device is released, the new one opens, and drift compensation settles again

### Requirement: Feedback protection
The engine SHALL refuse to run when System mix captures the same physical device that is used for shaker output. The status SHALL show CHECK OUTPUT, a feedback warning SHALL show under the shaker device selector, and Start SHALL be disabled until the conflict is resolved. If the conflict arises while running, the engine SHALL stop. The device lists SHALL still allow selecting such a combination.

#### Scenario: Conflict selected while stopped
- **WHEN** the capture device and the shaker device are the same device
- **THEN** the status shows CHECK OUTPUT, a feedback warning is shown, and Start is disabled

#### Scenario: App mode cannot conflict
- **WHEN** the source is App and the shaker device is also the Windows default output
- **THEN** no feedback warning is shown and the engine runs

### Requirement: Test signals do not change the engine state
Playing a test signal while the engine is stopped SHALL NOT change the header status from STOPPED. Test signals SHALL be unavailable while the engine is Switching or in Error.

#### Scenario: Pulse while stopped
- **WHEN** the engine is stopped and the user holds Pulse
- **THEN** the shaker vibrates and the status stays STOPPED

### Requirement: Real-time safety
The audio processing path SHALL NOT allocate memory, take locks, perform file, logging or UI operations, or wait on other threads. All device opening, closing and configuration SHALL happen outside the audio threads.

#### Scenario: UI busy
- **WHEN** the UI thread is blocked for 2 seconds
- **THEN** the shaker output continues without underruns

### Requirement: Engine independent of the UI
The engine SHALL keep running while the window is minimized, hidden to the tray, or while the UI is reloading. Closing or hiding the UI SHALL NOT stop the engine unless the user quits the application.

#### Scenario: Window hidden to tray
- **WHEN** the engine is Running and the window is hidden to the tray
- **THEN** shaker output continues uninterrupted
