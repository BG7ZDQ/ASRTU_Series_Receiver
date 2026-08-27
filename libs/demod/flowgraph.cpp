#include "flowgraph.h"
#include "shared_iq_source.h"

#include <gnuradio/analog/agc2_cc.h>
#include <gnuradio/analog/feedforward_agc_cc.h>
#include <gnuradio/analog/sig_source.h>
#include <gnuradio/blocks/complex_to_float.h>
#include <gnuradio/blocks/complex_to_real.h>
#include <gnuradio/blocks/delay.h>
#include <gnuradio/blocks/float_to_complex.h>
#include <gnuradio/blocks/multiply.h>
#include <gnuradio/blocks/multiply_const.h>
#include <gnuradio/blocks/nlog10_ff.h>
#include <gnuradio/blocks/null_sink.h>
#include <gnuradio/blocks/probe_signal.h>
#include <gnuradio/blocks/rms_cf.h>
#include <gnuradio/blocks/rms_ff.h>
#include <gnuradio/blocks/unpack_k_bits_bb.h>
#include <gnuradio/blocks/wavfile_sink.h>
#include <gnuradio/digital/adaptive_algorithm.h>
#include <gnuradio/digital/constellation.h>
#include <gnuradio/digital/costas_loop_cc.h>
#include <gnuradio/digital/fll_band_edge_cc.h>
#include <gnuradio/digital/linear_equalizer.h>
#include <gnuradio/digital/mpsk_snr_est.h>
#include <gnuradio/digital/pfb_clock_sync_ccf.h>
#include <gnuradio/digital/probe_mpsk_snr_est_c.h>
#include <gnuradio/digital/symbol_sync_cc.h>
#include <gnuradio/digital/timing_error_detector_type.h>
#include <gnuradio/fft/window.h>
#include <gnuradio/filter/firdes.h>
#include <gnuradio/filter/fir_filter_blk.h>
#include <gnuradio/lilacsat/fec_decode_b.h>
#include <gnuradio/lilacsat/vitfilt27_fb.h>
#include <gnuradio/network/socket_pdu.h>
#include <gnuradio/qtgui/const_sink_c.h>
#include <gnuradio/qtgui/freq_sink_c.h>
#include <gnuradio/qtgui/freq_sink_f.h>
#include <gnuradio/qtgui/waterfall_sink_c.h>
#include <gnuradio/qtgui/waterfall_sink_f.h>
#include <gnuradio/zeromq/pub_msg_sink.h>
#include <hyacinthsat/stereo_iq_source.h>
#include <hyacinthsat/wav_iq_source.h>

#include <QWidget>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace {
constexpr double kIfRate = 48000.0;
constexpr double kRealIfCenterHz = 12000.0;
// A real 0..24 kHz spectrum is plotted as -12..+12 kHz so that the
// radio's 12 kHz IF appears at 0 Hz in the GUI.
constexpr double kRealIfDisplayCenterHz = -kRealIfCenterHz;
constexpr double kSps = 5.0;
constexpr float kAlpha = 0.35f;

class AdjustableLmsAlgorithm final : public gr::digital::adaptive_algorithm
{
public:
    AdjustableLmsAlgorithm(gr::digital::constellation_sptr constellation,
                           std::shared_ptr<std::atomic<float>> gain)
        : adaptive_algorithm(gr::digital::adaptive_algorithm_t::LMS,
                             std::move(constellation)),
          gain_(std::move(gain))
    {
    }

    gr_complex update_tap(const gr_complex tap,
                          const gr_complex& input,
                          const gr_complex error,
                          const gr_complex /*decision*/) override
    {
        return std::conj(std::conj(tap) + gain_->load(std::memory_order_relaxed) *
                                          input * std::conj(error));
    }

    void initialize_taps(std::vector<gr_complex>& taps) override
    {
        std::fill(taps.begin(), taps.end(), gr_complex(0.0f, 0.0f));
    }

private:
    std::shared_ptr<std::atomic<float>> gain_;
};
}

AsrtuFlowgraph::AsrtuFlowgraph(LogCallback callback, Options options)
{
    build(std::move(callback), options);
}

AsrtuFlowgraph::~AsrtuFlowgraph()
{
    stop();
}

void AsrtuFlowgraph::build(LogCallback callback, const Options& options)
{
    expects_stereo_iq_ = !options.shared_iq_bridge && !options.real_if_12khz;
    tb_ = gr::make_top_block("Astro-series satellite C++/Qt demodulator", true);

    gr::basic_block_sptr source;
    gr::basic_block_sptr complexSource;
    const auto to_complex = gr::blocks::float_to_complex::make(1);
    if (options.shared_iq_bridge) {
        shared_iq_source_ = SharedIqSource::make();
        source = shared_iq_source_;
        complexSource = source;
    } else if (options.wav_path.empty()) {
        source = gr::hyacinthsat::stereo_iq_source::make(
            48000, options.audio_device_id, false);
    } else {
        source = gr::hyacinthsat::wav_iq_source::make(
            options.wav_path, 48000, false, options.enable_gui);
    }
    if (!complexSource)
        complexSource = to_complex;
    const auto oscillator = gr::analog::sig_source_c::make(
        kIfRate, gr::analog::GR_COS_WAVE, options.input_frequency_hz,
        1.0, gr_complex(0.0f, 0.0f));
    const auto mixer = gr::blocks::multiply_cc::make(1);
    const auto gain = gr::blocks::multiply_const_cc::make(gr_complex(10.0f, 0.0f));
    const auto lowpass = gr::filter::fir_filter_ccf::make(
        1, gr::filter::firdes::low_pass(1.0, kIfRate, 10000.0, 2000.0,
                                        gr::fft::window::WIN_HAMMING, 6.76));
    // Causal O(N) AGC: react quickly to overload, release slowly through
    // fades, and never amplify silence/noise by more than 40 dB. Unlike the
    // former 1024-sample feed-forward AGC this adds no 21.3 ms look-ahead.
    gr::basic_block_sptr agc;
    if (options.use_legacy_feedforward_agc)
        agc = gr::analog::feedforward_agc_cc::make(1024, 1.0f);
    else
        agc = gr::analog::agc2_cc::make(
            0.10f, 0.001f, 1.0f, 1.0f, 100.0f);

    fll_ = gr::digital::fll_band_edge_cc::make(kSps, kAlpha, 100, 0.01f);
    const auto rrc = gr::filter::firdes::root_raised_cosine(
        16.0, 16.0, 1.0 / kSps, kAlpha, int(11 * kSps * 16));
    clock_sync_ = gr::digital::pfb_clock_sync_ccf::make(
        kSps, 0.05f, rrc, 16, 8.0f, 0.05f, 2);
    costas_ = gr::digital::costas_loop_cc::make(0.1f, 2, false);

    auto bpsk = gr::digital::constellation_bpsk::make();
    bpsk->set_npwr(1.0);
    equalizer_gain_ = std::make_shared<std::atomic<float>>(0.05f);
    auto algorithm = std::make_shared<AdjustableLmsAlgorithm>(bpsk, equalizer_gain_);
    const auto equalizer = gr::digital::linear_equalizer::make(
        2, 2, algorithm, true, {}, "corr_est");
    const auto to_real = gr::blocks::complex_to_real::make(1);

    const auto delay = gr::blocks::delay::make(sizeof(float), 1);
    const auto viterbi_a = gr::lilacsat::vitfilt27_fb::make();
    const auto viterbi_b = gr::lilacsat::vitfilt27_fb::make();
    const auto unpack_a = gr::blocks::unpack_k_bits_bb::make(8);
    const auto unpack_b = gr::blocks::unpack_k_bits_bb::make(8);
    const auto fec_a = gr::lilacsat::fec_decode_b::make(223, true, false, false);
    const auto fec_b = gr::lilacsat::fec_decode_b::make(223, true, false, false);

    // Synchronizer diversity based on the useful topology of
    // Lilacsat-soundmodem-CLI. Input conditioning is shared, so its expensive
    // peak-scanning AGC, FFT/UI and network work are not duplicated.
    const auto parallel_fll = gr::digital::fll_band_edge_cc::make(
        kSps, kAlpha, 100, 0.068f);
    const auto parallel_clock = gr::digital::symbol_sync_cc::make(
        gr::digital::TED_GARDNER, kSps, 0.01f,
        float(std::sqrt(2.0) / 2.0), 1.0f, 0.05f, 1);
    const auto parallel_costas = gr::digital::costas_loop_cc::make(0.068f, 2, false);
    const auto parallel_real = gr::blocks::complex_to_real::make(1);
    const auto parallel_delay = gr::blocks::delay::make(sizeof(float), 1);
    const auto parallel_viterbi_a = gr::lilacsat::vitfilt27_fb::make();
    const auto parallel_viterbi_b = gr::lilacsat::vitfilt27_fb::make();
    const auto parallel_unpack_a = gr::blocks::unpack_k_bits_bb::make(8);
    const auto parallel_unpack_b = gr::blocks::unpack_k_bits_bb::make(8);
    const auto parallel_fec_a = gr::lilacsat::fec_decode_b::make(223, true, false, false);
    const auto parallel_fec_b = gr::lilacsat::fec_decode_b::make(223, true, false, false);

    snr_probe_ = gr::digital::probe_mpsk_snr_est_c::make(
        gr::digital::SNR_EST_SVR, 10000, 0.001);
    const auto rms = gr::blocks::rms_cf::make(0.0001);
    const auto db = gr::blocks::nlog10_ff::make(20.0f, 1, 0.0f);
    rssi_probe_ = gr::blocks::probe_signal_f::make();
    if (expects_stereo_iq_) {
        const auto iRms = gr::blocks::rms_ff::make(0.001f);
        const auto qRms = gr::blocks::rms_ff::make(0.001f);
        i_rms_probe_ = gr::blocks::probe_signal_f::make();
        q_rms_probe_ = gr::blocks::probe_signal_f::make();
        tb_->connect(source, 0, iRms, 0);
        tb_->connect(source, 1, qRms, 0);
        tb_->connect(iRms, 0, i_rms_probe_, 0);
        tb_->connect(qRms, 0, q_rms_probe_, 0);
    }

    if (options.enable_gui) {
        if (options.real_if_12khz) {
            input_spectrum_real_ = gr::qtgui::freq_sink_f::make(
                1024, gr::fft::window::WIN_BLACKMAN_hARRIS,
                kRealIfDisplayCenterHz, kIfRate,
                "Real IF input spectrum", 1, nullptr);
            input_spectrum_real_->set_plot_pos_half(true);
            input_spectrum_real_->set_update_time(0.10);
            input_spectrum_real_->set_y_axis(-140, 10);
            input_spectrum_real_->disable_legend();

            waterfall_real_ = gr::qtgui::waterfall_sink_f::make(
                1024, gr::fft::window::WIN_BLACKMAN_hARRIS,
                kRealIfDisplayCenterHz, kIfRate,
                "Real IF input waterfall", 1, nullptr);
            waterfall_real_->set_plot_pos_half(true);
            waterfall_real_->set_update_time(0.10);
            waterfall_real_->set_intensity_range(-100, 0);
            waterfall_real_->disable_legend();
        } else {
            input_spectrum_ = gr::qtgui::freq_sink_c::make(
                1024, gr::fft::window::WIN_BLACKMAN_hARRIS, 0.0, kIfRate,
                "Input spectrum", 1, nullptr);
            input_spectrum_->set_update_time(0.10);
            input_spectrum_->set_y_axis(-140, 10);
            input_spectrum_->disable_legend();

            waterfall_ = gr::qtgui::waterfall_sink_c::make(
                1024, gr::fft::window::WIN_BLACKMAN_hARRIS, 0.0, kIfRate,
                "Input waterfall", 1, nullptr);
            waterfall_->set_update_time(0.10);
            waterfall_->set_intensity_range(-100, 0);
            waterfall_->disable_legend();
        }

        loop_spectrum_ = gr::qtgui::freq_sink_c::make(
            1024, gr::fft::window::WIN_BLACKMAN_hARRIS, 0.0, kIfRate,
            "Loop spectrum", 2, nullptr);
        loop_spectrum_->set_update_time(0.10);
        loop_spectrum_->set_y_axis(-100, 0);
        loop_spectrum_->set_line_color(0, "blue");
        loop_spectrum_->set_line_color(1, "red");
        loop_spectrum_->set_line_label(0, "Before FLL");
        loop_spectrum_->set_line_label(1, "After FLL");
        loop_spectrum_->disable_legend();

        constellation_sink_ = gr::qtgui::const_sink_c::make(
            1024, "BPSK constellation", 1, nullptr);
        constellation_sink_->set_update_time(0.10);
        constellation_sink_->set_x_axis(-2.0, 2.0);
        constellation_sink_->set_y_axis(-2.0, 2.0);
        constellation_sink_->disable_legend();
    }

    frame_monitor_ = FrameMonitor::make(
        std::move(callback), options.payload_callback);

    if (options.enable_gui && options.real_if_12khz) {
        // Raw mono float samples feed both GUI sinks before any I+j0
        // construction or frequency translation.
        tb_->connect(source, 0, input_spectrum_real_, 0);
        tb_->connect(source, 0, waterfall_real_, 0);
    }
    if (!options.shared_iq_bridge)
        tb_->connect(source, 0, to_complex, 0);
    if (options.real_if_12khz && !options.shared_iq_bridge) {
        const auto zero = gr::analog::sig_source_f::make(
            kIfRate, gr::analog::GR_CONST_WAVE, 0.0, 0.0, 0.0);
        const auto unusedRightChannel = gr::blocks::null_sink::make(sizeof(float));
        tb_->connect(zero, 0, to_complex, 1);
        // Both audio source blocks expose two mandatory outputs. Drain the
        // unused channel in real-IF mode so GNU Radio can validate the graph.
        tb_->connect(source, 1, unusedRightChannel, 0);
    } else if (!options.shared_iq_bridge) {
        tb_->connect(source, 1, to_complex, 1);
    }
    if (!options.record_wav_path.empty()) {
        if (!options.wav_path.empty())
            throw std::invalid_argument("WAV playback and live recording cannot be enabled together");
        const int recordingChannels = options.real_if_12khz ? 1 : 2;
        const auto recorder = gr::blocks::wavfile_sink::make(
            options.record_wav_path.c_str(), recordingChannels, unsigned(kIfRate),
            gr::blocks::FORMAT_WAV, gr::blocks::FORMAT_PCM_16, false);
        if (options.shared_iq_bridge) {
            const auto splitIq = gr::blocks::complex_to_float::make(1);
            tb_->connect(complexSource, 0, splitIq, 0);
            tb_->connect(splitIq, 0, recorder, 0);
            tb_->connect(splitIq, 1, recorder, 1);
        } else {
            tb_->connect(source, 0, recorder, 0);
            if (!options.real_if_12khz)
                tb_->connect(source, 1, recorder, 1);
        }
    }
    tb_->connect(complexSource, 0, mixer, 0);
    tb_->connect(oscillator, 0, mixer, 1);
    if (options.enable_gui) {
        if (!options.real_if_12khz) {
            tb_->connect(complexSource, 0, input_spectrum_, 0);
            tb_->connect(complexSource, 0, waterfall_, 0);
        }
    }
    tb_->connect(mixer, 0, gain, 0);
    tb_->connect(gain, 0, lowpass, 0);
    if (options.enable_gui)
        tb_->connect(gain, 0, loop_spectrum_, 0);
    tb_->connect(lowpass, 0, agc, 0);
    tb_->connect(lowpass, 0, rms, 0);
    tb_->connect(rms, 0, db, 0);
    tb_->connect(db, 0, rssi_probe_, 0);
    tb_->connect(agc, 0, fll_, 0);
    if (options.enable_gui)
        tb_->connect(fll_, 0, loop_spectrum_, 1);
    tb_->connect(fll_, 0, clock_sync_, 0);
    tb_->connect(clock_sync_, 0, costas_, 0);
    tb_->connect(costas_, 0, equalizer, 0);
    tb_->connect(equalizer, 0, to_real, 0);
    tb_->connect(equalizer, 0, snr_probe_, 0);
    if (options.enable_gui)
        tb_->connect(equalizer, 0, constellation_sink_, 0);
    tb_->connect(to_real, 0, viterbi_a, 0);
    tb_->connect(to_real, 0, delay, 0);
    tb_->connect(delay, 0, viterbi_b, 0);
    tb_->connect(viterbi_a, 0, unpack_a, 0);
    tb_->connect(viterbi_b, 0, unpack_b, 0);
    tb_->connect(unpack_a, 0, fec_a, 0);
    tb_->connect(unpack_b, 0, fec_b, 0);

    if (options.enable_parallel_decoder) {
        tb_->connect(agc, 0, parallel_fll, 0);
        tb_->connect(parallel_fll, 0, parallel_clock, 0);
        tb_->connect(parallel_clock, 0, parallel_costas, 0);
        tb_->connect(parallel_costas, 0, parallel_real, 0);
        tb_->connect(parallel_real, 0, parallel_viterbi_a, 0);
        tb_->connect(parallel_real, 0, parallel_delay, 0);
        tb_->connect(parallel_delay, 0, parallel_viterbi_b, 0);
        tb_->connect(parallel_viterbi_a, 0, parallel_unpack_a, 0);
        tb_->connect(parallel_viterbi_b, 0, parallel_unpack_b, 0);
        tb_->connect(parallel_unpack_a, 0, parallel_fec_a, 0);
        tb_->connect(parallel_unpack_b, 0, parallel_fec_b, 0);
    }

    for (const auto& fec : { fec_a, fec_b })
        tb_->msg_connect(fec, "out", frame_monitor_, "primary");
    if (options.enable_parallel_decoder) {
        for (const auto& fec : { parallel_fec_a, parallel_fec_b })
            tb_->msg_connect(fec, "out", frame_monitor_, "parallel");
    }

    if (options.enable_network) {
        const auto tcp = gr::network::socket_pdu::make(
            "TCP_SERVER", "127.0.0.1", "9985", 10000, false);
        char zmqAddress[] = "tcp://127.0.0.1:5555";
        const auto zmq = gr::zeromq::pub_msg_sink::make(zmqAddress, 1000, true);
        tb_->msg_connect(frame_monitor_, "out", tcp, "pdus");
        tb_->msg_connect(frame_monitor_, "out", zmq, "in");
    }
}

void AsrtuFlowgraph::start()
{
    if (!running_) {
        tb_->start();
        running_ = true;
    }
}

void AsrtuFlowgraph::stop()
{
    if (running_) {
        tb_->stop();
        tb_->wait();
        running_ = false;
    }
}

void AsrtuFlowgraph::waitForCompletion()
{
    if (running_) {
        tb_->wait();
        running_ = false;
    }
}

double AsrtuFlowgraph::snr() const { return snr_probe_->snr(); }
double AsrtuFlowgraph::rssi() const { return rssi_probe_->level(); }

bool AsrtuFlowgraph::inputActive(double timeoutSeconds) const
{
    if (!shared_iq_source_)
        return true;
    const auto timeout = std::chrono::milliseconds(
        std::max(1LL, static_cast<long long>(std::llround(timeoutSeconds * 1000.0))));
    return shared_iq_source_->hasRecentSamples(timeout);
}

bool AsrtuFlowgraph::stereoIqContentMismatch() const
{
    if (!expects_stereo_iq_ || !i_rms_probe_ || !q_rms_probe_)
        return false;
    const double i = std::abs(double(i_rms_probe_->level()));
    const double q = std::abs(double(q_rms_probe_->level()));
    const double stronger = std::max(i, q);
    const double weaker = std::min(i, q);
    // Ignore silence. A sustained >30 dB channel imbalance is characteristic
    // of mono/real audio being supplied to a complex RAW I/Q input.
    return stronger > 1.0e-3 && weaker < stronger * 0.0316227766;
}

double AsrtuFlowgraph::loopFrequencyHz() const
{
    const double fllHz = fll_->get_frequency() * kIfRate / (2.0 * M_PI);
    const double costasHz = costas_->get_frequency() * (kIfRate / kSps) / (2.0 * M_PI);
    return fllHz + costasHz;
}

bool AsrtuFlowgraph::synced(double timeoutSeconds) const
{
    return frame_monitor_->frameCount() > 0 &&
           frame_monitor_->secondsSinceFrame() <= timeoutSeconds;
}

std::uint64_t AsrtuFlowgraph::frameCount() const { return frame_monitor_->frameCount(); }
std::uint64_t AsrtuFlowgraph::primaryFrameCount() const
{
    return frame_monitor_->primaryFrameCount();
}
std::uint64_t AsrtuFlowgraph::parallelFrameCount() const
{
    return frame_monitor_->parallelFrameCount();
}
QWidget* AsrtuFlowgraph::inputSpectrumWidget() const
{
    return input_spectrum_real_ ? input_spectrum_real_->qwidget()
                                : input_spectrum_->qwidget();
}

QWidget* AsrtuFlowgraph::waterfallWidget() const
{
    return waterfall_real_ ? waterfall_real_->qwidget()
                           : waterfall_->qwidget();
}
QWidget* AsrtuFlowgraph::loopSpectrumWidget() const { return loop_spectrum_->qwidget(); }
QWidget* AsrtuFlowgraph::constellationWidget() const { return constellation_sink_->qwidget(); }
void AsrtuFlowgraph::setFllBandwidth(double value) { fll_->set_loop_bandwidth(float(value)); }
void AsrtuFlowgraph::setTimingBandwidth(double value) { clock_sync_->set_loop_bandwidth(float(value)); }
void AsrtuFlowgraph::setPhaseBandwidth(double value) { costas_->set_loop_bandwidth(float(value)); }
void AsrtuFlowgraph::setEqualizerGain(double value)
{
    equalizer_gain_->store(float(value), std::memory_order_relaxed);
}

void AsrtuFlowgraph::resetInputSpectrum()
{
    if (input_spectrum_real_) {
        input_spectrum_real_->set_plot_pos_half(true);
        input_spectrum_real_->set_frequency_range(kRealIfDisplayCenterHz, kIfRate);
        input_spectrum_real_->set_y_axis(-140.0, 10.0);
        input_spectrum_real_->reset();
    } else {
        input_spectrum_->set_frequency_range(0.0, kIfRate);
        input_spectrum_->set_y_axis(-140.0, 10.0);
        input_spectrum_->reset();
    }
}

void AsrtuFlowgraph::resetWaterfall()
{
    if (waterfall_real_) {
        waterfall_real_->set_plot_pos_half(true);
        waterfall_real_->set_frequency_range(kRealIfDisplayCenterHz, kIfRate);
        waterfall_real_->set_intensity_range(-100.0, 0.0);
    } else {
        waterfall_->set_frequency_range(0.0, kIfRate);
        waterfall_->set_intensity_range(-100.0, 0.0);
    }
}

void AsrtuFlowgraph::resetLoopSpectrum()
{
    loop_spectrum_->set_frequency_range(0.0, kIfRate);
    loop_spectrum_->set_y_axis(-100.0, 0.0);
    loop_spectrum_->reset();
}

void AsrtuFlowgraph::resetConstellation()
{
    constellation_sink_->set_x_axis(-2.0, 2.0);
    constellation_sink_->set_y_axis(-2.0, 2.0);
    constellation_sink_->reset();
}
