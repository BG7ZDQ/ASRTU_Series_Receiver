#include "snr_scale.h"
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
		// Exact Gaussian second/fourth moments in an eight-value
		// fixture. Rotating every sample must not affect the blind
		// estimate.
		const double outer = std::sqrt(1.0 + std::sqrt(6.0));
		const double inner = std::sqrt(1.0 - std::sqrt(6.0) / 3.0);
		const double noiseSigns[] = {outer, -outer, inner, -inner,
					     inner, -inner, inner, -inner};
		for (const double db : {-5.0, 0.0, 5.0, 10.0, 20.0}) {
			for (const double gain : {0.01, 1.0, 100.0}) {
				sm_snr estimator{};
				sm_snr_init(&estimator);
				const double sigma =
				    gain /
				    std::sqrt(2.0 * std::pow(10.0, db / 10.0));
				for (unsigned int i = 0; i < SM_SNR_WINDOW;
				     ++i) {
					const double re =
					    (i & 1U ? gain : -gain) +
					    sigma * noiseSigns[(i / 2U) % 8U];
					const double im =
					    sigma * noiseSigns[(i / 16U) % 8U];
					const double phase = 0.2 * i;
					sm_snr_push(
					    &estimator,
					    float(re * std::cos(phase) -
						  im * std::sin(phase)),
					    float(re * std::sin(phase) +
						  im * std::cos(phase)));
				}
				require(std::abs(estimator.blind_db - db) <
					    0.001,
					"Blind SNR calibration/rotation/gain "
					"failed");
				sm_snr_init(&estimator);
				for (unsigned int i = 0; i < SM_SNR_WINDOW; ++i)
					sm_snr_push(&estimator, 0.0f, 0.0f);
				require(std::isnan(estimator.blind_db),
					"Silent blind estimator has SNR");
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
		std::cout << "SNR calibration, validity, silence and scale "
			     "checks passed\n";
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		return 1;
	}
}
