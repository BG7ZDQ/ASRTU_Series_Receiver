#include "demod_lock_status.h"
extern "C" {
#include "sm_api.h"
#include "sm_lock.h"
}

#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <random>
#include <algorithm>
#include <stdexcept>
#include <vector>

static void require(bool condition, const char *message)
{
	if (!condition)
		throw std::runtime_error(message);
}

static void window(sm_lock &state, int kind = 0)
{
	std::mt19937 random(91234);
	std::normal_distribution<float> noise(0.0f, 0.3f);
	for (unsigned int i = 0; i < SM_LOCK_WINDOW; ++i) {
		const auto signs = random();
		float re = (signs & 1U) ? 1.0f : -1.0f;
		float im = (signs & 2U) ? 0.2f : -0.2f;
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
			re = ((signs & 1U) ? 0.5f : -0.5f) +
			     ((signs & 2U) ? 0.7f : -0.7f);
			im = (signs & 4U) ? 0.7f : -0.7f;
		}
		if (kind == 8) re = noise(random), im = 0.0f;
		if (kind == 9) re = std::sin(float(i) * 0.2f), im = 0.0f;
		sm_lock_push(&state, re, im, kind == 5 ? 1.0f : 0.0f,
			     kind != 6);
	}
}

// Real-only and duplicated sound-card channels are not circular I/Q noise.
// Exercise the actual LPF/AGC/FLL/Gardner/Costas chain, including clipped
// colored noise and a synthetic voiced harmonic signal (not a recording).
static void rejectMicrophoneAudio()
{
	constexpr double pi = 3.141592653589793;
	const char *names[] = {"real white noise", "duplicated channels",
		"colored noise", "clipped colored noise", "voice-like harmonics",
		"300 Hz tone", "2400 Hz tone", "two tones", "AM tone"};
	for (int mode = 0; mode < 2; ++mode) {
		for (int kind = 0; kind < 9; ++kind) {
			std::unique_ptr<sm_decoder, decltype(&sm_decoder_free)> decoder(
				sm_decoder_new(), sm_decoder_free);
			require(bool(decoder), "Microphone decoder allocation failed");
			std::mt19937 random(91234U + unsigned(kind));
			std::normal_distribution<float> noise(0.0f, 0.2f);
			std::vector<float> iq(3840);
			std::vector<int16_t> audio(1920);
			double colored = 0.0;
			for (int block = 0; block < 750; ++block) {
				for (int i = 0; i < 1920; ++i) {
					const double t = (block * 1920 + i) / 48000.0;
					const float white = noise(random);
					colored = 0.94 * colored + 0.06 * white;
					double sample = white;
					if (kind == 2) sample = colored;
					if (kind == 3) sample = std::clamp(12.0 * colored, -0.4, 0.4);
					if (kind == 4) {
						sample = 0;
						for (int harmonic = 1; harmonic <= 18; ++harmonic)
							sample += std::sin(2 * pi * (137 * harmonic * t +
								9 * std::sin(2 * pi * 0.73 * t))) / harmonic;
						sample *= 0.2 * (0.15 + 0.85 * std::max(0.0, std::sin(2 * pi * 3.1 * t)));
					}
					if (kind == 5) sample = 0.3 * std::sin(2 * pi * 300 * t);
					if (kind == 6) sample = 0.3 * std::sin(2 * pi * 2400 * t);
					if (kind == 7) sample = 0.2 * (std::sin(2 * pi * 1000 * t) + std::sin(2 * pi * 2700 * t));
					if (kind == 8) sample = (0.2 + 0.15 * std::sin(2 * pi * 7 * t)) * std::cos(2 * pi * 2100 * t);
					iq[2 * i] = float(sample);
					iq[2 * i + 1] = kind == 1 ? float(sample) : 0.0f;
					audio[i] = int16_t(std::clamp(sample, -0.99, 0.99) * 32767.0);
				}
				if (mode == 0)
					sm_decoder_feed_iq(decoder.get(), iq.data(), 1920);
				else
					sm_decoder_feed(decoder.get(), audio.data(), 1920);
				if (sm_decoder_locked(decoder.get())) {
					std::cerr << names[kind] << " mode=" << mode
						<< " time=" << (block + 1) * 0.04 << '\n';
					throw std::runtime_error("Microphone audio falsely acquired lock");
				}
			}
		}
	}
}

int main()
{
	try {
		sm_lock state{};
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
		window(state, 8);
		window(state, 8);
		require(!state.locked, "Real microphone noise retained lock");
		for (int kind : {1, 2, 3, 4, 5, 6, 8, 9}) {
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
		for (int i = 0; i < 2; ++i)
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
		// Clean and approximately 0 dB Es/N0 after the 5-sample symbol filter.
		for (int noisy = 0; noisy < 2; ++noisy) {
			decoder.reset(sm_decoder_new());
			require(bool(decoder), "BPSK decoder allocation failed");
			std::mt19937 awgnRandom(67332);
			std::normal_distribution<float> awgn(0.0f, 0.5f * std::sqrt(2.5f));
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
					if (noisy) {
						iq[2U * i] += awgn(awgnRandom);
						iq[2U * i + 1U] += awgn(awgnRandom);
					}
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
			require(std::isfinite(sm_decoder_snr(decoder.get())),
				"Locked decoder without valid frames has no SNR");
			sm_decoder_reset_lock(decoder.get());
			require(!sm_decoder_locked(decoder.get()),
				"Input-gap reset failed");
			require(std::isnan(sm_decoder_snr(decoder.get())),
				"Input-gap reset retained SNR");
		}
		for (int kind = 0; kind < 2; ++kind) {
			decoder.reset(sm_decoder_new());
			require(bool(decoder), "Decoder allocation failed");
			for (int block = 0; block < 750; ++block) {
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
			require(std::isnan(sm_decoder_snr(decoder.get())),
				"Unlocked decoder exposed synchronized SNR");
		}
		rejectMicrophoneAudio();
		std::cout << "Demodulator lock, no-frame acquisition, "
			     "rejection and freshness passed\n";
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		return 1;
	}
}
