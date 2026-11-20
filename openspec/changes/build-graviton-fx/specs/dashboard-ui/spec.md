# Spec Delta

## Purpose

Defines the Graviton FX window: the dashboard that shows and controls the engine live, the Settings page, and the message contract that keeps the UI separate from the engine.

## ADDED Requirements

### Requirement: Visual identity
The UI SHALL follow the approved mockup's own dark design: graphite surfaces, amber for the shaker signal, blue for captured input and waiting states, neutral white for the filter response, and semantic green, red and amber for states. It SHALL NOT imitate any existing product's look. Its fonts (Chakra Petch, Inter, JetBrains Mono, under the SIL Open Font License) SHALL be bundled with the app, so the UI makes no web requests and works offline.

#### Scenario: First launch
- **WHEN** the app opens
- **THEN** the window shows the dark dashboard with the "GRAVITON FX" header

### Requirement: Header
The header SHALL show the logo and "GRAVITON FX" wordmark (with the app version as the logo's tooltip), the engine status, a Start/Stop button, a Settings button, and minimize and close controls. It SHALL NOT contain a view switch or a latency readout; latency is shown in the Engine health panel.

#### Scenario: Start disabled on feedback
- **WHEN** the engine reports Check output
- **THEN** the Start button is disabled

### Requirement: Status presentation
Each engine state SHALL have a distinct look in the header: STOPPED grey, STARTING and SWITCHING amber and blinking, RUNNING green and pulsing, WAITING blue, ERROR and CHECK OUTPUT red. Engine health values SHALL show "--" when the engine is not Running.

#### Scenario: Error shown in place
- **WHEN** the engine enters Error because of an output problem
- **THEN** the status shows ERROR and the Shaker panel shows the error message

### Requirement: Source and Shaker panels
The Source panel SHALL contain the System mix / App switch, the device or app selector (with refresh in App mode), the waiting status line when needed, and the format line in System mix. The Shaker output panel SHALL contain the WASAPI Exclusive / ASIO switch, the device selector, waiting, feedback and error messages, buffer size with buffer time, the format line and channel remark, and the Pulse and Sweep buttons.

#### Scenario: Waiting line placement
- **WHEN** the selected shaker device is unplugged
- **THEN** the Shaker panel shows the waiting line with a blue dot aligned to its first line of text

### Requirement: Test buttons
Pulse and Sweep SHALL look and behave alike: idle labels "Pulse" and "Sweep", amber while active, the other button disabled meanwhile. Pulse is press-and-hold and reads "Pulsing" while held. Sweep is click to start and click to stop, and shows the current step frequency (for example "45 Hz") while running. Both SHALL be usable with the engine running or stopped.

#### Scenario: Holding Pulse
- **WHEN** the user holds Pulse
- **THEN** the button turns amber and reads "Pulsing", and returns to "Pulse" on release

### Requirement: Engine health panel
The health panel SHALL show latency, clock drift, rate ("48 → 44.1" or "48 native"), Dropouts (red when above 0, with a tooltip explaining what counts), and ring-buffer fill with the current percentage at the tile's top right and a bar scaled "empty · half · full". A lock chip SHALL read SETTLING, LOCKED, WAITING, ERROR or IDLE.

#### Scenario: Locked
- **WHEN** drift compensation has locked
- **THEN** the chip reads LOCKED

#### Scenario: Fill tile
- **WHEN** the ring buffer is at 52% while running
- **THEN** the fill tile shows "52%" at its top right and the bar sits just past the "half" mark

### Requirement: Gravity well
The Signal panel SHALL show a perspective grid ("gravity well") whose dip follows the shaker drive with a springy, punchy response, a fine ripple while bass plays, and a slow ambient swell, with a softly shaded amber ball that grows slightly with drive. Its top-left SHALL read "Shaker drive" with a one-word input status in grey: "Active", "No signal" or "Off". Its top-right SHALL show the drive as a percentage and the output level in dBFS ("-inf dBFS" when silent).

#### Scenario: Game paused
- **WHEN** the engine is Running and no input arrives for 2 seconds
- **THEN** the status word reads "No signal"

#### Scenario: Reduced motion
- **WHEN** Windows requests reduced motion
- **THEN** the bounce, ripple and swell stop, and the dip still follows the drive

### Requirement: Meters
The Signal panel SHALL show IN and OUT meters as solid bars (blue and amber) that stretch to the height of the gravity well, with a peak-hold marker, the peak value in dB above each bar ("-inf" when silent), and a dB scale from 0 to -48.

#### Scenario: Meter values
- **WHEN** the output peaks at -12 dBFS
- **THEN** the value above the OUT meter reads -12

### Requirement: Level history
The Signal panel SHALL show a 5-second level history on a -36 to 0 dB scale: captured input as a blue line with a soft fill, shaker output as an amber line with a soft fill and glow, and marks along the top where the limiter engaged ("Limiter active") or the output clipped ("Clipping", when the limiter is off).

#### Scenario: Limiter label follows the setting
- **WHEN** the limiter is disabled in Settings
- **THEN** the legend entry reads "Clipping" in red

### Requirement: Filter panel
The filter panel SHALL provide Cutoff and Gain knobs (drag, Shift-drag for fine steps, mouse wheel, arrow keys, double-click to reset), a slope selector labelled "Slope · dB/oct", the preset picker in its header, and a chart from 10 Hz to 2 kHz.

#### Scenario: Knob changes chart
- **WHEN** the user drags the Cutoff knob
- **THEN** the knob value, the response curve and the shaker output follow continuously

### Requirement: Filter chart
The chart SHALL show SUB / BASS / MIDS regions, the live input and output spectrum in the same style as the level history, a dotted shaker-peak line, the combined subsonic and low-pass response as a neutral white line with a cutoff handle and label, and the current sweep frequency as a marker while a sweep runs. Dragging the chart SHALL set the cutoff; dragging the handle SHALL set cutoff and gain.

#### Scenario: Dragging the handle
- **WHEN** the user drags the cutoff handle up and to the right
- **THEN** cutoff and gain increase and the knobs follow

### Requirement: Preset picker
The filter panel header SHALL show the active preset in a dropdown with a modified marker. The dropdown SHALL list presets and offer "Save current as new preset" (with an inline name field) and "Manage presets…" (which opens Settings). Revert and Save SHALL appear next to it only when the preset is modified.

#### Scenario: Save as new from the dropdown
- **WHEN** the user opens the dropdown, chooses "Save current as new preset", types a name and presses Enter
- **THEN** the preset is created and shown as active

### Requirement: Settings page
The Settings button SHALL open a Settings page with a back control to the dashboard, also closed by Esc. The page SHALL show one full-width section per row, each with its title and a short description on the left and its options on the right.

#### Scenario: Open and close Settings
- **WHEN** the user clicks the Settings button and then presses Esc
- **THEN** the Settings page opens and then returns to the dashboard

### Requirement: Settings sections
The Settings page SHALL contain, in order: Application (start with Windows, start minimized to tray, start engine on launch, close button hides to tray, check for updates automatically), Engine (subsonic filter, subsonic slope, output limiter), Presets (use, rename, duplicate, delete), and About (update status with Check now and View release, the GitHub project link with Open, reset to defaults, version and license).

#### Scenario: About section
- **WHEN** the user scrolls to About
- **THEN** it shows the update status, "github.com/deuter0n/GravitonFX" with Open, Reset to defaults, and "Graviton FX 0.1.0 · GPL-3.0-or-later"

#### Scenario: Reset to defaults
- **WHEN** the user presses Reset and then confirms
- **THEN** all settings return to their defaults, Default becomes the active preset, and user presets are kept

#### Scenario: GitHub link
- **WHEN** the user presses Open next to "github.com/deuter0n/GravitonFX"
- **THEN** the project page opens in the default browser

### Requirement: Update indicator
When an update is available, the Settings button SHALL show a small amber dot, and its tooltip SHALL name the new version. The dot SHALL disappear once the installed version is current.

#### Scenario: Update available
- **WHEN** the host reports that 0.2.0 is available
- **THEN** the Settings button shows the amber dot and About shows "Update available: 0.2.0. You have 0.1.0." with View release

### Requirement: Restricted links
The UI SHALL ask the host to open links only with `openUrl`, and the host SHALL open only `https://github.com/deuter0n/GravitonFX` and pages under it, in the default browser. Any other URL SHALL be refused.

#### Scenario: Unexpected URL
- **WHEN** the UI sends `openUrl` with a URL outside the project's GitHub pages
- **THEN** the host ignores it and opens nothing

### Requirement: Separation through messages
The UI SHALL communicate with the host only through JSON messages: `state`, `devices` and `telemetry` from the host; `start`, `stop`, `set`, `pulseStart`, `pulseStop`, `sweep`, `refreshProcesses`, `resetSettings`, `checkUpdates`, `openUrl` and the preset commands from the UI. The UI SHALL NOT read or write settings files or the registry.

#### Scenario: Setting change round trip
- **WHEN** the user changes the buffer size
- **THEN** the UI sends a `set` message and updates its display from the host's next `state` message

### Requirement: Accessibility and motion
All controls SHALL be keyboard operable with a visible focus indicator. When the operating system requests reduced motion, decorative animation SHALL stop while live data keeps updating.

#### Scenario: Keyboard only
- **WHEN** the user navigates with Tab, arrow keys, Space and Enter
- **THEN** every control can be reached and operated, including holding Pulse with Space

### Requirement: Idle cost when hidden
While the window is minimized or hidden to the tray, the UI SHALL stop rendering and the host SHALL stop sending telemetry.

#### Scenario: Hidden to tray
- **WHEN** the window is hidden to the tray for an hour while the engine is running
- **THEN** the UI uses no rendering time during that hour
