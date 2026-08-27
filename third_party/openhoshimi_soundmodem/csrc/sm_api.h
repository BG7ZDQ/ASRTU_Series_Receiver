/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * sm_api - self-contained ASRTU-1 (AO-123) 9k6 BPSK CCSDS decoder.
 *
 * Wraps the reentrant smdsp demodulator and the vendored HIT LilacSat
 * FEC chain (dual-phase Viterbi + CCSDS deframer) behind a small C ABI
 * suitable for FFI. The caller feeds int16 audio at SM_RATE (48 kHz mono,
 * 12 kHz IF) and polls decoded 223-byte CCSDS frames.
 *
 * Copyright 2026 Izumi Chino, Mashiro Chen and Hyacinth Satellite Team
 */

#ifndef OPENHOSHIMI_SM_API_H
#define OPENHOSHIMI_SM_API_H

#include <stddef.h>
#include <stdint.h>

/* Payload length of one decoded CCSDS frame (RS(255,223) message). */
#define SM_FRAME_LEN 223

/* Opaque decoder handle. */
struct sm_decoder;

/* Allocate a decoder, or NULL on allocation failure. */
struct sm_decoder *sm_decoder_new(void);

/* Release a decoder created by sm_decoder_new(). NULL is tolerated. */
void sm_decoder_free(struct sm_decoder *dec);

/*
 * Feed @n int16 samples (48 kHz mono, 12 kHz IF). Decoded frames are
 * queued internally; drain them with sm_decoder_poll().
 */
void sm_decoder_feed(struct sm_decoder *dec, const int16_t *samples,
		     size_t n);

/*
 * Feed @n complex baseband samples at 48 kHz as interleaved float pairs
 * (@iq has 2*@n floats: re0, im0, re1, im1, ...). The signal must sit near
 * 0 Hz; this is the lossless IQ entry that keeps the full complex sample.
 */
void sm_decoder_feed_iq(struct sm_decoder *dec, const float *iq, size_t n);

/*
 * Process any buffered partial block at end of stream, so the trailing
 * samples below one processing block are not dropped.
 */
void sm_decoder_flush(struct sm_decoder *dec);

/*
 * Pop one decoded frame. Writes SM_FRAME_LEN bytes to @out and the RS
 * byte-correction count to @corrected. Returns 1 if a frame was written,
 * 0 if the queue is empty.
 */
int sm_decoder_poll(struct sm_decoder *dec, uint8_t *out, int *corrected);

#endif /* OPENHOSHIMI_SM_API_H */
