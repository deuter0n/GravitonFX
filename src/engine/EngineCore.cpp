// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 deuter0n

#include "engine/EngineCore.h"

namespace gfx {

namespace {

constexpr double kRetrySeconds = 3.0;
constexpr double kPollSeconds = 2.0;
constexpr double kStallSeconds = 0.5;
constexpr double kTestMessageSeconds = 4.0;
constexpr double kSourceSwapTimeout = 1.0;
constexpr double kFadeTimeout = 0.05;

}  // namespace

EngineCore::EngineCore(Backend& backend) : backend_(backend) {}

EngineCore::~EngineCore()
{
    closeOutput();
    closeSources();
}

double EngineCore::seconds() const
{
    return static_cast<double>(backend_.now()) / static_cast<double>(backend_.ticksPerSecond());
}

// ---------------------------------------------------------------------------
// Commands

void EngineCore::apply(const EngineSettings& settings)
{
    const EngineSettings old = settings_;
    settings_ = settings;
    render_.setParams(settings.filter, settings.limiter);

    state_.checkOutput = feedbackConflict();
    if (state_.checkOutput) {
        if (active_) {
            stop();  // never loop the shaker signal back into the capture
        }
        publish();
        return;
    }
    if (!active_) {
        publish();
        return;
    }

    if (!old.sameOutput(settings)) {
        // The output rate may change, so both sides reopen.
        setPhase(Phase::Switching);
        publish();
        closeOutput();
        closeSources();
        tryActivate();
    } else if (!old.sameSource(settings)) {
        if (outputOpen_) {
            switchSource();
        } else {
            tryActivate();
        }
    }
    publish();
}

void EngineCore::switchSource()
{
    setPhase(Phase::Switching);
    publish();
    if (openSource()) {
        missing_ = Missing::None;
        state_.messagePanel = Panel::None;
        state_.message.clear();
        setPhase(Phase::Running);
    } else if (state_.phase == Phase::Switching) {
        // The swap timed out: the output stopped calling back.
        closeOutput();
        closeSources();
        enterError(Panel::Shaker, L"The output stopped responding.");
    }
    publish();
}

void EngineCore::start()
{
    if (active_ || state_.checkOutput) {
        return;
    }
    if (testOnly_) {
        closeOutput();  // the engine takes the output over from the test
        testOnly_ = false;
    }
    active_ = true;
    setPhase(Phase::Starting);
    publish();
    tryActivate();
}

void EngineCore::stop()
{
    active_ = false;
    testOnly_ = false;
    closeOutput();
    closeSources();
    missing_ = Missing::None;
    state_.messagePanel = Panel::None;
    state_.message.clear();
    setPhase(Phase::Stopped);
    publish();
}

void EngineCore::pulseStart()
{
    if (state_.phase == Phase::Switching || state_.phase == Phase::Error) {
        return;  // tests are unavailable then
    }
    if (!outputOpen_) {
        if (!openOutput(true)) {
            return;
        }
        testOnly_ = true;
    }
    render_.pulseStart();
    state_.pulsing = true;
    state_.sweeping = false;
    publish();
}

void EngineCore::pulseStop()
{
    render_.pulseStop();
    state_.pulsing = false;
    publish();
}

void EngineCore::sweep()
{
    if (state_.sweeping) {
        render_.testStop();
        state_.sweeping = false;
        publish();
        return;
    }
    if (state_.phase == Phase::Switching || state_.phase == Phase::Error) {
        return;
    }
    if (!outputOpen_) {
        if (!openOutput(true)) {
            return;
        }
        testOnly_ = true;
    }
    render_.sweepStart();
    state_.sweeping = true;
    state_.pulsing = false;
    publish();
}

// ---------------------------------------------------------------------------
// Events

void EngineCore::onDeviceEvent(DeviceEvent event, const std::wstring&)
{
    if (event == DeviceEvent::DefaultChanged) {
        // The default may now be the shaker device, or capture must move.
        const bool conflict = feedbackConflict();
        if (conflict != state_.checkOutput) {
            state_.checkOutput = conflict;
            if (conflict && active_) {
                stop();
            }
            publish();
        }
        const bool followsDefault = settings_.sourceMode == EngineSettings::SourceMode::SystemMix &&
                                    settings_.captureFollowsDefault;
        if (active_ && followsDefault && state_.phase == Phase::Running && resolvedCaptureId() != capturedId_) {
            switchSource();
            return;
        }
    }
    // Something came back: Waiting and Error try again right away.
    if (active_ && (state_.phase == Phase::Waiting || state_.phase == Phase::Error)) {
        tryActivate();
    }
}

void EngineCore::tick()
{
    const double now = seconds();

    // Tests: release the test-only output when the test ends.
    state_.pulsing = render_.pulsing();
    state_.sweeping = render_.sweeping();
    if (testOnly_ && !render_.testActive()) {
        closeOutput();
        testOnly_ = false;
    }
    if (!state_.testMessage.empty() && now >= testMessageUntil_) {
        state_.testMessage.clear();
    }

    if (active_) {
        // Output health.
        if (outputOpen_) {
            const double sinceCallback =
                static_cast<double>(backend_.now() - output_->lastCallbackTime()) / backend_.ticksPerSecond();
            if (output_->failed()) {
                closeOutput();
                closeSources();
                if (!backend_.isConnected(settings_.shakerDeviceId)) {
                    enterWaiting(Missing::Shaker);
                } else {
                    enterError(Panel::Shaker, L"The output stopped responding.");
                }
            } else if (sinceCallback > kStallSeconds) {
                closeOutput();
                closeSources();
                enterError(Panel::Shaker, L"The output stopped responding.");
            }
        }

        // Capture health, and App mode following its process.
        if (outputOpen_ && activeSlot_ >= 0) {
            const bool appMode = settings_.sourceMode == EngineSettings::SourceMode::App;
            bool lost = captures_[activeSlot_]->failed();
            bool restarted = false;
            if (appMode && now >= nextPoll_) {
                nextPoll_ = now + kPollSeconds;
                const uint32_t pid = backend_.findProcess(settings_.appExe);
                lost = lost || pid == 0;
                restarted = pid != 0 && pid != capturedPid_;
            }
            if (restarted) {
                switchSource();  // same app, new process: capture it again
            } else if (lost) {
                if (appMode ? backend_.findProcess(settings_.appExe) == 0
                            : !backend_.isConnected(resolvedCaptureId())) {
                    // Keep the output running (silence) while waiting.
                    render_.setSource(nullptr);
                    for (int i = 0; i < 100 && !render_.sourceSwapped(); ++i) {
                        backend_.waitBriefly();
                    }
                    if (!render_.sourceSwapped()) {
                        closeOutput();  // output not running: safe to detach directly
                    }
                    closeSources();
                    enterWaiting(appMode ? Missing::App : Missing::CaptureDevice);
                } else {
                    closeOutput();
                    closeSources();
                    enterError(Panel::Source, L"Capture stopped.");
                }
            }
        }

        // Waiting: look again every 2 s. Error: retry every 3 s.
        if (state_.phase == Phase::Waiting && now >= nextPoll_) {
            nextPoll_ = now + kPollSeconds;
            tryActivate();
        } else if (state_.phase == Phase::Error && now >= retryAt_) {
            tryActivate();
        }
    }
    publish();
}

Telemetry EngineCore::telemetry() const
{
    const double bufferMs = outputOpen_ ? 1000.0 * state_.outputFormat.bufferFrames / state_.outputFormat.sampleRate : 0.0;
    const double packetMs = state_.captureOpen && state_.captureFormat.sampleRate > 0.0
                                ? 1000.0 * static_cast<double>(state_.captureFormat.packetFrames) / state_.captureFormat.sampleRate
                                : 0.0;
    Telemetry t = render_.telemetry(bufferMs, packetMs);
    t.test = testOnly_;
    return t;
}

// ---------------------------------------------------------------------------
// Opening and closing

void EngineCore::tryActivate()
{
    if (!active_) {
        return;
    }
    if (!outputOpen_) {
        // Input stages are prepared for the output rate, so sources reopen
        // whenever the output does.
        closeSources();
        if (!openOutput(false)) {
            return;
        }
    }
    if (activeSlot_ < 0 && !openSource()) {
        return;
    }
    missing_ = Missing::None;
    state_.messagePanel = Panel::None;
    state_.message.clear();
    setPhase(Phase::Running);
    publish();
}

bool EngineCore::openOutput(bool forTest)
{
    const std::wstring& name = settings_.shakerDeviceName.empty() ? std::wstring(L"The shaker device")
                                                                    : settings_.shakerDeviceName;
    if (!backend_.isConnected(settings_.shakerDeviceId)) {
        if (forTest) {
            showTestMessage(name + L" isn't connected.");
        } else {
            enterWaiting(Missing::Shaker);
        }
        return false;
    }

    output_ = backend_.makeOutput(settings_.driver);
    const OpenResult r = output_->open(settings_.shakerDeviceId, settings_.bufferFrames, &render_);
    if (!r.ok()) {
        output_.reset();
        if (forTest) {
            showTestMessage(r.status == OpenStatus::Missing ? name + L" isn't connected." : r.message);
        } else if (r.status == OpenStatus::Missing) {
            enterWaiting(Missing::Shaker);
        } else {
            enterError(Panel::Shaker, r.message);
        }
        return false;
    }

    const OutputFormat& f = output_->format();
    render_.prepare(f.sampleRate, f.bufferFrames, settings_);
    render_.clearSource();
    if (!output_->start()) {
        output_->close();
        output_.reset();
        if (forTest) {
            showTestMessage(L"Can't open " + name + L". Reconnect it or pick another device.");
        } else {
            enterError(Panel::Shaker, L"Can't open " + name + L". Reconnect it or pick another device.");
        }
        return false;
    }
    outputOpen_ = true;
    state_.outputOpen = true;
    state_.outputFormat = f;
    return true;
}

void EngineCore::closeOutput()
{
    if (!outputOpen_) {
        return;
    }
    render_.fadeOut();  // 10 ms, so closing never clicks
    const double until = seconds() + kFadeTimeout;
    while (!render_.fadedOut() && seconds() < until) {
        backend_.waitBriefly();
    }
    output_->close();
    output_.reset();
    outputOpen_ = false;
    state_.outputOpen = false;
    state_.outputFormat = {};
    render_.clearSource();  // the output thread has stopped
}

bool EngineCore::openSource()
{
    SourceSpec spec;
    std::wstring key;
    uint32_t pid = 0;
    if (settings_.sourceMode == EngineSettings::SourceMode::SystemMix) {
        key = resolvedCaptureId();
        if (key.empty() || !backend_.isConnected(key)) {
            enterWaiting(Missing::CaptureDevice);
            return false;
        }
        spec.kind = SourceSpec::Kind::Loopback;
        spec.deviceId = key;
    } else {
        pid = backend_.findProcess(settings_.appExe);
        if (pid == 0) {
            enterWaiting(Missing::App);
            return false;
        }
        spec.kind = SourceSpec::Kind::Process;
        spec.processId = pid;
        key = settings_.appExe;
    }

    // Open into the slot the render path isn't using.
    const int slotIndex = activeSlot_ == 0 ? 1 : 0;
    SourceSlot& slot = slots_[slotIndex];
    auto capture = backend_.makeCapture();
    slot.ring.clear();  // nobody reads this slot right now
    const OpenResult r = capture->open(spec, &slot.ring);
    if (!r.ok()) {
        if (r.status == OpenStatus::Missing) {
            enterWaiting(spec.kind == SourceSpec::Kind::Process ? Missing::App : Missing::CaptureDevice);
        } else {
            closeOutput();
            closeSources();
            if (r.status == OpenStatus::Unsupported) {
                enterError(Panel::Source, r.message, false);  // retrying can't help
            } else if (spec.kind == SourceSpec::Kind::Process) {
                enterError(Panel::Source, L"Can't capture " + settings_.appExe + L". Restart it or pick another app.");
            } else {
                enterError(Panel::Source, r.message);
            }
        }
        return false;
    }
    const CaptureFormat& f = capture->format();
    slot.stage.prepare(f.sampleRate, state_.outputFormat.sampleRate, f.packetFrames,
                       static_cast<size_t>(state_.outputFormat.bufferFrames));
    slot.capture = capture.get();
    capture->start();

    // Hand it to the render path; the old source fades out (<= 2.5 ms).
    render_.setSource(&slot);
    const double until = seconds() + kSourceSwapTimeout;
    while (!render_.sourceSwapped() && seconds() < until) {
        backend_.waitBriefly();
    }
    if (!render_.sourceSwapped()) {
        // The output isn't calling back; stop it so nothing reads the old slot.
        closeOutput();
    }

    // Now nothing reads the old slot: close it.
    const int oldIndex = activeSlot_;
    captures_[slotIndex] = std::move(capture);
    activeSlot_ = slotIndex;
    if (oldIndex >= 0) {
        captures_[oldIndex]->close();
        captures_[oldIndex].reset();
        slots_[oldIndex].capture = nullptr;
    }
    capturedId_ = key;
    capturedPid_ = pid;
    nextPoll_ = seconds() + kPollSeconds;
    state_.captureOpen = true;
    state_.captureFormat = f;
    return outputOpen_;
}

void EngineCore::closeSources()
{
    // Callers make sure the render path no longer reads the slots (output
    // closed, or swapped to silence).
    if (!outputOpen_) {
        render_.clearSource();
    }
    for (int i = 0; i < 2; ++i) {
        if (captures_[i]) {
            captures_[i]->close();
            captures_[i].reset();
        }
        slots_[i].capture = nullptr;
    }
    activeSlot_ = -1;
    capturedId_.clear();
    capturedPid_ = 0;
    state_.captureOpen = false;
    state_.captureFormat = {};
}

// ---------------------------------------------------------------------------
// State

void EngineCore::enterWaiting(Missing what)
{
    missing_ = what;
    setPhase(Phase::Waiting);
    state_.messageIsError = false;
    switch (what) {
    case Missing::App:
        state_.messagePanel = Panel::Source;
        state_.message = settings_.appExe + L" isn't running. Capture starts when it opens.";
        break;
    case Missing::CaptureDevice:
        state_.messagePanel = Panel::Source;
        state_.message = settings_.captureDeviceName + L" isn't connected. Capture resumes when it's back.";
        break;
    case Missing::Shaker:
        state_.messagePanel = Panel::Shaker;
        state_.message = settings_.shakerDeviceName + L" isn't connected. Output starts when it's back.";
        break;
    case Missing::None:
        break;
    }
    nextPoll_ = seconds() + kPollSeconds;
    publish();
}

void EngineCore::enterError(Panel panel, const std::wstring& message, bool retry)
{
    setPhase(Phase::Error);
    state_.messagePanel = panel;
    state_.messageIsError = true;
    state_.message = message;
    state_.retrying = retry;
    retryAt_ = retry ? seconds() + kRetrySeconds : 1e300;  // never, until something changes
    publish();
}

void EngineCore::setPhase(Phase phase)
{
    state_.phase = phase;
    if (phase != Phase::Error) {
        state_.retrying = false;
    }
}

void EngineCore::showTestMessage(const std::wstring& message)
{
    state_.testMessage = message;
    testMessageUntil_ = seconds() + kTestMessageSeconds;
    publish();
}

void EngineCore::publish()
{
    if (listener_ && !(state_ == lastPublished_)) {
        lastPublished_ = state_;
        listener_(state_);
    }
}

std::wstring EngineCore::resolvedCaptureId() const
{
    return settings_.captureFollowsDefault ? backend_.defaultRenderId() : settings_.captureDeviceId;
}

bool EngineCore::feedbackConflict() const
{
    return settings_.sourceMode == EngineSettings::SourceMode::SystemMix &&
           settings_.driver == OutputDriver::WasapiExclusive && !settings_.shakerDeviceId.empty() &&
           resolvedCaptureId() == settings_.shakerDeviceId;
}

}  // namespace gfx
