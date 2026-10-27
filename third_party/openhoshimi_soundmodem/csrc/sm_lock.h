/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Local BPSK carrier/timing lock detector. This observes synchronization;
 * it neither depends on decoded frames nor certifies correct frame data. */
#ifndef ASRTU_SM_LOCK_H
#define ASRTU_SM_LOCK_H

#include <math.h>
#include <stddef.h>
#include <string.h>

#define SM_LOCK_WINDOW 1920U /* 200 ms at 9600 symbols/s */

struct sm_lock {
	double i_sum, q_sum, i2, q2, iq, timing_error;
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
	if (!isfinite(re) || !isfinite(im) || !isfinite(timing_error)) {
		sm_lock_init(s);
		return;
	}
	s->i_sum += re;
	s->q_sum += im;
	s->i2 += (double)re * re;
	s->q2 += (double)im * im;
	s->iq += (double)re * im;
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
		const int good = power > 1.0e-12 * s->count &&
				 axis > coherence * power &&
				 fabs(2.0 * cross) < phase * axis &&
				 fabs(s->timing_error) < timing * power &&
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
		s->count = s->healthy = 0;
	}
}

#endif
