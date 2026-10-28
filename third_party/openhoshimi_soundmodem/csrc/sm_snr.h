/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Local receiver extension: synchronized BPSK symbol Es/N0, not RF C/N.
 * For x = +/-A + nI + j*nQ with equal I/Q noise variance:
 * Es = E[I^2] - E[Q^2], N0 = 2 E[Q^2]. Carrier/timing errors also
 * contribute to the residual; the decoder API gates this on loop lock.
 */
#ifndef ASRTU_SM_SNR_H
#define ASRTU_SM_SNR_H

#include <math.h>
#include <stddef.h>

#define SM_SNR_WINDOW 960U /* 100 ms at 9600 symbols/s */

struct sm_snr {
	double i_power, q_power;
	double db;
	size_t count;
};

static inline void sm_snr_init(struct sm_snr *s)
{
	s->i_power = s->q_power = 0.0;
	s->db = NAN;
	s->count = 0;
}

static inline void sm_snr_push(struct sm_snr *s, float re, float im)
{
	if (!isfinite(re) || !isfinite(im)) {
		sm_snr_init(s);
		return;
	}
	s->i_power += (double)re * re;
	s->q_power += (double)im * im;
	if (++s->count == SM_SNR_WINDOW) {
		const double signal = s->i_power - s->q_power;
		const double noise = 2.0 * s->q_power;
		s->db = signal > 0.0 && noise > 0.0
			    ? 10.0 * log10(signal / noise)
			    : NAN;
		s->i_power = s->q_power = 0.0;
		s->count = 0;
	}
}

static inline double sm_snr_read(const struct sm_snr *s)
{
	return s->db;
}

#endif
