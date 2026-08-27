/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * smdsp - ASRTU-1 (AO-123) 9k6 BPSK demodulator, extracted from the
 * official HIT LilacSat soundmodem CLI and ported from C-style C++ to
 * reentrant C99.
 *
 * The signal chain is faithful to the upstream processSignal() path:
 *
 *	int16 48 kHz mono (12 kHz IF)
 *		-> DDC (mix 12 kHz -> complex baseband)
 *		-> FIR low-pass (10 kHz / 48 kHz)
 *		-> complex AGC
 *		-> band-edge FLL
 *		-> Gardner symbol timing (Farrow interpolation, sps = 5)
 *		-> Costas carrier loop
 *		-> real part -> soft symbols
 *
 * All upstream file-scope state is collected into struct sm_demod so a
 * caller may run independent decoders concurrently.
 *
 * Copyright 2015 WEI Mingchuan, BG2BHC and HIT (upstream algorithm)
 * Copyright 2026 Izumi Chino, Mashiro Chen and Hyacinth Satellite Team
 * (C99 reentrant port)
 */

#ifndef OPENHOSHIMI_SMDSP_H
#define OPENHOSHIMI_SMDSP_H

#include <stddef.h>
#include <stdint.h>

/* Audio input contract, matching the upstream soundmodem. */
#define SM_RATE			48000	/* input sample rate (Hz) */
#define SM_IF_FREQ		12000	/* intermediate frequency (Hz) */
#define SM_SPS			5	/* samples per symbol (9600 baud) */
#define SM_BLOCK_FRAMES		1920	/* 40 ms processing block */

/* Sizing constants lifted from the upstream implementation. */
#define SM_AGC_WINDOW		512
#define SM_LPF_NTAPS		43
#define SM_FLL_MAX_TAPS		100
#define SM_GARDNER_HIST		32

/* A single-precision complex sample. */
struct sm_cf {
	float re;
	float im;
};

/*
 * Gardner timing-error-detector state. Field names and semantics match
 * the upstream gardner_ted_t; the interpolation history is a power-of-two
 * ring so the modulo reduces to a mask.
 */
struct sm_gardner {
	float phase;
	float freq;
	float integ;
	struct sm_cf hist[SM_GARDNER_HIST];
	uint8_t hist_len;
};

/* Band-edge FLL state, including the two spun band-edge filters. */
struct sm_fll {
	float phase;
	float freq;
	float alpha;
	float beta;
	float max_freq;
	float min_freq;
	size_t ntaps;
	struct sm_cf taps_lower[SM_FLL_MAX_TAPS];
	struct sm_cf taps_upper[SM_FLL_MAX_TAPS];
	struct sm_cf up_buffer[SM_FLL_MAX_TAPS];
	struct sm_cf lo_buffer[SM_FLL_MAX_TAPS];
	size_t up_idx;
	size_t lo_idx;
};

/* Second-order Costas loop state. */
struct sm_costas {
	float phase;
	float freq;
	float alpha;
	float beta;
	float max_freq;
	float min_freq;
};

/*
 * Full demodulator state. One instance carries the whole sample-to-symbol
 * chain; sm_demod_run() consumes int16 samples and emits soft real symbols.
 */
struct sm_demod {
	double ddc_phase;

	struct sm_cf agc_buffer[SM_AGC_WINDOW];
	size_t agc_index;
	float agc_gain;

	/*
	 * Monotonic-decreasing deque of (position, envelope) giving the
	 * sliding-window maximum envelope over the last SM_AGC_WINDOW samples
	 * in O(1) amortised, replacing the upstream O(window) rescan.
	 */
	size_t agc_pos;
	size_t agc_dq_head;
	size_t agc_dq_tail;
	size_t agc_dq_pos[SM_AGC_WINDOW + 1];
	float agc_dq_env[SM_AGC_WINDOW + 1];

	struct sm_cf lpf_history[SM_LPF_NTAPS - 1];

	struct sm_fll fll;
	struct sm_gardner gardner;
	struct sm_costas costas;

	float delay_history;
};

/*
 * Initialise a demodulator. rolloff/fll_bw/costas_bw mirror the upstream
 * tuning (0.35, 0.068, 0.068); pass those defaults unless experimenting.
 */
void sm_demod_init(struct sm_demod *d, float rolloff, float fll_bw,
		   float costas_bw);

/*
 * Demodulate up to SM_BLOCK_FRAMES int16 samples into soft real symbols.
 *
 * @in:		int16 samples at SM_RATE (12 kHz IF, mono)
 * @n_in:	number of input samples, must be <= SM_BLOCK_FRAMES
 * @sym0:	receives symbols on the on-time phase
 * @sym1:	receives symbols on the 1-sample-delayed phase
 * @cap:	capacity of each output buffer, must be >= SM_BLOCK_FRAMES / SM_SPS
 *
 * Returns the number of symbols written to each of @sym0 and @sym1.
 */
size_t sm_demod_run(struct sm_demod *d, const int16_t *in, size_t n_in,
		    float *sym0, float *sym1, size_t cap);

/*
 * Demodulate up to SM_BLOCK_FRAMES complex baseband samples at SM_RATE.
 *
 * The IQ entry point (upstream processIQSignal): the signal must already sit
 * near 0 Hz, so it skips the DDC and LPF and keeps the full complex sample
 * instead of the real 12 kHz-IF projection. Same symbol outputs as
 * sm_demod_run().
 */
size_t sm_demod_run_iq(struct sm_demod *d, const struct sm_cf *in, size_t n_in,
		       float *sym0, float *sym1, size_t cap);

#endif /* OPENHOSHIMI_SMDSP_H */
