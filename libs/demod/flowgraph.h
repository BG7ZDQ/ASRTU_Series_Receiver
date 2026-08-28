#pragma once

#include "frame_monitor.h"

#include <gnuradio/blocks/probe_signal.h>
#include <gnuradio/top_block.h>
#include <atomic>
#include <memory>
#include <string>

class QWidget;
class SharedIqSource;

namespace gr {
namespace digital {
class costas_loop_cc;
class fll_band_edge_cc;
class pfb_clock_sync_ccf;
class probe_mpsk_snr_est_c;
}
namespace qtgui {
class const_sink_c;
class freq_sink_c;
class freq_sink_f;
class waterfall_sink_c;
class waterfall_sink_f;
}
}

class AsrtuFlowgraph final
{
public:
    using LogCallback = FrameMonitor::Callback;
    struct Options {
        std::string wav_path;
        std::string record_wav_path;
        double input_frequency_hz = 0.0;
        int audio_device_id = -1;
        bool real_if_12khz = false;
        bool shared_iq_bridge = false;
        bool fast_playback = false;
        // Effective replay multiplier when fast_playback is enabled. The
        // replay is capped here (instead of running at the ~60x raw WAV
        // speed) so the waterfall has time to scroll and the decoded frames
        // are spread over a visibly accelerated, but observable, duration.
        double replay_rate = 30.0;
        bool enable_gui = true;
        bool enable_network = true;
        bool enable_openhoshimi_decoder = true;
        bool use_legacy_feedforward_agc = false; // benchmark/regression only
        FrameMonitor::PayloadCallback payload_callback;
        FrameMonitor::PayloadCallback local_candidate_callback;
    };

    AsrtuFlowgraph(LogCallback callback, const Options& options);
    ~AsrtuFlowgraph();

    void start();
    void waitForCompletion();
    void stop();
    bool running() const noexcept { return running_; }

    double snr() const;
    double rssi() const;
    double loopFrequencyHz() const;
    bool stereoIqContentMismatch() const;
    bool inputActive(double timeoutSeconds = 0.5) const;
    bool synced(double timeoutSeconds = 1.5) const;
    std::uint64_t frameCount() const;
    std::uint64_t primaryFrameCount() const;
    std::uint64_t openHoshimiFrameCount() const;
    std::uint64_t suppressedDuplicateCount() const;

    QWidget* inputSpectrumWidget() const;
    QWidget* waterfallWidget() const;
    QWidget* loopSpectrumWidget() const;
    QWidget* constellationWidget() const;

    void setFllBandwidth(double value);
    void setTimingBandwidth(double value);
    void setPhaseBandwidth(double value);
    void setEqualizerGain(double value);
    void resetInputSpectrum();
    void resetWaterfall();
    void resetLoopSpectrum();
    void resetConstellation();

private:
    void build(LogCallback callback, const Options& options);

    gr::top_block_sptr tb_;
    std::shared_ptr<gr::digital::probe_mpsk_snr_est_c> snr_probe_;
    std::shared_ptr<SharedIqSource> shared_iq_source_;
    std::shared_ptr<gr::blocks::probe_signal_f> rssi_probe_;
    std::shared_ptr<gr::blocks::probe_signal_f> i_rms_probe_;
    std::shared_ptr<gr::blocks::probe_signal_f> q_rms_probe_;
    std::shared_ptr<gr::blocks::probe_signal_f> iq_difference_rms_probe_;
    std::shared_ptr<gr::digital::fll_band_edge_cc> fll_;
    std::shared_ptr<gr::digital::pfb_clock_sync_ccf> clock_sync_;
    std::shared_ptr<gr::digital::costas_loop_cc> costas_;
    std::shared_ptr<std::atomic<float>> equalizer_gain_;
    std::shared_ptr<std::atomic<std::int64_t>> last_input_sample_ms_;
    std::shared_ptr<gr::qtgui::freq_sink_c> input_spectrum_;
    std::shared_ptr<gr::qtgui::freq_sink_f> input_spectrum_real_;
    std::shared_ptr<gr::qtgui::waterfall_sink_c> waterfall_;
    std::shared_ptr<gr::qtgui::waterfall_sink_f> waterfall_real_;
    std::shared_ptr<gr::qtgui::freq_sink_c> loop_spectrum_;
    std::shared_ptr<gr::qtgui::const_sink_c> constellation_sink_;
    FrameMonitor::sptr frame_monitor_;
    bool expects_stereo_iq_ = false;
    bool monitors_live_audio_ = false;
    bool running_ = false;
};
