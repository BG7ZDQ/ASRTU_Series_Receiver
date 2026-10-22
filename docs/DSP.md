# ASRTU-1 C++/Qt portable demodulator

Native Qt5/GNU Radio 3.10 rewrite of `demod_asrtu1_audio.grc`.

It retains the live input spectrum, waterfall, RSSI bar, BPSK constellation,
loop spectrum, SNR history, loop frequency offset, FEC frame monitoring and
`SYNCED`/`NOSYNC` status. SNR sampling starts as soon as the flowgraph starts;
it does not wait for the first decoded frame. `SYNCED` means that a valid FEC
PDU was produced during the previous 1.5 seconds.

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
