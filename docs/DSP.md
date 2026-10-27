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

The detector uses 1920-symbol (about 200 ms) windows after Gardner/Costas:
DC-removed I/Q second moments must show a real-axis BPSK constellation,
the normalized mean Gardner timing error must be small, and FLL, Costas
and timing-frequency estimates must stay away from their limits. Acquire
thresholds are axis/power > 0.25, |2 IQ covariance|/axis < 0.4 and
|summed timing error|/power < 0.15; hold thresholds are 0.15, 0.8 and 0.25.
At least 99% of the window's observed loop-limit checks must pass.
Three consecutive good windows acquire lock, two bad windows release it.
These are a practical lock estimate, not a guarantee of correct data or an
absolute sensitivity specification. Circular noise, silence, stationary CW,
rotating symbols and unsettled timing are covered by regression tests.

The GUI also requires fresh decoder input (less than 500 ms old). Stop or
EOF clears the lamp; after an input gap the detector must acquire again.
`LOCKED` can appear before any valid frame. It does not certify successful
CCSDS synchronization or FEC: use decoded frames/logs for that.

The SNR readout is now synchronized BPSK symbol **Es/N0** from the OpenHoshimi
Gardner/Costas output, replacing the primary path's blind SVR probe. For
locked BPSK and equal I/Q noise variances it estimates signal energy as
`E[I²] - E[Q²]` and noise as `2 E[Q²]`, using non-overlapping 1920-symbol
windows (approximately 200 ms). The result is `10 log10(signal/noise)`.
Residual phase/timing error, interference and noncircular noise can bias it;
it is not RF C/N over the receiver bandwidth, nor information-bit Eb/N0.

A locked demodulator and a successful OpenHoshimi RS frame are required before
displaying the estimate. A green lamp with `SNR: --` is therefore possible.
After two seconds of symbols without another valid RS frame, silence,
nonfinite input, or undefined signal/noise powers, the readout is `--` and
the history has a gap. No-input detection also inserts gaps. A benchmark with
`--no-openhoshimi` has no SNR estimate (JSON null); it does not fall back to SVR.
The benchmark field is now `final_symbol_esn0_db`.

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
