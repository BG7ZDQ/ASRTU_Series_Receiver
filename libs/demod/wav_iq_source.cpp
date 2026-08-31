#include "wav_iq_source.h"

#include <gnuradio/blocks/complex_to_float.h>
#include <gnuradio/blocks/float_to_complex.h>
#include <gnuradio/blocks/multiply_const.h>
#include <gnuradio/blocks/throttle.h>
#include <gnuradio/blocks/wavfile_source.h>
#include <gnuradio/filter/firdes.h>
#include <gnuradio/filter/rational_resampler.h>
#include <gnuradio/gr_complex.h>
#include <gnuradio/io_signature.h>
#include <gnuradio/sptr_magic.h>

#include <numeric>
#include <stdexcept>
#include <vector>

namespace {
std::vector<float> resamplerTaps(unsigned interpolation, unsigned decimation)
{
    const double ratio = double(interpolation) / double(decimation);
    const double transition = ratio >= 1.0 ? 0.1 : ratio * 0.1;
    const double cutoff = ratio >= 1.0 ? 0.45 : ratio * 0.5 - transition / 2.0;
    return gr::filter::firdes::low_pass(
        interpolation, interpolation, cutoff, transition,
        gr::fft::window::WIN_KAISER, 7.0);
}
}

WavIqSource::sptr WavIqSource::make(const std::string& path,
                                    int outputSampleRate,
                                    bool repeat,
                                    bool throttle)
{
    return gnuradio::make_block_sptr<WavIqSource>(
        path, outputSampleRate, repeat, throttle);
}

WavIqSource::WavIqSource(const std::string& path,
                         int outputSampleRate,
                         bool repeat,
                         bool throttle)
    : gr::hier_block2("asrtu_wav_iq_source",
                      gr::io_signature::make(0, 0, 0),
                      gr::io_signature::make(2, 2, sizeof(float)))
{
    if (outputSampleRate <= 0)
        throw std::invalid_argument("output sample rate must be positive");

    const auto source = gr::blocks::wavfile_source::make(path.c_str(), repeat);
    if (source->channels() < 1)
        throw std::runtime_error("WAV file contains no audio channels");

    gr::basic_block_sptr iSource = source;
    gr::basic_block_sptr qSource;
    int iPort = 0;
    int qPort = 0;
    if (source->channels() >= 2) {
        qSource = source;
        qPort = 1;
    } else {
        const auto zeroQ = gr::blocks::multiply_const_ff::make(0.0f);
        connect(source, 0, zeroQ, 0);
        qSource = zeroQ;
    }

    const unsigned inputRate = source->sample_rate();
    if (inputRate != unsigned(outputSampleRate)) {
        const unsigned divisor = std::gcd(inputRate, unsigned(outputSampleRate));
        const unsigned interpolation = unsigned(outputSampleRate) / divisor;
        const unsigned decimation = inputRate / divisor;
        const auto taps = resamplerTaps(interpolation, decimation);
        const auto iResampler = gr::filter::rational_resampler_fff::make(
            interpolation, decimation, taps);
        const auto qResampler = gr::filter::rational_resampler_fff::make(
            interpolation, decimation, taps);
        connect(iSource, iPort, iResampler, 0);
        connect(qSource, qPort, qResampler, 0);
        iSource = iResampler;
        qSource = qResampler;
        iPort = 0;
        qPort = 0;
    }

    if (!throttle) {
        connect(iSource, iPort, self(), 0);
        connect(qSource, qPort, self(), 1);
        return;
    }

    const auto combine = gr::blocks::float_to_complex::make(1);
    const auto limiter = gr::blocks::throttle::make(
        sizeof(gr_complex), outputSampleRate, true);
    const auto split = gr::blocks::complex_to_float::make(1);
    connect(iSource, iPort, combine, 0);
    connect(qSource, qPort, combine, 1);
    connect(combine, 0, limiter, 0);
    connect(limiter, 0, split, 0);
    connect(split, 0, self(), 0);
    connect(split, 1, self(), 1);
}
