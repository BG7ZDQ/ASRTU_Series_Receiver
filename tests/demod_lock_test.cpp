#include "demod_lock_status.h"
extern "C" {
#include "sm_api.h"
#include "sm_lock.h"
}

#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

static void require(bool condition, const char *message)
{
	if (!condition)
		throw std::runtime_error(message);
}

static void window(sm_lock &state, int kind = 0)
{
	for (unsigned int i = 0; i < SM_LOCK_WINDOW; ++i) {
		float re = (i & 1U) ? 1.0f : -1.0f;
		float im = (i & 2U) ? 0.2f : -0.2f;
		if (kind == 1)
			re = 0.0f, im = 0.0f; // silence
		if (kind == 2)
			re *= 0.2f; // circular noise covariance
		if (kind == 3)
			re = 1.0f + re * 0.2f; // noisy CW
		if (kind == 4) {	       // rotating constellation
			const float phase = float(i) * 0.02f;
			im = re * std::sin(phase);
			re *= std::cos(phase);
		}
		if (kind == 7) { // between acquire and hold thresholds
			re = ((i & 1U) ? 0.5f : -0.5f) +
			     ((i & 2U) ? 0.7f : -0.7f);
			im = (i & 4U) ? 0.7f : -0.7f;
		}
		sm_lock_push(&state, re, im, kind == 5 ? 1.0f : 0.0f,
			     kind != 6);
	}
}

int main()
{
	try {
		sm_lock state{};
		window(state);
		window(state);
		require(!state.locked, "Acquired without debounce");
		window(state);
		require(state.locked, "BPSK did not acquire");
		for (int i = 0; i < 4; ++i)
			window(state, 7);
		require(state.locked,
			"Weak signal within hold threshold lost lock");
		window(state, 2);
		require(state.locked, "Single bad window flickered");
		window(state);
		require(state.locked, "Recovery lost lock");
		window(state, 2);
		window(state, 2);
		require(!state.locked, "Noise retained lock");
		for (int kind = 1; kind <= 6; ++kind) {
			sm_lock_init(&state);
			for (int i = 0; i < 4; ++i)
				window(state, kind);
			require(!state.locked,
				"Invalid signal or timing acquired");
		}
		sm_lock_init(&state);
		for (int i = 0; i < 4; ++i)
			window(state, 7);
		require(!state.locked, "Hold-only signal acquired new lock");
		for (int i = 0; i < 3; ++i)
			window(state);
		require(state.locked, "Reacquisition failed");
		sm_lock_push(&state, std::numeric_limits<float>::quiet_NaN(), 0,
			     0, 1);
		require(!state.locked, "Nonfinite input retained lock");

		DemodLockStatus status;
		require(!status.locked(1000), "Initial stale lock");
		status.update(true, 1000);
		require(status.locked(1499) && !status.locked(1500),
			"Input timeout failed");
		status.update(false, 1501);
		require(!status.locked(1501), "Unlock was not immediate");
		status.update(true, 2000);
		require(status.locked(2001), "Fresh lock was not published");

		// Run real LPF/FLL/Gardner/Costas on random BPSK without CCSDS
		// framing. Lock must be possible with zero successfully decoded
		// FEC frames.
		std::unique_ptr<sm_decoder, decltype(&sm_decoder_free)> decoder(
		    sm_decoder_new(), sm_decoder_free);
		require(bool(decoder), "Decoder allocation failed");
		require(!sm_decoder_locked(decoder.get()),
			"Initial decoder lock");
		std::vector<float> iq(3840);
		unsigned int random = 1234567;
		float bit = 0.5f;
		bool everLocked = false;
		int validFrames = 0;
		for (int block = 0; block < 125; ++block) {
			for (int i = 0; i < 1920; ++i) {
				const int sample = block * 1920 + i;
				random = random * 1664525U + 1013904223U;
				if (sample % 5 == 0)
					bit = (random & 0x80000000U) ? 0.5f
								     : -0.5f;
				const double phase = 2.0 * 3.141592653589793 *
						     100.0 * sample / 48000.0;
				iq[2U * i] = bit * float(std::cos(phase));
				iq[2U * i + 1U] = bit * float(std::sin(phase));
			}
			sm_decoder_feed_iq(decoder.get(), iq.data(), 1920);
			everLocked =
			    everLocked || sm_decoder_locked(decoder.get());
			uint8_t frame[SM_FRAME_LEN];
			int corrected;
			while (
			    sm_decoder_poll(decoder.get(), frame, &corrected))
				if (corrected >= 0)
					++validFrames;
		}
		require(everLocked && sm_decoder_locked(decoder.get()),
			"Actual loops did not lock");
		require(validFrames == 0,
			"Unexpected valid frame in random-bit fixture");
		sm_decoder_reset_lock(decoder.get());
		require(!sm_decoder_locked(decoder.get()),
			"Input-gap reset failed");
		for (int kind = 0; kind < 2; ++kind) {
			decoder.reset(sm_decoder_new());
			require(bool(decoder), "Decoder allocation failed");
			for (int block = 0; block < 125; ++block) {
				for (int i = 0; i < 1920; ++i) {
					random =
					    random * 1664525U + 1013904223U;
					const float ni =
					    float(random >> 8) / 16777216.0f -
					    0.5f;
					random =
					    random * 1664525U + 1013904223U;
					const float nq =
					    float(random >> 8) / 16777216.0f -
					    0.5f;
					const double phase =
					    2.0 * 3.141592653589793 * 100.0 *
					    (block * 1920 + i) / 48000.0;
					iq[2U * i] =
					    kind ? 0.5f * float(
							      std::cos(phase)) +
						       0.1f * ni
						 : ni;
					iq[2U * i + 1U] =
					    kind ? 0.5f * float(
							      std::sin(phase)) +
						       0.1f * nq
						 : nq;
				}
				sm_decoder_feed_iq(decoder.get(), iq.data(),
						   1920);
				require(
				    !sm_decoder_locked(decoder.get()),
				    "Actual loops falsely locked to noise/CW");
			}
		}
		std::cout << "Demodulator lock, no-frame acquisition, "
			     "rejection and freshness passed\n";
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		return 1;
	}
}
