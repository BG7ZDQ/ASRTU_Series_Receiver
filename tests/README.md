# Tests and benchmarks

`benchmark_main.cpp` is the command-line DSP regression and performance entry point built as `ASRTU1_Benchmark` on Windows.

Large satellite recordings, generated logs, screenshots, build outputs, callsigns, and station coordinates are intentionally not stored in this repository. Regression recordings should be supplied locally and referenced by path when running the benchmark.

Maintenance scripts that generate translations, icons, and release archives belong in `tools/` and are version controlled. One-off investigation output belongs in ignored `outputs/` or `work/` directories.

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
batches and checks all 200 ms readings, timestamps and historical algorithm
selection. It compares SVR against the GNU Radio kernel, checks intermediate
signal changes, delayed producers, pause/EOF and bounded buffering. This
catches fast replay losing points between GUI refreshes.
