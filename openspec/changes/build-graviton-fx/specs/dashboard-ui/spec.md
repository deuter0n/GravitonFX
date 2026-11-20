# Spec Delta

## Purpose

Defines the Graviton FX window: the dashboard that shows and controls the engine live, the Settings page, and the message contract that keeps the UI separate from the engine.

## ADDED Requirements

### Requirement: Visual identity
The UI SHALL follow the approved mockup's own dark design: graphite surfaces, amber for the shaker signal, blue for captured input and waiting states, neutral white for the filter response, and semantic green, red and amber for states. It SHALL NOT imitate any existing product's look. Its fonts (Chakra Petch, Inter, JetBrains Mono, under the SIL Open Font License) SHALL be bundled with the app, so the UI makes no web requests and works offline.

#### Scenario: First launch
- **WHEN** the app opens
- **THEN** the window shows the dark dashboard with the "GRAVITON FX" header

### Requirement: Dashboard layout
The dashboard SHALL fill the window with Source and Shaker output in a left column, and the Signal panel (gravity well with the level history beside it) above the Low-pass filter on the right. The Engine health panel SHALL run as a strip across the full width at the bottom. Both columns SHALL end at the same height: Source is as tall as its content, and Shaker output takes the rest of the left column with the test buttons at its bottom.

#### Scenario: Columns end together
- **WHEN** the dashboard is shown at any window size
- **THEN** the bottoms of the Shaker output and Low-pass filter panels line up, directly above the Engine health strip

### Requirement: Consistent spacing
The space between any two cards, between the columns, above the Engine health strip and between the cards and the window edge SHALL be the same everywhere (12 CSS pixels), on the dashboard and the Settings page.

#### Scenario: Equal gaps
- **WHEN** the dashboard is shown at any window size
- **THEN** every gap between neighbouring cards and between the cards and the window edge measures the same

### Requirement: Fits the window without scrolling
The dashboard SHALL fit the window's height without a scrollbar on 16:9 screens, for windows from 720 CSS pixels tall. The gravity well and the level history SHALL take the height left over. The window SHALL NOT be resizable below 1180 × 720 CSS pixels and SHALL open at a size that fits the screen's work area.

#### Scenario: Laptop at 1920 × 1080, 125% scaling
- **WHEN** the window is maximized on a 1920 × 1080 screen at 125% scaling (about 1536 × 816 CSS pixels after the taskbar)
- **THEN** every panel is visible at once and there is no vertical scrollbar

#### Scenario: Taller window
- **WHEN** the window is made taller
- **THEN** the gravity well and the level history grow, and the other panels keep their size

### Requirement: Header
The header SHALL show the logo and "GRAVITON FX" wordmark (with the app version as the logo's tooltip), the engine status, a Start/Stop button, a Settings button, and minimize and close controls. It SHALL NOT contain a view switch or a latency readout; latency is shown in the Engine health panel.

#### Scenario: Start disabled on feedback
- **WHEN** the engine reports Check output
- **THEN** the Start button is disabled

### Requirement: Status presentation
Each engine state SHALL have a distinct look in the header: STOPPED grey, STARTING and SWITCHING amber and blinking, RUNNING green and pulsing, WAITING blue, ERROR and CHECK OUTPUT red. Engine health values SHALL show "--" when the engine is not Running.

#### Scenario: Error shown in place
- **WHEN** the engine enters Error because of an output problem
- **THEN** the status shows ERROR and the header message line shows the error in red

### Requirement: Source and Shaker panels
The Source panel SHALL contain the System mix / App switch, the device or app selector (with refresh in App mode), and the format tags in System mix. The Shaker output panel SHALL contain the WASAPI Exclusive / ASIO switch, the device selector, a full-width buffer selector whose entries include the buffer time ("64 samples · 1.45 ms"), the format tags (with "out 1–2" on devices with more than 2 outputs), and the Pulse and Sweep buttons on separate rows.

#### Scenario: Multi-output device
- **WHEN** the shaker device changes from a stereo DAC to an 8-output interface
- **THEN** the tags change to show "8 ch" and "out 1–2" in the same row, and no panel changes height

### Requirement: Header message line
Waiting, feedback, error and test messages SHALL appear as one line in the header between the logo and the status, with a blue dot for waiting and a red dot for errors and the feedback warning, and the full text on hover. The selector the message is about SHALL get a matching blue or red outline. Errors SHALL take precedence over the feedback warning, which takes precedence over waiting messages. Messages SHALL never change the height of any panel.

#### Scenario: Shaker unplugged
- **WHEN** the selected shaker device is unplugged
- **THEN** the header shows "<name> isn't connected. Output starts when it's back." with a blue dot, and the shaker device selector has a blue outline

#### Scenario: No message
- **WHEN** nothing is missing or wrong
- **THEN** the header shows no message line and no selector is outlined

### Requirement: Test buttons
Pulse and Sweep SHALL look and behave alike: idle labels "Pulse Test · 40 Hz" and "Sweep Test · 20 – 200 Hz", amber while active, the other button disabled meanwhile. Pulse is press-and-hold and reads "Pulsing · 40 Hz" while held. Sweep is click to start and click to stop, and reads "Sweeping · <current step> Hz" (for example "Sweeping · 45 Hz") while running. Both SHALL be usable with the engine running or stopped.

#### Scenario: Holding Pulse
- **WHEN** the user holds Pulse
- **THEN** the button turns amber and reads "Pulsing · 40 Hz", and returns to "Pulse Test · 40 Hz" on release

### Requirement: Header stays one row
The header SHALL always be a single row. A message too long for the space SHALL be cut with "…", with the full text on hover, so a message never changes the height of the header.

#### Scenario: Long message on a narrow window
- **WHEN** the feedback warning shows on a 1366 px wide window
- **THEN** the header stays one row and every panel keeps its height

### Requirement: Message texts
Messages SHALL be short and plain: what happened, then what to do, in one or two short sentences. The texts SHALL be exactly those in the scenarios below, with the device or app name filled in. Errors that retry by themselves SHALL be followed by a faint "Retrying…"; errors that can't be fixed by retrying SHALL NOT.

#### Scenario: Waiting messages (blue)
- **WHEN** the engine waits for a missing capture device, app or shaker device
- **THEN** the header shows, in that case, "<device> isn't connected. Capture resumes when it's back.", "<app.exe> isn't running. Capture starts when it opens." or "<device> isn't connected. Output starts when it's back."

#### Scenario: Source errors (red, retrying)
- **WHEN** capture fails for a reason other than a missing source
- **THEN** the header shows one of "Can't capture <device>. Pick another device.", "Can't capture <app.exe>. Restart it or pick another app.", "<device> uses an unsupported format. Pick another device." or "Capture stopped.", followed by "Retrying…", and the source selector is outlined in red

#### Scenario: Per-app capture unsupported (red, no retry)
- **WHEN** App is selected on Windows older than 10 version 2004
- **THEN** the header shows "Capturing one app needs Windows 10 2004 or later. Use System mix." without "Retrying…"

#### Scenario: Shaker output errors (red, retrying)
- **WHEN** the shaker output can't be opened or stops working
- **THEN** the header shows one of "Another app is using <device>. Close it or pick another device.", "Exclusive mode is off for <device>. Turn it on in Windows sound settings.", "<device> has no supported format. Pick another device.", "<device> can't use this buffer size. Pick a larger one.", "Can't open <device>. Reconnect it or pick another device." or "The output stopped responding.", followed by "Retrying…", and the shaker selector is outlined in red

#### Scenario: Feedback warning (red)
- **WHEN** System mix captures the shaker's own device
- **THEN** the header shows "The shaker device is also the capture device. Pick another one, or capture one app."

#### Scenario: Test signal can't start (red, about 4 seconds)
- **WHEN** a test signal can't open the shaker output while the engine is stopped
- **THEN** the header shows "<device> isn't connected." or the matching shaker output error above, without "Retrying…"

### Requirement: Engine health panel
The health strip SHALL show, in one row, latency, clock drift, rate ("48 → 44.1" or "48 native"), Dropouts (red when above 0, with a tooltip explaining what counts), and ring-buffer fill with the current percentage at the tile's top right and a bar scaled "empty · half · full". A lock chip SHALL read SETTLING, LOCKED, WAITING, ERROR or IDLE.

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

### Requirement: Live levels
The top-right of the level history SHALL show the current input and output levels in dBFS as "IN <n> · OUT <n> dBFS" (blue and amber, "-inf" when silent). There SHALL be no separate IN/OUT meter bars; the level history's right edge is the live level.

#### Scenario: Output level readout
- **WHEN** the output level is -12 dBFS
- **THEN** the readout shows "OUT -12"

### Requirement: Level history
The Signal panel SHALL show, beside the gravity well and at the same height, a 5-second level history on a -36 to 0 dB scale: captured input as a blue line with a soft fill, shaker output as an amber line with a soft fill and glow, and marks along the top where the limiter engaged ("Limiter active") or the output clipped ("Clipping", when the limiter is off).

#### Scenario: Limiter label follows the setting
- **WHEN** the limiter is disabled in Settings
- **THEN** the legend entry reads "Clipping" in red

### Requirement: Legends inside the graphs
The level history and the filter chart SHALL draw their legends inside their own boxes, centered in a band below the plot (the level history keeps "Last 5 s" and the live levels in a band above it), so each graph uses the full height of its area. The level history box SHALL be exactly as tall as the gravity well box.

#### Scenario: History matches the well
- **WHEN** the window is resized
- **THEN** the level history box stays the same height as the gravity well box, and no legend sits below either graph

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
The UI SHALL communicate with the host only through JSON messages: `state`, `devices` and `telemetry` from the host; `start`, `stop`, `set`, `pulseStart`, `pulseStop`, `sweep`, `refreshProcesses`, `resetSettings`, `checkUpdates`, `openUrl`, `windowMinimize`, `windowClose` and the preset commands from the UI. The UI SHALL NOT read or write settings files or the registry.

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
