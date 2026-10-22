#include "shared_bridge.h"
#include <imgui.h>
#include <module.h>
#include <core.h>
#include <config.h>
#include <gui/gui.h>
#include <gui/tuner.h>
#include <signal_path/signal_path.h>
#include <signal_path/sink.h>
#include <dsp/sink/handler_sink.h>
#include <dsp/sink/null_sink.h>
#include <utils/flog.h>
#include <chrono>
#include <mutex>
#include <utility>

SDRPP_MOD_INFO{
    "asrtu_sink", "ASRTU shared-memory I/Q and Doppler bridge",
    "ASRTU Satellite Team", 0, 2, 0, 1
};

namespace {
ConfigManager settings;
class AsrtuSink final : public SinkManager::Sink {
public:
    AsrtuSink(SinkManager::Stream* stream, std::string name)
        : stream_(stream), name_(std::move(name))
    {
        settings.acquire();
        if (settings.conf.contains(name_) && settings.conf[name_].is_object())
            autoDoppler_ = settings.conf[name_].value("autoDoppler", true);
        settings.release();
        stream_->setSampleRate(48000.0f);
        // Tap before the volume/mute stage: these controls must not alter I/Q.
        input_ = stream_->bindStream();
        samples_.init(input_, writeSamples, this);
        discardAudio_.init(stream_->sinkOut);
        frameHandler_.handler = onFrame;
        frameHandler_.ctx = this;
        gui::waterfall.onFFTRedraw.bindHandler(&frameHandler_);
    }

    ~AsrtuSink() override {
        gui::waterfall.onFFTRedraw.unbindHandler(&frameHandler_);
        stop();
        stream_->unbindStream(input_);
    }

    void start() override {
        std::lock_guard<std::mutex> lifecycle(lifecycle_);
        if (draining_) return;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!bridge_.start()) flog::warn("ASRTU bridge: {}", bridge_.error());
        }
        // Always drain our tap, even when ownership was refused. Otherwise a
        // blocked second plugin would stall SDR++'s other signal consumers.
        try {
            discardAudio_.start();
            samples_.start();
            draining_ = true;
        } catch (...) {
            samples_.stop();
            discardAudio_.stop();
            std::lock_guard<std::mutex> lock(mutex_);
            bridge_.stop();
            throw;
        }
    }

    void stop() override {
        std::lock_guard<std::mutex> lifecycle(lifecycle_);
        // Never hold mutex_ while joining a sample callback that takes it.
        if (draining_) {
            samples_.stop();
            discardAudio_.stop();
        }
        draining_ = false;
        std::lock_guard<std::mutex> lock(mutex_);
        bridge_.stop();
        doppler_.close();
        tracking_ = false;
        trackedVfo_.clear();
        trackedCenterHz_ = 0.0;
    }

    void menuHandler() override {
        std::lock_guard<std::mutex> lock(mutex_);
        ImGui::TextUnformatted("ASRTU shared-memory bridge (48 kHz I/Q)");
        ImGui::TextUnformatted("Use RAW demodulation at 48 kHz.");
        if (ImGui::Checkbox("Automatic Doppler tuning##asrtu", &autoDoppler_)) {
            settings.acquire();
            settings.conf[name_]["autoDoppler"] = autoDoppler_;
            settings.release(true);
        }
        if (bridge_.active()) {
            ImGui::TextUnformatted("Bridge active");
            ImGui::TextUnformatted(autoDoppler_ && tracking_ ? "Doppler: tracking" :
                                   autoDoppler_ ? "Doppler: waiting for valid tracker data / Radio VFO" : "Doppler: off");
            if (autoDoppler_ && tracking_)
                ImGui::Text("Target: %.6f MHz (%+.0f Hz)", double(targetHz_) / 1e6, double(correctionHz_));
            if (ImGui::Button("Stop bridge##asrtu")) {
                bridge_.stop();
                doppler_.close();
                tracking_ = false;
            }
        } else {
            ImGui::TextWrapped("%s", bridge_.error().empty() ? "Bridge stopped" : bridge_.error().c_str());
            if (ImGui::Button("Start bridge##asrtu")) {
                if (!bridge_.start()) flog::warn("ASRTU bridge: {}", bridge_.error());
            }
        }
    }

private:
    static void writeSamples(dsp::stereo_t* samples, int count, void* context) {
        auto* self = static_cast<AsrtuSink*>(context);
        std::lock_guard<std::mutex> lock(self->mutex_);
        self->bridge_.write(samples, count);
    }

    static void onFrame(ImGui::WaterFall::FFTRedrawArgs, void* context) {
        auto* self = static_cast<AsrtuSink*>(context);
        const auto now = std::chrono::steady_clock::now();
        std::int64_t target = 0, correction = 0;
        {
            std::lock_guard<std::mutex> lock(self->mutex_);
            if (now - self->lastPoll_ < std::chrono::milliseconds(100)) return;
            self->lastPoll_ = now;
            // A sink is not guaranteed to have the same name as the waterfall
            // VFO (SDR++ names the default VFO "Radio"). Prefer the sink's
            // own VFO, then fall back to the selected/first VFO.
            std::string vfoName = self->name_;
            if (gui::waterfall.vfos.find(vfoName) == gui::waterfall.vfos.end()) {
                vfoName = gui::waterfall.selectedVFO;
                if (vfoName.empty() || gui::waterfall.vfos.find(vfoName) ==
                                           gui::waterfall.vfos.end())
                    vfoName = gui::waterfall.vfos.empty()
                                  ? std::string()
                                  : gui::waterfall.vfos.begin()->first;
            }
            self->tracking_ = self->bridge_.active() && self->autoDoppler_ &&
                              !vfoName.empty() &&
                              self->doppler_.read(target, correction);
            if (!self->tracking_) return;
            self->targetHz_ = target;
            self->correctionHz_ = correction;
            self->trackedVfo_ = vfoName;
        }
        // Run tuning on SDR++'s GUI thread, including when the sink menu is
        // collapsed. Do not hold the sample lock while reconfiguring the DSP.
        std::string trackedVfo;
        {
            std::lock_guard<std::mutex> lock(self->mutex_);
            trackedVfo = self->trackedVfo_;
        }
        if (trackedVfo.empty()) return;
        // Put the SDR hardware center 5 kHz above the wanted signal, then put
        // the VFO back on the wanted frequency. The IF/DC leakage stays at the
        // hardware center instead of sitting under the VFO.
        constexpr double kIfOffsetHz = 5000.0;
        constexpr double kRecentersHz = 1000.0;
        const double wantedHz = double(target);
        const double desiredCenter = wantedHz + kIfOffsetHz;
        bool recenter = false;
        {
            std::lock_guard<std::mutex> lock(self->mutex_);
            recenter = self->trackedCenterHz_ <= 0.0 ||
                       std::abs(desiredCenter - self->trackedCenterHz_) >= kRecentersHz;
            if (recenter)
                self->trackedCenterHz_ = desiredCenter;
        }
        if (recenter) {
            sigpath::sourceManager.tune(desiredCenter);
            gui::waterfall.setCenterFrequency(desiredCenter);
            gui::waterfall.setViewOffset(0);
        }
        // Let SDR++'s normal tuner move the VFO to the wanted frequency.
        // The hardware center was placed 5 kHz above it, so the VFO offset is
        // about -5 kHz and the IF/DC leakage stays at the hardware center.
        tuner::tune(tuner::TUNER_MODE_NORMAL, trackedVfo, wantedHz);
    }

    SinkManager::Stream* stream_;
    std::string name_;
    dsp::stream<dsp::stereo_t>* input_ = nullptr;
    dsp::sink::Handler<dsp::stereo_t> samples_;
    dsp::sink::Null<dsp::stereo_t> discardAudio_;
    EventHandler<ImGui::WaterFall::FFTRedrawArgs> frameHandler_;
    asrtu::SharedBridge bridge_;
    asrtu::DopplerReader doppler_;
    std::mutex mutex_, lifecycle_;
    std::chrono::steady_clock::time_point lastPoll_{};
    std::string trackedVfo_;
    double trackedCenterHz_ = 0.0;
    bool draining_ = false;
    bool autoDoppler_ = true;
    bool tracking_ = false;
    std::int64_t targetHz_ = 0, correctionHz_ = 0;
};

class AsrtuSinkModule final : public ModuleManager::Instance {
public:
    explicit AsrtuSinkModule(std::string) {
        provider_.create = create;
        provider_.ctx = this;
        enable();
    }
    ~AsrtuSinkModule() override { disable(); }
    void postInit() override {}
    void enable() override {
        if (enabled_) return;
        sigpath::sinkManager.registerSinkProvider("ASRTU Receiver", provider_);
        enabled_ = true;
    }
    void disable() override {
        if (!enabled_) return;
        sigpath::sinkManager.unregisterSinkProvider("ASRTU Receiver");
        enabled_ = false;
    }
    bool isEnabled() override { return enabled_; }
private:
    static SinkManager::Sink* create(SinkManager::Stream* stream,
                                    std::string name, void*) {
        return new AsrtuSink(stream, std::move(name));
    }
    SinkManager::SinkProvider provider_{};
    bool enabled_ = false;
};
}

MOD_EXPORT void _INIT_() {
    settings.setPath(core::args["root"].s() + "/asrtu_sink_config.json");
    settings.load(json::object());
    settings.enableAutoSave();
}
MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new AsrtuSinkModule(std::move(name));
}
MOD_EXPORT void _DELETE_INSTANCE_(ModuleManager::Instance* instance) { delete instance; }
MOD_EXPORT void _END_() {
    settings.disableAutoSave();
    settings.save();
}
