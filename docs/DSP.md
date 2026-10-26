# ASRTU-1 C++/Qt portable demodulator

Native Qt5/GNU Radio 3.10 rewrite of `demod_asrtu1_audio.grc`.

It retains the live input spectrum, waterfall, RSSI bar, BPSK constellation,
loop spectrum, SNR history, loop frequency offset, FEC frame monitoring and
`SYNCED`/`NOSYNC` status. `SYNCED` means that a valid FEC PDU was produced
on either decoder during the previous 1.5 seconds.

The SNR readout is now synchronized BPSK symbol **Es/N0** from the OpenHoshimi
Gardner/Costas output, replacing the primary path's blind SVR probe. For
locked BPSK and equal I/Q noise variances it estimates signal energy as
`E[I²] - E[Q²]` and noise as `2 E[Q²]`, using non-overlapping 1920-symbol
windows (approximately 200 ms). The result is `10 log10(signal/noise)`.
Residual phase/timing error, interference and noncircular noise can bias it;
it is not RF C/N over the receiver bandwidth, nor information-bit Eb/N0.

A successful OpenHoshimi RS frame is required before displaying the estimate.
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
