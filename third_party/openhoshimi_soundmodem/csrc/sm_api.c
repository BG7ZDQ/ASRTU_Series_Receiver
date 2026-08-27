// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * sm_api - self-contained ASRTU-1 9k6 BPSK CCSDS decoder. See sm_api.h.
 *
 * The pipeline after the smdsp demodulator mirrors the upstream soundmodem
 * processSignal() tail: two symbol phases (on-time and 1-symbol delayed)
 * each run an independent K=7 Viterbi decoder followed by a CCSDS deframer
 * (ASM sync, NRZ-M differential, descramble, RS(255,223)).
 *
 * Copyright 2026 Izumi Chino, Mashiro Chen and Hyacinth Satellite Team
 */

#include "sm_api.h"

#include "ccsds.h"
#include "smdsp.h"
#include "viterbi27.h"

#include <stdlib.h>
#include <string.h>

#define SM_CCSDS_ASM		0x1ACFFC1DUL
#define SM_FRAME_QUEUE		512
#define SM_SYM_BATCH		16	/* soft symbols per Viterbi byte */
#define SM_SYM_CAP		512	/* max symbols emitted per block */

struct sm_frame {
	uint8_t data[SM_FRAME_LEN];
	int corrected;
};

/* One demodulation phase: its own Viterbi and CCSDS deframer state. */
struct sm_phase {
	v27 vi;
	Ccsds cc;
	float carry[SM_SYM_BATCH];
	size_t carry_n;
};

struct sm_decoder {
	struct sm_demod demod;
	struct sm_phase phase[2];

	int16_t block[SM_BLOCK_FRAMES];
	size_t block_n;

	struct sm_cf iq_block[SM_BLOCK_FRAMES];
	size_t iq_block_n;

	struct sm_frame queue[SM_FRAME_QUEUE];
	size_t q_head;
	size_t q_tail;
	size_t q_count;
};

/* CCSDS sync hook: enqueue successfully RS-decoded frames. */
static void sm_frame_hook(uint8_t *buf, uint16_t len, int16_t byte_corr,
			  void *obj_ptr)
{
	struct sm_decoder *dec = (struct sm_decoder *)obj_ptr;
	struct sm_frame *f;
	size_t n;

	if (byte_corr < 0)			/* RS failure */
		return;
	if (dec->q_count >= SM_FRAME_QUEUE)	/* queue full, drop */
		return;

	f = &dec->queue[dec->q_tail];
	n = len < SM_FRAME_LEN ? len : SM_FRAME_LEN;
	memset(f->data, 0, sizeof(f->data));
	memcpy(f->data, buf, n);
	f->corrected = byte_corr;
	dec->q_tail = (dec->q_tail + 1) % SM_FRAME_QUEUE;
	dec->q_count++;
}

struct sm_decoder *sm_decoder_new(void)
{
	struct sm_decoder *dec;
	int i;

	dec = (struct sm_decoder *)calloc(1, sizeof(*dec));
	if (!dec)
		return NULL;

	sm_demod_init(&dec->demod, 0.35f, 0.01f, 0.068f);
	for (i = 0; i < 2; i++) {
		vitfilt27_init(&dec->phase[i].vi);
		ccsds_init(&dec->phase[i].cc, SM_CCSDS_ASM, SM_FRAME_LEN, dec,
			   sm_frame_hook);
		dec->phase[i].cc.cfg_using_m = 1;
		dec->phase[i].cc.cfg_using_convolutional_code = 0;
		dec->phase[i].carry_n = 0;
	}
	return dec;
}

void sm_decoder_free(struct sm_decoder *dec)
{
	free(dec);
}

/* Push one phase's soft symbols through Viterbi -> unpack -> CCSDS. */
static void sm_decode_phase(struct sm_phase *ph, const float *syms, size_t n)
{
	float buf[SM_SYM_BATCH + SM_SYM_CAP];
	uint8_t bits[SM_SYM_CAP];
	size_t total, groups, nbits, off, g, k;

	memcpy(buf, ph->carry, ph->carry_n * sizeof(*buf));
	memcpy(buf + ph->carry_n, syms, n * sizeof(*buf));
	total = ph->carry_n + n;

	groups = total / SM_SYM_BATCH;
	nbits = 0;
	for (g = 0; g < groups; g++) {
		unsigned char sy[SM_SYM_BATCH];
		unsigned char byte = 0;

		for (k = 0; k < SM_SYM_BATCH; k++) {
			float v = buf[g * SM_SYM_BATCH + k] * 127.5f + 127.5f;

			if (v < 0.0f)
				v = 0.0f;
			if (v > 255.0f)
				v = 255.0f;
			sy[k] = (unsigned char)v;
		}
		vitfilt27_decode(&ph->vi, sy, &byte, SM_SYM_BATCH);
		for (k = 0; k < 8; k++)
			bits[nbits++] = (uint8_t)((byte >> (7 - k)) & 1);
	}

	if (nbits > 0) {
		ccsds_rx_proc(&ph->cc, bits, (unsigned int)nbits);
		ccsds_pull(&ph->cc);
	}

	off = groups * SM_SYM_BATCH;
	ph->carry_n = total - off;
	memmove(ph->carry, buf + off, ph->carry_n * sizeof(*buf));
}

/* Run both symbol phases through Viterbi + CCSDS. */
static void sm_dispatch(struct sm_decoder *dec, const float *sym0,
			const float *sym1, size_t nsym)
{
	sm_decode_phase(&dec->phase[0], sym0, nsym);
	sm_decode_phase(&dec->phase[1], sym1, nsym);
}

static void sm_process_block(struct sm_decoder *dec, const int16_t *s, size_t n)
{
	float sym0[SM_SYM_CAP];
	float sym1[SM_SYM_CAP];
	size_t nsym;

	nsym = sm_demod_run(&dec->demod, s, n, sym0, sym1, SM_SYM_CAP);
	sm_dispatch(dec, sym0, sym1, nsym);
}

static void sm_process_block_iq(struct sm_decoder *dec, const struct sm_cf *bb,
				size_t n)
{
	float sym0[SM_SYM_CAP];
	float sym1[SM_SYM_CAP];
	size_t nsym;

	nsym = sm_demod_run_iq(&dec->demod, bb, n, sym0, sym1, SM_SYM_CAP);
	sm_dispatch(dec, sym0, sym1, nsym);
}

void sm_decoder_feed(struct sm_decoder *dec, const int16_t *samples, size_t n)
{
	size_t off = 0;

	while (off < n) {
		size_t space = SM_BLOCK_FRAMES - dec->block_n;
		size_t take = n - off < space ? n - off : space;

		memcpy(dec->block + dec->block_n, samples + off,
		       take * sizeof(*samples));
		dec->block_n += take;
		off += take;
		if (dec->block_n == SM_BLOCK_FRAMES) {
			sm_process_block(dec, dec->block, SM_BLOCK_FRAMES);
			dec->block_n = 0;
		}
	}
}

void sm_decoder_feed_iq(struct sm_decoder *dec, const float *iq, size_t n)
{
	size_t off = 0;

	while (off < n) {
		size_t space = SM_BLOCK_FRAMES - dec->iq_block_n;
		size_t take = n - off < space ? n - off : space;
		size_t k;

		for (k = 0; k < take; k++) {
			dec->iq_block[dec->iq_block_n + k].re = iq[2 * (off + k)];
			dec->iq_block[dec->iq_block_n + k].im = iq[2 * (off + k) + 1];
		}
		dec->iq_block_n += take;
		off += take;
		if (dec->iq_block_n == SM_BLOCK_FRAMES) {
			sm_process_block_iq(dec, dec->iq_block, SM_BLOCK_FRAMES);
			dec->iq_block_n = 0;
		}
	}
}

void sm_decoder_flush(struct sm_decoder *dec)
{
	if (dec->block_n > 0) {
		sm_process_block(dec, dec->block, dec->block_n);
		dec->block_n = 0;
	}
	if (dec->iq_block_n > 0) {
		sm_process_block_iq(dec, dec->iq_block, dec->iq_block_n);
		dec->iq_block_n = 0;
	}
}

int sm_decoder_poll(struct sm_decoder *dec, uint8_t *out, int *corrected)
{
	struct sm_frame *f;

	if (dec->q_count == 0)
		return 0;

	f = &dec->queue[dec->q_head];
	memcpy(out, f->data, SM_FRAME_LEN);
	if (corrected)
		*corrected = f->corrected;
	dec->q_head = (dec->q_head + 1) % SM_FRAME_QUEUE;
	dec->q_count--;
	return 1;
}
