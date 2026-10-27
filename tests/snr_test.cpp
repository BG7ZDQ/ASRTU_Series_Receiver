#include "snr_scale.h"
#include "snr_history.h"
extern "C" {
#include "sm_api.h"
#include "sm_snr.h"
}

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

static void require(bool condition, const char *message)
{
	if (!condition)
		throw std::runtime_error(message);
}

int main()
{
	try {
		// Independent equiprobable signal/noise signs give exact zero
		// cross terms and known equal I/Q variances; test negative and
		// positive dB.
		for (const double db : {-5.0, 0.0, 5.0, 10.0, 20.0}) {
			for (const double gain : {0.01, 1.0, 100.0}) {
				sm_snr estimator{};
				sm_snr_init(&estimator);
				require(std::isnan(sm_snr_read(&estimator)),
					"Empty estimator has SNR");
				const double sigma =
				    gain /
				    std::sqrt(2.0 * std::pow(10.0, db / 10.0));
				for (unsigned int i = 0; i < SM_SNR_WINDOW;
				     ++i) {
					const double signal =
					    (i & 1U) ? gain : -gain;
					const double ni =
					    (i & 2U) ? sigma : -sigma;
					const double nq =
					    (i & 4U) ? sigma : -sigma;
					sm_snr_push(&estimator,
						    float(signal + ni),
						    float(nq));
				}
				require(std::abs(sm_snr_read(&estimator) - db) <
					    0.001,
					"SNR calibration or gain invariance "
					"failed");
				for (unsigned int i = 0;
				     i < 15U * SM_SNR_WINDOW; ++i)
					sm_snr_push(&estimator, 1.0f, 0.1f);
				require(std::isfinite(sm_snr_read(&estimator)),
					"SNR expired without frames");
				for (unsigned int i = 0; i < 2U * SM_SNR_WINDOW;
				     ++i)
					sm_snr_push(&estimator, 0.0f, 0.0f);
				require(std::isnan(sm_snr_read(&estimator)),
					"Silence has SNR");
				sm_snr_push(
				    &estimator,
				    std::numeric_limits<float>::infinity(),
				    0.0f);
				require(std::isnan(sm_snr_read(&estimator)),
					"Nonfinite data retained SNR");
			}
		}
		sm_decoder *decoder = sm_decoder_new();
		require(decoder != nullptr, "Decoder allocation failed");
		require(std::isnan(sm_decoder_snr(decoder)),
			"New decoder has SNR");
		float silence[3840]{};
		for (int i = 0; i < 60; ++i)
			sm_decoder_feed_iq(decoder, silence, 1920);
		sm_decoder_flush(decoder);
		require(std::isnan(sm_decoder_snr(decoder)),
			"Silent decoder acquired SNR");
		sm_decoder_free(decoder);

		for (const auto range : {std::pair<double, double>{12.1, 12.2},
					 {-25.0, -20.0},
					 {-30.0, 70.0},
					 {900.0, 999.0}}) {
			const auto scale = snrScale(range.first, range.second);
			require(scale.minimum <= 0 && scale.maximum >= 0,
				"Zero not visible");
			require(scale.minimum <= range.first &&
				    scale.maximum >= range.second,
				"Data outside axis");
			require(scale.step >= 1 &&
				    std::floor(scale.step) == scale.step,
				"Noninteger grid");
			require(std::fmod(scale.minimum, scale.step) == 0.0,
				"Zero is not a tick");
			require((scale.maximum - scale.minimum) / scale.step <=
				    6,
				"Crowded grid");
		}
		// Two UI updates 100 ms apart must represent different amounts
		// of audio at 1x versus 30x; irregular actual progress is
		// authoritative.
		SnrHistory realtime, fast;
		realtime.add(1.0, 0.1);
		realtime.add(2.0, 0.2);
		fast.add(1.0, 3.0);
		fast.add(2.0, 6.0);
		require(
		    std::abs((1.0 - fast.position(fast.points().front())) /
				 (1.0 - realtime.position(
					    realtime.points().front())) -
			     30.0) < 1e-9,
		    "Fast replay does not advance the time axis by audio time");
		for (int i = 3; i <= 40; ++i)
			fast.add(2.0, 3.0 * i);
		require(fast.points().size() == 21 &&
			    fast.points().front().seconds == 60.0 &&
			    fast.points().back().seconds == 120.0,
			"Replay history is not the last 60 audio seconds");
		require(!fast.add(9.0, 120.0) &&
			    fast.points().back().value == 2.0,
			"Paused/EOF input changed the SNR display");
		require(!fast.add(std::numeric_limits<double>::quiet_NaN(),
				  130.0) &&
			    fast.points().back().seconds == 120.0,
			"Invalid SNR advanced history");
		fast.add(3.0, 151.5);
		require(
		    std::abs(fast.position(fast.points().front()) - 0.025) <
			1e-9,
		    "Irregular audio progress was replaced with a fixed tick");
		fast.add(4.0, 1.0);
		require(fast.points().size() == 1 &&
			    fast.position(fast.points().back()) == 1.0,
			"Restarted replay retained timestamps from the "
			"previous stream");
		SnrHistory clamped;
		require(clamped.add(-100.0, 1.0, false) &&
			    clamped.points().back().value == kSnrFloorDb &&
			    !clamped.points().back().synchronized,
			"SNR floor or SVR marker was not retained");
		require(clamped.add(8.0, 2.0, true) &&
			    clamped.points().back().synchronized,
			"Es/N0 marker was not retained");
		std::cout << "SNR calibration, validity, silence and scale "
			     "checks passed\n";
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		return 1;
	}
}
