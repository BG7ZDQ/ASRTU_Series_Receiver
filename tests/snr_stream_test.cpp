#include "openhoshimi_decoder_sink.h"
#include "svr_snr_sink.h"
#include <cmath>
#include <iostream>
#include <random>
#include <stdexcept>

static void require(bool condition, const char *message)
{
	if (!condition)
		throw std::runtime_error(message);
}
static std::vector<SnrSample> runSvr(const std::vector<gr_complex> &data,
				     int chunk)
{
	auto buffer = std::make_shared<SnrSampleBuffer>(false);
	auto sink = SvrSnrSink::make(buffer);
	gr_vector_void_star output;
	for (int offset = 0; offset < int(data.size()) - 1;) {
		const int count =
		    std::min(chunk, int(data.size()) - 1 - offset);
		gr_vector_const_void_star input{data.data() + offset};
		require(sink->work(count, input, output) == count,
			"SVR consumption changed");
		offset += count;
	}
	return buffer->take();
}
static std::vector<SnrSample> runDecoder(const std::vector<gr_complex> &data,
					 int chunk)
{
	auto buffer = std::make_shared<SnrSampleBuffer>();
	auto sink = OpenHoshimiDecoderSink::make(buffer);
	// A distinctive fallback makes lock/algorithm selection observable.
	for (std::uint64_t tick = 1; tick <= 50; ++tick)
		buffer->svr(tick, -10.0);
	gr_vector_void_star output;
	for (int offset = 0; offset < int(data.size());) {
		const int count = std::min(chunk, int(data.size()) - offset);
		gr_vector_const_void_star input{data.data() + offset};
		require(sink->work(count, input, output) == count,
			"Decoder consumption changed");
		offset += count;
	}
	require(sink->processedSamples() == data.size(),
		"Sample clock changed");
	return buffer->take();
}
static void compare(const std::vector<SnrSample> &a,
		    const std::vector<SnrSample> &b)
{
	require(a.size() == 50 && b.size() == 50,
		"Lost 200 ms samples between UI refreshes");
	for (std::size_t i = 0; i < a.size(); ++i) {
		require(a[i].seconds == double(i + 1) * 0.2 &&
			    a[i].seconds == b[i].seconds,
			"Timestamps depend on scheduler batch size");
		require(a[i].synchronized == b[i].synchronized,
			"Historical lock selection changed");
		require((std::isnan(a[i].value) && std::isnan(b[i].value)) ||
			    std::abs(a[i].value - b[i].value) < 1e-10,
			"SNR depends on scheduler batch size");
	}
}
int main()
{
	try {
		std::mt19937 random(321);
		std::normal_distribution<float> noise(0.0f, 0.2f);
		std::vector<gr_complex> symbols(96001);
		for (std::size_t i = 0; i < symbols.size(); ++i) {
			// Changing amplitude exposes replacing intermediate
			// readings with repeated copies of the last value in a
			// work batch.
			const float amplitude = (i / 1920) % 2 ? 0.4f : 1.0f;
			symbols[i] = {
			    ((random() & 1U) ? amplitude : -amplitude) +
				noise(random),
			    noise(random)};
		}
		const auto svr = runSvr(symbols, 97);
		compare(svr, runSvr(symbols, 32768));
		gr::digital::mpsk_snr_est_svr reference(0.001);
		reference.update(96000, symbols.data());
		require(std::abs(reference.snr() - svr.back().value) < 1e-10,
			"SVR kernel/alpha differs from original GNU Radio "
			"estimator");
		require(std::abs(svr[20].value - svr[21].value) > 1.0,
			"Intermediate SNR variation was flattened");

		std::vector<gr_complex> iq(480000);
		float sign = 1.0f;
		for (std::size_t i = 0; i < iq.size(); ++i) {
			if (i % 5 == 0)
				sign = random() & 1U ? 0.7f : -0.7f;
			const double phase = 2.0 * 3.141592653589793 * 100.0 *
					     double(i) / 48000.0;
			iq[i] = {float(sign * std::cos(phase)) +
				     0.1f * noise(random),
				 float(sign * std::sin(phase)) +
				     0.1f * noise(random)};
		}
		const auto decoded = runDecoder(iq, 127);
		compare(decoded, runDecoder(iq, 65536));
		require(decoded.back().synchronized,
			"BPSK did not lock in streaming test");

		SnrSampleBuffer joined;
		joined.decoder(1, 5.0, true);
		joined.decoder(2, 6.0, false);
		auto first = joined.take();
		require(first.size() == 1 && first[0].synchronized,
			"Locked samples wait unnecessarily for SVR");
		joined.svr(1, -20.0); // Late producer must not resurrect
				      // consumed samples.
		joined.svr(2, -2.0);
		auto second = joined.take();
		require(
		    second.size() == 1 && !second[0].synchronized &&
			second[0].value == -2.0,
		    "Unlocked reading matched the wrong historical interval");
		require(joined.take().empty(),
			"Pause/EOF replayed already consumed samples");
		SnrSampleBuffer bounded(false);
		for (std::uint64_t i = 1; i <= 10000; ++i)
			bounded.svr(i, 1.0);
		auto recent = bounded.take();
		require(
		    recent.size() == SnrSampleBuffer::capacity &&
			recent.back().seconds == 2000.0,
		    "Slow UI causes unbounded history or loses latest data");
		std::cout << "Dense SNR sampling, batch invariance, historical "
			     "selection and bounded buffering passed\n";
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
