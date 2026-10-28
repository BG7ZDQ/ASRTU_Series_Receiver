/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Local BPSK carrier/timing lock detector. This observes synchronization;
 * it neither depends on decoded frames nor certifies correct frame data. */
#ifndef ASRTU_SM_LOCK_H
#define ASRTU_SM_LOCK_H

#include <math.h>
#include <stddef.h>
#include <string.h>

#define SM_LOCK_WINDOW 960U /* 100 ms at 9600 symbols/s */
#define SM_LOCK_LAGS 4U

struct sm_lock {
	double i_sum, q_sum, i2, q2, iq, timing_error;
	double i3, i4;
	double lag_product[SM_LOCK_LAGS], lag_x[SM_LOCK_LAGS];
	double lag_y[SM_LOCK_LAGS], previous[SM_LOCK_LAGS];
	size_t count, healthy;
	unsigned int good_windows, bad_windows;
	int locked;
};

static inline void sm_lock_init(struct sm_lock *s)
{
	memset(s, 0, sizeof(*s));
}

static inline void sm_lock_push(struct sm_lock *s, float re, float im,
				float timing_error, int loops_healthy)
{
	unsigned int lag;
	const double x = re;
	if (!isfinite(re) || !isfinite(im) || !isfinite(timing_error)) {
		sm_lock_init(s);
		return;
	}
	s->i_sum += re;
	s->q_sum += im;
	s->i2 += (double)re * re;
	s->q2 += (double)im * im;
	s->iq += (double)re * im;
	s->i3 += x * x * x;
	s->i4 += x * x * x * x;
	for (lag = 0; lag < SM_LOCK_LAGS; ++lag) {
		if (s->count > lag) {
			s->lag_product[lag] += x * s->previous[lag];
			s->lag_x[lag] += x;
			s->lag_y[lag] += s->previous[lag];
		}
	}
	for (lag = SM_LOCK_LAGS - 1; lag > 0; --lag)
		s->previous[lag] = s->previous[lag - 1];
	s->previous[0] = x;
	s->timing_error += timing_error;
	s->healthy += loops_healthy != 0;
	if (++s->count == SM_LOCK_WINDOW) {
		/* Subtract DC: a stationary unmodulated carrier is not BPSK
		 * lock. BPSK's squared constellation must align with the real
		 * axis; circular noise and a rotating constellation fail this
		 * test. */
		const double vi = s->i2 - s->i_sum * s->i_sum / s->count;
		const double vq = s->q2 - s->q_sum * s->q_sum / s->count;
		const double cross = s->iq - s->i_sum * s->q_sum / s->count;
		const double power = vi + vq;
		const double axis = vi - vq;
		const double coherence = s->locked ? 0.15 : 0.25;
		const double phase = s->locked ? 0.8 : 0.4;
		const double timing = s->locked ? 0.25 : 0.15;
		const double mean = s->i_sum / s->count;
		const double m4 = s->i4 - 4.0 * mean * s->i3 +
			6.0 * mean * mean * s->i2 -
			3.0 * s->count * mean * mean * mean * mean;
		/* Axis alignment alone also accepts real microphone audio.
		 * For BPSK plus independent Gaussian I/Q noise, the negative
		 * fourth cumulant is 2*A^4, and (vi-vq)^2 estimates N^2*A^4.
		 * Gaussian real noise has zero cumulant even when Q is absent.
		 * This is a distribution check, not a packet/FEC requirement. */
		const double cumulant = 3.0 * vi * vi - s->count * m4;
		const double shape = s->locked ? 0.5 : 0.75;
		const double correlation = s->locked ? 0.35 : 0.25;
		int independent = 1;
		/* Scrambled telemetry symbols are weakly correlated. Check
		 * several lags: a tone can have zero correlation at lag one.
		 * Pair-specific centering also rejects audio with a DC offset. */
		for (lag = 0; lag < SM_LOCK_LAGS; ++lag) {
			const double pairs = (double)(s->count - lag - 1U);
			const double covariance = s->lag_product[lag] -
				s->lag_x[lag] * s->lag_y[lag] / pairs;
			if (fabs(covariance) >= correlation * vi * pairs / s->count)
				independent = 0;
		}
		const int good = power > 1.0e-12 * s->count &&
				 axis > coherence * power &&
				 fabs(2.0 * cross) < phase * axis &&
				 fabs(s->timing_error) < timing * power &&
				 cumulant > shape * axis * axis && independent &&
				 s->healthy >= SM_LOCK_WINDOW * 99U / 100U;
		if (good) {
			s->bad_windows = 0;
			if (s->good_windows < 2U)
				++s->good_windows;
			if (s->good_windows >= 2U)
				s->locked = 1;
		} else {
			s->good_windows = 0;
			if (s->bad_windows < 2U)
				++s->bad_windows;
			if (s->bad_windows >= 2U)
				s->locked = 0;
		}
		s->i_sum = s->q_sum = s->i2 = s->q2 = s->iq = 0.0;
		s->timing_error = 0.0;
		s->i3 = s->i4 = 0.0;
		memset(s->lag_product, 0, sizeof(s->lag_product));
		memset(s->lag_x, 0, sizeof(s->lag_x));
		memset(s->lag_y, 0, sizeof(s->lag_y));
		s->count = s->healthy = 0;
	}
}

#endif
