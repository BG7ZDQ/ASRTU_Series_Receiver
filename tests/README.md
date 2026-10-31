# Tests and benchmarks

`benchmark_main.cpp` is the command-line DSP regression and performance entry point built as `ASRTU1_Benchmark` on Windows.

Large satellite recordings, generated logs, screenshots, build outputs, callsigns, and station coordinates are intentionally not stored in this repository. Regression recordings should be supplied locally and referenced by path when running the benchmark.

Maintenance scripts that generate translations, icons, and release archives belong in `tools/` and are version controlled. One-off investigation output belongs in ignored `outputs/` or `work/` directories.

`ini_settings` verifies explicit file storage even when Qt's default format is
native: UTF-8 station names, SDR arrays/paths, binary window geometry, distinct
launcher/decoder files, proxy station import and blocked-path errors. Tests use
an isolated `ASRTU_CONFIG_DIR`; no application preference registry is touched.

`grid_locator` checks Maidenhead encoding and decoding against the operator
reference squares (OL63/OM89/IO91 at 4, 6 and 8 characters), lowercase and
separator tolerance, pole/antimeridian clamping, cell-centre round trips and
rejection of malformed locators.

`launcher_grid` runs the real launcher offscreen and types a locator into the
station form: the decimal fields must receive the centre of the square, a
decimal edit must rewrite the locator, invalid input must flag the field
without changing the station, and entering 4, 6 or 8 characters must keep the
operator's precision.

`spectrum_card` runs with Qt's offscreen platform and isolated INI settings.
It checks both spectrum modules' floating controls, reclaimed space on collapse,
fixed collapsed height on resize, restoration and persisted visibility.
It also checks balanced columns at narrow widths and preservation of manually
adjusted horizontal and vertical splits when the window resizes.

`snr_estimation_and_scale` checks synchronized BPSK Es/N0 against known
signal/noise powers from -5 to 20 dB and across input gains, sustained estimates
without frames, silence/nonfinite input, and integer grid scales including zero.
It also verifies audio-time history at 1x/30x replay, the 60-second cutoff,
irregular processing progress, invalid readings, pause/EOF and restart.
The 1.5.5 issue #27 regression also replayed SatNOGS JAMX observation 14907288:
329 unique 223-byte frames, primary 310 / OpenHoshimi 303, with exactly the
same payload hash set as before the SNR change. Recording files stay local.

Windows bridge regression checks:

```powershell
ctest --test-dir build-vs-fix -C Release -R '^(shared_bridge|shared_iq_source)$' --output-on-failure
./tests/run_bridge_plugin_test.ps1 -SdrSharpApiRoot ./packaging/payload/sdrsharp-api -NativeBridgeTest ./build-vs-fix/Release/ASRTU1_SharedBridgeTest.exe
```

The native bridge test uses private mapping names and tests ownership, crash
recovery, ring wraparound, and Doppler freshness. The managed test verifies
SDR# contention and interoperability with the native ownership token. It exits
without changing anything if production I/Q shared memory already exists.

`demod_loop_lock` checks lock hysteresis, invalid-input reset, noise/CW/rotation
and unsettled-timing rejection, and the 500 ms wall-clock freshness gate. It
also feeds five seconds of random BPSK through the actual decoder with a
100 Hz frequency offset: the loops must lock and expose finite SNR without any
valid CCSDS frame. Input-gap reset must clear the estimator; unlocked input
must not expose synchronized SNR or falsely light the lock indicator.

`frame_monitor` also checks that the callback used for the title-bar packet
total fires only once for eight duplicate valid decoder results, and excludes
short payloads and failed local recovery candidates.

`snr_stream_sampling` feeds identical symbols/IQ through tiny and large work
batches and checks all 100 ms readings, timestamps and historical algorithm
selection. It compares SVR against the GNU Radio kernel, checks intermediate
signal changes, delayed producers, pause/EOF and bounded buffering. This
catches fast replay losing points between GUI refreshes.

The lock regression includes 30 seconds each of noise and noisy CW, as well
as acquisition/release debounce. It additionally checks nine microphone-like
fixtures for 30 seconds each in both zero-IF I/Q and real 12 kHz IF paths:
real white noise, duplicated channels, colored noise, clipped colored noise,
synthetic voiced harmonics, 300/2400 Hz tones, two tones and AM.
Real noise must also release an existing lock. Random, unframed BPSK remains
a positive control; the lock detector never requires valid FEC frames. The response audit uses local JAMX sections
with known switch times; see DSP notes for observed latency and its limits.

Windows installer layout regression (after building the installer):

```powershell
python ci/check_windows_startup.py packaging/inno/stage --installer-stage --output work/installed-startup
```

This materializes the generated Inno core file manifest into a temporary
layout and verifies the shared `bin` runtime, ordinary files without hard
links, actual loaded CRT paths, decoder readiness and Qt screenshots
(including TinyDoppler) with developer PATH/settings removed.
Flat-runtime smoke tests alone cannot catch DLLs removed during installer
staging. The previous layout fails both app-local presence and loader-path
checks. No real installation, registry change or observation upload occurs.
