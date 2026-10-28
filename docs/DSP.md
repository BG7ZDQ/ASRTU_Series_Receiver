# ASRTU-1 C++/Qt portable demodulator

Native Qt5/GNU Radio 3.10 rewrite of `demod_asrtu1_audio.grc`.

It retains the live input spectrum, waterfall, RSSI bar, BPSK constellation,
loop spectrum, SNR history, loop frequency offset, FEC frame monitoring and
`LOCKED`/`UNLOCKED` status. The lamp now observes the OpenHoshimi
carrier/timing loops independently of FEC frames. The primary GNU Radio
constellation and loop-offset plots remain diagnostics of the primary path;
they do not drive this lamp.

The title bar refreshes the session packet total on the existing 100 ms status
timer. It counts valid 223-byte payloads after the shared two-second duplicate
suppression, excluding failed FEC/SSDV recovery candidates. The atomic counter
belongs to the window, so input restarts/device changes preserve it; reopening
the window resets it. It measures reception, not upload acknowledgements.

The detector uses 960-symbol (about 100 ms) windows after Gardner/Costas:
DC-removed I/Q second moments must show a real-axis BPSK constellation,
the normalized mean Gardner timing error must be small, and FLL, Costas
and timing-frequency estimates must stay away from their limits. Acquire
thresholds are axis/power > 0.25, |2 IQ covariance|/axis < 0.4 and
|summed timing error|/power < 0.15; hold thresholds are 0.15, 0.8 and 0.25.
At least 99% of the window's observed loop-limit checks must pass.
Two consecutive good windows acquire lock, two bad windows release it
(about 200 ms of confirmation). This excludes the time needed for the actual
carrier/timing loops to converge: the indicator does not move historical
colors forward or claim lock while the constellation is still rotating.
These are a practical lock estimate, not a guarantee of correct data or an
absolute sensitivity specification. Circular noise, silence, stationary CW,
rotating symbols and unsettled timing are covered by regression tests.

The GUI also requires fresh decoder input (less than 500 ms old). Stop or
EOF clears the lamp; after an input gap the detector must acquire again.
`LOCKED` can appear before any valid frame. It does not certify successful
CCSDS synchronization or FEC: use decoded frames/logs for that.

The single status row shows SNR, loop frequency offset (`Loop df`) and lock state.
`SNR (Es/N0)` is synchronized BPSK symbol **Es/N0** from the OpenHoshimi
Gardner/Costas output. For
locked BPSK and equal I/Q noise variances it estimates signal energy as
`E[I²] - E[Q²]` and noise as `2 E[Q²]`, using non-overlapping 960-symbol
windows (approximately 100 ms). The result is `10 log10(signal/noise)`.
Residual phase/timing error, interference and noncircular noise can bias it;
it is not RF C/N over the receiver bandwidth, nor information-bit Eb/N0.

Neither estimate depends on valid CCSDS/RS packets. While unlocked, the UI
uses the original 1.5.4 GNU Radio SVR estimator on the primary equalized/
sanitized symbol stream, labelled `SNR (SVR)` (signal-to-variation ratio).
The streaming sink uses the same `mpsk_snr_est_svr` kernel, smoothing alpha
0.001 and two-symbol history as the original probe. It captures results at
fixed audio intervals rather than querying the probe on the GUI timer.
See [GNU Radio's SVR implementation](https://github.com/gnuradio/gnuradio/blob/main/gr-digital/lib/mpsk_snr_est.cc).
Unsettled timing, noise-only input and interference can bias this blind result;
a finite value does not certify BPSK reception or loop lock. Like 1.5.4,
invalid results retain the last valid value and its algorithm label, and do
not advance history. Before the first valid result the label shows `--`.
The status row also follows 1.5.4: SNR and `Loop df` are adjacent on the left,
with the lock indicator on the right and the original 10-point bold font.

After 500 ms without fresh input, the GUI freezes the last SNR label and
history rather than clearing or advancing them. The lock lamp still turns
red. Stop/EOF also freezes the display. Resuming samples resets acquisition
and the estimator, then updates resume. Frozen values are historical, not
new measurements. The plot spans the last 60 seconds of processed audio,
using the decoder's consumed sample count at 48 kHz rather than counting
100 ms GUI ticks. Fast replay advances the axis with actual audio progress
(e.g. 30x replay covers 60 audio seconds in about two wall-clock seconds),
even if processing cannot sustain the requested speed. No new samples means
no new point, including at EOF. Invalid SNR keeps the last valid display;
the next valid point uses its true audio timestamp.

Both DSP threads capture one reading per 100 ms of audio (4800 decoder input
samples or 960 primary-path symbols), splitting scheduler batches at these
boundaries. This matches the synchronized estimator's nominal 100 ms window.
The GUI drains all accumulated readings on its 100 ms timer; at 30x replay a
refresh can consume about 30 readings instead of discarding all but the last.
The nominal 60-second plot therefore retains about 600 intervals at any speed;
invalid readings are still omitted. Each interval uses its historical loop
lock to choose Es/N0 or SVR. The plot draws Es/N0 segments in green and SVR
segments in a muted red; adjacent segments share their endpoint so an algorithm
switch is a seamless color change, not a break in the curve. Displayed and
plotted SNR is clamped to a floor of -25 dB (values below -25 are shown as
-25); raw benchmark diagnostics remain unclamped. Primary symbol timestamps use the nominal 9600
baud rate; filter delay and recovered clock variation can slightly offset the
two paths. The joined buffer holds at most 1200 intervals (120 audio seconds),
keeping memory bounded if the UI stalls. Already consumed intervals cannot
be reintroduced by a late producer. Final queued readings are drained before
the input-freshness check. No interpolation creates intermediate measurements.
A benchmark with
`--no-openhoshimi` has no synchronized SNR (JSON null). The benchmark field
`final_symbol_esn0_db` contains synchronized Es/N0 only; `final_svr_snr_db`
reports the separate primary-path SVR result. `snr_samples` contains the
remaining bounded batch of timestamped readings at completion.

The 60-second history always includes a labelled 0 dB line, with integer
1/2/5-family grid steps (minimum 5 dB) and an initial -5 to 20 dB range.
The range expands to cover the data, and only contracts when the candidate
range is at most half the displayed range, preventing small fluctuations
from continually rescaling the plot.

The spectrum and waterfall display the normalized output of the same causal
AGC that feeds the demodulator. This applies equally to real 12 kHz IF and
complex zero-IF inputs. The AGC-output plots are limited to a 24 kHz span
(`-12` to `+12` kHz) while the demodulator continues to run at 48 kHz. RSSI
remains measured before AGC so it continues to represent input level rather
than normalized demodulator level.

The application is a Windows GUI-subsystem executable, so no black console
window appears. Runtime and FEC messages are appended to `ASRTU1_Demod.log`
next to the executable.

The GNU Radio spectrum, waterfall, constellation and loop plots retain their
normal zoom controls. Right-click anywhere inside one of these plots to restore
its original axes. The lightweight Qt RSSI meter uses a smoothed adaptive range:
large changes expand the scale immediately and stable readings contract it
gradually.

The Windows shared-memory source forwards SDR#'s short post-decimation
callbacks immediately without waiting inside the GNU Radio scheduler, which
keeps the demodulator and the Qt sinks running at a steady cadence. The sinks
accumulate samples through their own update interval, and the bridge keeps its
120 ms maximum-latency bound.

## Build

The development environment currently targets:

- Visual Studio 2022 x64
- Qt 5 from `C:\ProgramData\radioconda`
- GNU Radio 3.10 plus `gr-lilacsat`; live audio and WAV input are implemented locally so capture behavior is versioned with this application

Configure with `CMAKE_PREFIX_PATH`, `CMAKE_INCLUDE_PATH` and
`CMAKE_LIBRARY_PATH` pointing at `C:\ProgramData\radioconda\Library`, then
build the Release configuration. Run `package_portable.ps1` to copy only the
transitive runtime DLLs and the Qt Windows platform plugin.

For this machine, `build_release.ps1` performs configure, Release build and
portable dependency collection in one command. It also works around launchers
that inject duplicate `PATH`/`Path` environment keys.

A response audit spliced real JAMX sections into strong-signal intervals at
5-15 s and 20-30 s. Reducing measurement/confirmation windows from 200 to
100 ms changed acquisitions from 6.6/22.6 s to 6.6/22.4 s. One loss-of-lock
transition advanced from 15.4 to 15.2 s. These are recording-specific results,
not a guaranteed acquisition time. Diagnostic constellation moments showed
that actual carrier convergence, rather than GUI repainting, dominated the
remaining delay. Decoder loop bandwidths and thresholds are unchanged.
