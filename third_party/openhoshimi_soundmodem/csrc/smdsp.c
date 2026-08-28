// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * smdsp - ASRTU-1 9k6 BPSK demodulator, ported from the HIT LilacSat
 * soundmodem CLI processSignal() path to reentrant C99. See smdsp.h.
 *
 * Copyright 2015 WEI Mingchuan, BG2BHC and HIT (upstream algorithm)
 * Copyright 2026 Izumi Chino, Mashiro Chen and Hyacinth Satellite Team
 * (C99 reentrant port)
 */

#include "smdsp.h"

#include <math.h>
#include <string.h>

#define SM_PI			3.14159265358979323846

/* DDC input gain applied after down-conversion (upstream multiply_const). */
#define SM_DDC_GAIN		10.0f

/*
 * 43-tap 10 kHz / 48 kHz low-pass, copied verbatim from the upstream
 * taps_10k_48k so the matched-filter response is bit-for-bit identical.
 */
static const double sm_lpf_taps[SM_LPF_NTAPS] = {
	 0.0008558771223761141,   0.0011713290587067604,  -0.0004347003123257309,
	-0.0022162303794175386,  -0.0007736302795819938,   0.003487595357000828,
	 0.003792312229052186,   -0.0035177047830075026,  -0.008780770935118198,
	 7.095331955714482e-18,   0.014107000082731247,    0.00912476982921362,
	-0.016035277396440506,   -0.024353675544261932,    0.009045750834047794,
	 0.043783221393823624,    0.014427214860916138,   -0.0632917582988739,
	-0.07147771865129471,     0.07780873030424118,     0.30532506108283997,
	 0.4159052073955536,      0.30532506108283997,     0.07780873030424118,
	-0.07147771865129471,    -0.0632917582988739,      0.014427214860916138,
	 0.043783221393823624,    0.009045750834047794,   -0.024353675544261932,
	-0.016035277396440506,    0.00912476982921362,     0.014107000082731247,
	 7.095331955714482e-18,  -0.008780770935118198,   -0.0035177047830075026,
	 0.003792312229052186,    0.003487595357000828,   -0.0007736302795819938,
	-0.0022162303794175386,  -0.0004347003123257309,   0.0011713290587067604,
	 0.0008558771223761141
};

static inline struct sm_cf sm_cf_mul(struct sm_cf a, struct sm_cf b)
{
	struct sm_cf o;

	o.re = a.re * b.re - a.im * b.im;
	o.im = a.re * b.im + a.im * b.re;
	return o;
}

static float sm_sinc(float x)
{
	float arg = (float)SM_PI * x;

	return x == 0.0f ? 1.0f : sinf(arg) / arg;
}

/*
 * Build the two band-edge filters by spinning a baseband sinc-sum up and
 * down in frequency. Faithful to upstream fll_init().
 */
static void sm_fll_init(struct sm_fll *f, float sps, float ro, size_t fsize,
			float bw)
{
	float bb_taps[SM_FLL_MAX_TAPS];
	float damping, denom, power, invpower, inv_twice_sps;
	float half_sps_inv;
	float mf;
	int m;
	long n;
	size_t i;

	if (fsize > SM_FLL_MAX_TAPS)
		fsize = SM_FLL_MAX_TAPS;

	f->phase = 0.0f;
	f->freq = 0.0f;
	f->max_freq = (float)(2.0 * SM_PI) * (2.0f / sps);
	f->min_freq = -f->max_freq;

	damping = sqrtf(2.0f) / 2.0f;
	denom = 1.0f + 2.0f * damping * bw + bw * bw;
	f->alpha = (4.0f * damping * bw) / denom;
	f->beta = (4.0f * bw * bw) / denom;

	mf = rintf((float)fsize / sps);
	m = (int)mf;
	power = 0.0f;
	half_sps_inv = 2.0f / sps;
	for (i = 0; i < fsize; i++) {
		float k = -(float)m + (float)i * half_sps_inv;
		float position = ro * k;
		float tap = sm_sinc(position - 0.5f) + sm_sinc(position + 0.5f);

		power += tap * tap;
		bb_taps[i] = tap;
	}

	n = ((long)fsize - 1) / 2;
	invpower = 1.0f / power;
	inv_twice_sps = 0.5f / sps;
	for (i = 0; i < fsize; i++) {
		float tap = bb_taps[i] * invpower;
		float k = (float)((long)i - n) * inv_twice_sps;
		float wl = -2.0f * (float)SM_PI * (1.0f + ro) * k;
		float wu = 2.0f * (float)SM_PI * (1.0f + ro) * k;
		size_t index = fsize - i - 1;

		f->taps_lower[index].re = tap * cosf(wl);
		f->taps_lower[index].im = tap * sinf(wl);
		f->taps_upper[index].re = tap * cosf(wu);
		f->taps_upper[index].im = tap * sinf(wu);
	}

	f->ntaps = fsize;
	f->up_idx = 0;
	f->lo_idx = 0;
	memset(f->up_buffer, 0, sizeof(f->up_buffer));
	memset(f->lo_buffer, 0, sizeof(f->lo_buffer));
}

/* One band-edge FIR tap-and-accumulate over a circular history. */
static struct sm_cf sm_band_filter(struct sm_cf *buffer, size_t *idx,
				   const struct sm_cf *taps, size_t ntaps,
				   struct sm_cf input)
{
	struct sm_cf sum = { 0.0f, 0.0f };
	size_t buf_idx;
	size_t i;

	buffer[*idx] = input;
	buf_idx = *idx;
	for (i = 0; i < ntaps; i++) {
		sum.re += buffer[buf_idx].re * taps[i].re -
			  buffer[buf_idx].im * taps[i].im;
		sum.im += buffer[buf_idx].re * taps[i].im +
			  buffer[buf_idx].im * taps[i].re;
		buf_idx = (buf_idx == 0) ? ntaps - 1 : buf_idx - 1;
	}

	*idx += 1;
	if (*idx >= ntaps)
		*idx = 0;
	return sum;
}

void sm_demod_init(struct sm_demod *d, float rolloff, float fll_bw,
		   float costas_bw)
{
	float damping, denom;

	memset(d, 0, sizeof(*d));

	d->ddc_phase = 0.0;
	d->agc_index = 0;
	d->agc_gain = 1.0f;

	sm_fll_init(&d->fll, (float)SM_SPS, rolloff, SM_FLL_MAX_TAPS, fll_bw);

	d->gardner.phase = 0.0f;
	d->gardner.freq = 1.0f / (float)SM_SPS;
	d->gardner.integ = 0.0f;
	d->gardner.hist_len = 0;

	d->costas.phase = 0.0f;
	d->costas.freq = 0.0f;
	d->costas.max_freq = 1.0f;
	d->costas.min_freq = -1.0f;
	damping = sqrtf(2.0f) / 2.0f;
	denom = 1.0f + 2.0f * damping * costas_bw + costas_bw * costas_bw;
	d->costas.alpha = (4.0f * damping * costas_bw) / denom;
	d->costas.beta = (4.0f * costas_bw * costas_bw) / denom;

	d->delay_history = 0.0f;
}

/* DDC + fixed gain: int16 12 kHz IF -> complex baseband. */
static void sm_ddc(struct sm_demod *d, const int16_t *in, struct sm_cf *out,
		   size_t n)
{
	size_t i;

	for (i = 0; i < n; i++) {
		double x = (double)in[i] / 32768.0;

		out[i].re = (float)(x * cos(d->ddc_phase)) * SM_DDC_GAIN;
		out[i].im = (float)(-x * sin(d->ddc_phase)) * SM_DDC_GAIN;
		d->ddc_phase += 2.0 * SM_PI * SM_IF_FREQ / SM_RATE;
		if (d->ddc_phase > 2.0 * SM_PI)
			d->ddc_phase -= 2.0 * SM_PI;
	}
}

/* Complex FIR low-pass with cross-block history. */
static void sm_lpf(struct sm_demod *d, const struct sm_cf *in,
		   struct sm_cf *out, size_t n)
{
	size_t k, idx;
	long j;

	for (idx = 0; idx < n; idx++) {
		float acc_re = 0.0f;
		float acc_im = 0.0f;

		for (k = 0; k < SM_LPF_NTAPS; k++) {
			float r, i;
			double h = sm_lpf_taps[k];

			j = (long)idx - (long)k;
			if (j >= 0) {
				r = in[j].re;
				i = in[j].im;
			} else {
				r = d->lpf_history[SM_LPF_NTAPS - 1 + j].re;
				i = d->lpf_history[SM_LPF_NTAPS - 1 + j].im;
			}
			acc_re += r * (float)h;
			acc_im += i * (float)h;
		}
		out[idx].re = acc_re;
		out[idx].im = acc_im;
	}

	if (n >= SM_LPF_NTAPS - 1) {
		memcpy(d->lpf_history, &in[n - (SM_LPF_NTAPS - 1)],
		       (SM_LPF_NTAPS - 1) * sizeof(*in));
	} else {
		memmove(d->lpf_history, d->lpf_history + n,
			(SM_LPF_NTAPS - 1 - n) * sizeof(*in));
		memcpy(d->lpf_history + (SM_LPF_NTAPS - 1 - n), in,
		       n * sizeof(*in));
	}
}

static inline float sm_envelope(struct sm_cf x)
{
	float r_abs = fabsf(x.re);
	float i_abs = fabsf(x.im);

	if (r_abs > i_abs)
		return r_abs + 0.4f * i_abs;
	return i_abs + 0.4f * r_abs;
}

/*
 * Sliding-window complex AGC normalising to unit envelope. Bit-identical to
 * the upstream per-sample window rescan, but the max envelope is maintained by
 * a monotonic-decreasing deque so each sample costs O(1) amortised instead of
 * O(SM_AGC_WINDOW). Zero-initialised buffer slots have envelope 0, which never
 * beats a real peak, so they need no deque entry.
 */
static void sm_agc(struct sm_demod *d, const struct sm_cf *in,
		   struct sm_cf *out, size_t n)
{
	const size_t cap = SM_AGC_WINDOW + 1;
	size_t i;

	for (i = 0; i < n; i++) {
		float e = sm_envelope(in[i]);
		size_t t = d->agc_pos;
		float max;

		d->agc_buffer[d->agc_index] = in[i];
		d->agc_index = (d->agc_index + 1) % SM_AGC_WINDOW;

		while (d->agc_dq_head != d->agc_dq_tail &&
		       d->agc_dq_pos[d->agc_dq_head] + SM_AGC_WINDOW <= t)
			d->agc_dq_head = (d->agc_dq_head + 1) % cap;

		/* Drop back entries no larger than the incoming envelope. */
		while (d->agc_dq_head != d->agc_dq_tail) {
			size_t back = (d->agc_dq_tail + cap - 1) % cap;

			if (d->agc_dq_env[back] <= e)
				d->agc_dq_tail = back;
			else
				break;
		}
		d->agc_dq_pos[d->agc_dq_tail] = t;
		d->agc_dq_env[d->agc_dq_tail] = e;
		d->agc_dq_tail = (d->agc_dq_tail + 1) % cap;

		max = d->agc_dq_env[d->agc_dq_head];
		if (max < 0.0000001f)
			max = 0.0000001f;
		d->agc_gain = 1.0f / max;
		out[i].re = d->agc_buffer[d->agc_index].re * d->agc_gain;
		out[i].im = d->agc_buffer[d->agc_index].im * d->agc_gain;

		d->agc_pos = t + 1;
	}
}

/* Band-edge FLL: derotate and drive the loop from the edge-power error. */
static void sm_fll_work(struct sm_demod *d, const struct sm_cf *in,
			struct sm_cf *out, size_t n)
{
	struct sm_fll *f = &d->fll;
	size_t i;

	for (i = 0; i < n; i++) {
		struct sm_cf nco = { cosf(f->phase), sinf(f->phase) };
		struct sm_cf o_l, o_u;
		float err;

		out[i] = sm_cf_mul(in[i], nco);
		o_l = sm_band_filter(f->up_buffer, &f->up_idx, f->taps_upper,
				     f->ntaps, out[i]);
		o_u = sm_band_filter(f->lo_buffer, &f->lo_idx, f->taps_lower,
				     f->ntaps, out[i]);
		err = (o_l.im * o_l.im + o_l.re * o_l.re) -
		      (o_u.im * o_u.im + o_u.re * o_u.re);

		f->freq += f->beta * err;
		f->phase += f->freq + f->alpha * err;
		while (f->phase > 2.0f * (float)SM_PI)
			f->phase -= 2.0f * (float)SM_PI;
		while (f->phase < -2.0f * (float)SM_PI)
			f->phase += 2.0f * (float)SM_PI;
		if (f->freq > f->max_freq)
			f->freq = f->max_freq;
		else if (f->freq < f->min_freq)
			f->freq = f->min_freq;
	}
}

static inline struct sm_cf sm_farrow(struct sm_cf xm1, struct sm_cf x0,
				     struct sm_cf x1, struct sm_cf x2, float mu)
{
	struct sm_cf c1, c2, y;

	c1.re = 0.5f * (x2.re - x1.re - x0.re + xm1.re);
	c1.im = 0.5f * (x2.im - x1.im - x0.im + xm1.im);
	c2.re = 1.5f * x1.re - 0.5f * (x2.re + x0.re + xm1.re);
	c2.im = 1.5f * x1.im - 0.5f * (x2.im + x0.im + xm1.im);
	y.re = (c1.re * mu + c2.re) * mu + x0.re;
	y.im = (c1.im * mu + c2.im) * mu + x0.im;
	return y;
}

static inline struct sm_cf sm_hist_rel(const struct sm_gardner *g, int rel)
{
	int latest = (g->hist_len + SM_GARDNER_HIST - 1) % SM_GARDNER_HIST;
	int pos = latest + rel;

	while (pos < 0)
		pos += SM_GARDNER_HIST;
	while (pos >= SM_GARDNER_HIST)
		pos -= SM_GARDNER_HIST;
	return g->hist[pos];
}

static inline struct sm_cf sm_interp_at(const struct sm_gardner *g, float t_rel)
{
	float kf = floorf(t_rel);
	int k = (int)kf;
	float mu = t_rel - (float)k;

	return sm_farrow(sm_hist_rel(g, k - 1), sm_hist_rel(g, k),
			 sm_hist_rel(g, k + 1), sm_hist_rel(g, k + 2), mu);
}

/*
 * Gardner symbol timing with Farrow interpolation and a leaky integrator.
 * Emits one complex symbol per recovered symbol instant. Faithful to
 * upstream gardner_process_block() (sps = 5, delay D = 3).
 */
static size_t sm_gardner(struct sm_demod *d, const struct sm_cf *in,
			 struct sm_cf *out, size_t n, size_t cap)
{
	struct sm_gardner *g = &d->gardner;
	const float f0 = 1.0f / (float)SM_SPS;
	const float half = 0.5f * (float)SM_SPS;
	const float kp = 2e-4f;
	const float ki = 5e-6f;
	const float df = 0.01f * f0;
	const float dlyf = ceilf(half);
	const int dly = (int)dlyf;
	size_t out_cnt = 0;
	size_t i;

	for (i = 0; i < n; i++) {
		float mu, err, freq;
		float t_mid, t_early, t_late;
		struct sm_cf y_mid, y_early, y_late;

		g->hist[g->hist_len] = in[i];
		g->hist_len = (uint8_t)((g->hist_len + 1) % SM_GARDNER_HIST);

		g->phase += g->freq;
		if (g->phase < 1.0f)
			continue;
		mu = g->phase - 1.0f;
		g->phase -= 1.0f;

		t_mid = -(float)dly - mu;
		t_early = t_mid - half;
		t_late = t_mid + half;
		y_mid = sm_interp_at(g, t_mid);
		y_early = sm_interp_at(g, t_early);
		y_late = sm_interp_at(g, t_late);

		err = (y_early.re - y_late.re) * y_mid.re +
		      (y_early.im - y_late.im) * y_mid.im;
		g->integ = 0.999f * g->integ + ki * err;
		freq = f0 + g->integ + kp * err;
		if (freq > f0 + df)
			freq = f0 + df;
		if (freq < f0 - df)
			freq = f0 - df;
		g->freq = freq;

		if (out_cnt < cap)
			out[out_cnt++] = y_mid;
	}
	return out_cnt;
}

/* Costas carrier loop over recovered symbols. */
static void sm_costas(struct sm_demod *d, struct sm_cf *sym, size_t n)
{
	struct sm_costas *c = &d->costas;
	size_t i;

	for (i = 0; i < n; i++) {
		struct sm_cf nco = { cosf(c->phase), -sinf(c->phase) };
		float err;

		sym[i] = sm_cf_mul(sym[i], nco);
		err = sym[i].im * sym[i].re;
		if (err > 1.0f)
			err = 1.0f;
		else if (err < -1.0f)
			err = -1.0f;

		c->freq += c->beta * err;
		c->phase += c->freq + c->alpha * err;
		while (c->phase > 2.0f * (float)SM_PI)
			c->phase -= 2.0f * (float)SM_PI;
		while (c->phase < -2.0f * (float)SM_PI)
			c->phase += 2.0f * (float)SM_PI;
		if (c->freq > c->max_freq)
			c->freq = c->max_freq;
		else if (c->freq < c->min_freq)
			c->freq = c->min_freq;
	}
}

/*
 * Shared back half of both entry points: complex baseband in @bb ->
 * AGC -> FLL -> Gardner -> Costas -> real soft symbols on the on-time
 * (@sym0) and 1-symbol-delayed (@sym1) phases.
 */
static size_t sm_demod_symbols(struct sm_demod *d, const struct sm_cf *bb,
			       size_t n, float *sym0, float *sym1, size_t cap)
{
	struct sm_cf work_a[SM_BLOCK_FRAMES];
	struct sm_cf work_b[SM_BLOCK_FRAMES];
	struct sm_cf syms[SM_BLOCK_FRAMES];
	size_t n_sym;
	size_t i;

	sm_agc(d, bb, work_a, n);
	sm_fll_work(d, work_a, work_b, n);
	n_sym = sm_gardner(d, work_b, syms, n, SM_BLOCK_FRAMES);
	sm_costas(d, syms, n_sym);

	if (n_sym > cap)
		n_sym = cap;
	for (i = 0; i < n_sym; i++) {
		sym0[i] = syms[i].re;
		sym1[i] = d->delay_history;
		d->delay_history = syms[i].re;
	}
	return n_sym;
}

size_t sm_demod_run(struct sm_demod *d, const int16_t *in, size_t n_in,
		    float *sym0, float *sym1, size_t cap)
{
	struct sm_cf work_a[SM_BLOCK_FRAMES];
	struct sm_cf work_b[SM_BLOCK_FRAMES];

	if (n_in > SM_BLOCK_FRAMES)
		n_in = SM_BLOCK_FRAMES;

	sm_ddc(d, in, work_a, n_in);
	sm_lpf(d, work_a, work_b, n_in);
	return sm_demod_symbols(d, work_b, n_in, sym0, sym1, cap);
}

size_t sm_demod_run_iq(struct sm_demod *d, const struct sm_cf *in, size_t n_in,
		       float *sym0, float *sym1, size_t cap)
{
	struct sm_cf work_b[SM_BLOCK_FRAMES];

	if (n_in > SM_BLOCK_FRAMES)
		n_in = SM_BLOCK_FRAMES;

	/*
	 * Skip only the DDC (the caller already delivers baseband); keep the
	 * matched low-pass, since our IQ is not pre-filtered the way upstream's
	 * ZMQ source was, and its band-limiting is worth ~dB at the slicer.
	 */
	sm_lpf(d, in, work_b, n_in);
	return sm_demod_symbols(d, work_b, n_in, sym0, sym1, cap);
}
