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
The dashboard SHALL fill the window with Source and Shaker output in a left column, and the Low-pass filter above the Signal panel (gravity well with the level history beside it) on the right. The Engine health panel SHALL run as a strip across the full width at the bottom. Both columns SHALL end at the same height: Source is as tall as its content, and Shaker output takes the rest of the left column with the test buttons at its bottom.

#### Scenario: Columns end together
- **WHEN** the dashboard is shown at any window size
- **THEN** the bottoms of the Shaker output and Low-pass filter panels line up, directly above the Engine health strip

### Requirement: Left column on taller windows
On windows taller than 720 CSS pixels, the left column SHALL use the extra height by making its selectors, switches and test buttons taller and the gaps between its groups wider, each up to a fixed limit, keeping each card's fields together. Height beyond those limits SHALL go above the Pulse and Sweep buttons, which sit at the bottom of the Shaker output card, while Driver, Device and Buffer stay together at the top. The format tags SHALL be centered in their card.

#### Scenario: 16:10 screen
- **WHEN** the window is 883 CSS pixels tall
- **THEN** the left column's controls are taller than at 720 pixels, Driver, Device and Buffer stay together, and the test buttons sit at the bottom of the Shaker output card

### Requirement: Knobs on taller windows
The Cutoff and Gain knobs SHALL be 92 CSS pixels at a 720 pixel tall window and SHALL grow with the window's extra height up to 120 pixels; the level graphs give up that height.

#### Scenario: 16:10 screen
- **WHEN** the window is 883 CSS pixels tall
- **THEN** the knobs are 120 pixels and the dashboard still fits without a scrollbar

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
- **THEN** the status shows ERROR and the message under the shaker device selector shows the error in red

### Requirement: Source and Shaker panels
The Source panel SHALL contain the System mix / App switch and the device or app selector (with refresh in App mode). The Shaker output panel SHALL contain the WASAPI Exclusive / ASIO switch, the device selector, a full-width buffer slider with one labeled stop per size labeled with its sample count, and the delay it adds on its label row ("+1.45 ms"), and the Pulse and Sweep buttons on separate rows.

#### Scenario: Multi-output device
- **WHEN** the shaker device changes from a stereo DAC to an 8-output interface
- **THEN** the tags change to show "8 ch" and "out 1–2" in the same row, and no panel changes height

### Requirement: Format tags placement
Format tags SHALL sit right under the device selector they describe, as part of the same field: in Source only in System mix, and in Shaker output before the buffer selector, with "out 1–2" added on devices with more than 2 outputs.

#### Scenario: Shaker output order
- **WHEN** the Shaker output panel is shown
- **THEN** it reads, top to bottom: driver switch, device selector with its format tags, buffer slider, test buttons

### Requirement: Messages under the selector
Waiting, feedback, error and test messages SHALL appear under the control they are about (a device or app selector, or the buffer slider), in place of its format tags or stop labels, with a blue dot for waiting and a red dot for errors and the feedback warning, and the control SHALL get a matching outline (the slider a red track). Every message SHALL fit in two lines, uncut. Errors SHALL take precedence over the feedback warning, which takes precedence over waiting messages.

#### Scenario: Shaker unplugged
- **WHEN** the selected shaker device is unplugged
- **THEN** under the shaker device selector, with a blue dot, it reads "Not connected. Output starts when it's back.", and the selector has a blue outline

#### Scenario: No message
- **WHEN** nothing is missing or wrong
- **THEN** every selector shows its format tags, the buffer slider its stop labels, and none is outlined

### Requirement: Test buttons
Pulse and Sweep SHALL look and behave alike: idle labels "Pulse Test · 40 Hz" and "Sweep Test · 20 – 200 Hz", amber while active, the other button disabled meanwhile. Pulse is press-and-hold and reads "Pulsing · 40 Hz" while held. Sweep is click to start and click to stop, and reads "Sweeping · <current step> Hz" (for example "Sweeping · 45 Hz") while running. Both SHALL be usable with the engine running or stopped.

#### Scenario: Holding Pulse
- **WHEN** the user holds Pulse
- **THEN** the button turns amber and reads "Pulsing · 40 Hz", and returns to "Pulse Test · 40 Hz" on release

### Requirement: Header stays one row
The header SHALL always be a single row (logo, status, Start/Stop, Settings and the window controls), so nothing in it changes the height of the panels below. No header item SHALL wrap, shrink or be cut, the logo and wordmark included; on a narrow page Start/Stop SHALL show only its icon.

#### Scenario: Minimum width
- **WHEN** the window is at its minimum width of 1180 CSS pixels
- **THEN** the header is one row

#### Scenario: Narrow page
- **WHEN** the page is narrower than the minimum window, as in a preview or on a phone, and the status reads CHECK OUTPUT
- **THEN** the full logo and wordmark show, the status stays on one line, Start/Stop shows only its icon, and the window controls stay inside

### Requirement: Message texts
Messages SHALL be short and plain: what happened, then what to do. They SHALL NOT repeat the device or app name, which the selector above them already shows. The texts SHALL be exactly those in the scenarios below.

#### Scenario: Waiting messages (blue)
- **WHEN** the engine waits for a missing capture device, app or shaker device
- **THEN** the message reads, in that case, "Not connected. Capture resumes when it's back.", "Not running. Capture starts when it opens.", "Pick an app to capture." (App mode before any app is chosen) or "Not connected. Output starts when it's back."

#### Scenario: Source errors (red)
- **WHEN** capture fails for a reason other than a missing source
- **THEN** the message under the source selector reads one of "Can't capture this device. Pick another one.", "Can't capture this app. Restart it or pick another.", "Unsupported format. Pick another device.", "Capture stopped." or, on Windows older than 10 version 2004, "Needs Windows 10 2004 or later. Use System mix."

#### Scenario: Shaker output errors (red)
- **WHEN** the shaker output can't be opened or stops working
- **THEN** the message under the shaker device selector reads one of "In use by another app. Close it or pick another device.", "Exclusive mode is off. Turn it on in Windows sound settings.", "No supported format. Pick another device.", "Can't open. Reconnect it or pick another device." or "Stopped responding."

#### Scenario: Buffer size refused (red)
- **WHEN** the device or ASIO driver refuses the chosen buffer size
- **THEN** the message under the buffer slider reads "Buffer size not supported. Pick a larger one." and the slider track turns red

#### Scenario: Feedback warning (red)
- **WHEN** System mix captures the shaker's own device
- **THEN** the message under the shaker device selector reads "Also the capture device. Pick another, or capture one app."

#### Scenario: Test signal can't start (red, about 4 seconds)
- **WHEN** a test signal can't open the shaker output while the engine is stopped
- **THEN** the message under the shaker device selector reads "Not connected." or the matching shaker output error above; a refused buffer size shows under the buffer slider instead

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

#### Scenario: Modified preset keeps the header in one row
- **WHEN** the cutoff, slope or gain is changed so the active preset becomes modified
- **THEN** two square icon buttons appear to the left of the dropdown in the same row and the same dark style: Revert (a counter-clockwise arrow) and Save (a disk in amber, with a "+" when it saves as new from Default), each named in its tooltip, and the filter panel keeps its height

### Requirement: Settings page
The Settings button SHALL open a Settings page with a back control to the dashboard, also closed by Esc. The page SHALL show one full-width section per row, each with its title and a short description on the left and its options on the right.

#### Scenario: Open and close Settings
- **WHEN** the user clicks the Settings button and then presses Esc
- **THEN** the Settings page opens and then returns to the dashboard

### Requirement: Settings sections
The Settings page SHALL contain, in order: Application (start with Windows, start in the tray with Windows, start engine on launch, close button hides to tray, check for updates automatically), Engine (subsonic filter, subsonic slope, output limiter), Presets (use, rename, duplicate, delete), and About (version and license under its heading, updates, the GitHub project link, reset to defaults).

#### Scenario: About section
- **WHEN** the user scrolls to About
- **THEN** "Graviton FX 0.1.0 · GPL-3.0-or-later" shows under the About heading, followed by the update status, the GitHub project link and Reset to defaults

#### Scenario: Reset to defaults
- **WHEN** the user presses Reset and then confirms
- **THEN** all settings return to their defaults, Default becomes the active preset, and user presets are kept

#### Scenario: GitHub link
- **WHEN** the user clicks the "github.com/deuter0n/GravitonFX" link, which ends with an external-link icon
- **THEN** the project page opens in the default browser

#### Scenario: Preset actions
- **WHEN** the user looks at a preset in the Presets section
- **THEN** its actions are icon buttons (use, rename, duplicate, delete) whose names show as tooltips, and delete asks for a second click

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

#### Scenario: Animation effects turned off while open
- **WHEN** the user turns off Windows "Animation effects" while the window is open
- **THEN** decorative animation stops without restarting the app

### Requirement: Rendering cost while open
The UI SHALL redraw its animated views at most about 60 times a second, in step with the display, whatever its refresh rate; the level history SHALL scroll smoothly between samples. When the engine is stopped, no test signal plays and every view has settled, the well's ambient swell SHALL fade to still, the UI SHALL stop redrawing, and the host SHALL stop sending telemetry after one last update.

#### Scenario: Open and stopped
- **WHEN** the window is open and the engine has been stopped for 10 seconds
- **THEN** the well is still and the UI uses almost no CPU

#### Scenario: Fast display
- **WHEN** the engine runs on a 120 Hz display
- **THEN** the views redraw about 60 times a second, not 120, and the level history glides instead of stepping

### Requirement: Idle cost when hidden
While the window is minimized or hidden to the tray, the UI SHALL stop rendering and the host SHALL stop sending telemetry.

#### Scenario: Hidden to tray
- **WHEN** the window is hidden to the tray for an hour while the engine is running
- **THEN** the UI uses no rendering time during that hour
