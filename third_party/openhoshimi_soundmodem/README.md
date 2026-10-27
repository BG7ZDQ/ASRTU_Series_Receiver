# OpenHoshimi soundmodem decoder core

This directory contains the ASRTU soundmodem demodulator and CCSDS decoder
core adapted from OpenHoshimi. The `openhoshimi-soundmodem` package declares
GPL-3.0-or-later; retain and follow the notices in each source file. The
upstream repository's root GPL text is preserved verbatim as
`OPENHOSHIMI_ROOT_LICENSE` for completeness.

Source: https://github.com/IzumiChino/OpenHoshimi

Credits include BG6HNY / Hyacinth Satellite Team and the upstream HIT LilacSat
soundmodem authors identified in the individual source files.

Local receiver extensions include failed-RS candidate routing, IQ filtering,
and the `sm_decoder_snr()` synchronized-symbol Es/N0 API. The SNR accumulator
observes the existing Gardner/Costas output without changing decoder samples.
The API exposes synchronized SNR only while carrier/timing loops are locked,
independently of RS frames. The receiver separately uses GNU Radio SVR while
unlocked. See
[DSP notes](../../docs/DSP.md) for assumptions and input freshness. These
additions are local, not an upstream-provided SNR interface.

`sm_decoder_locked()` exposes a local carrier/timing lock detector independent
of frame reception. `sm_decoder_reset_lock()` clears acquisition history and SNR after
an input gap. The observer does not alter the DSP loop feedback or soft symbols.
