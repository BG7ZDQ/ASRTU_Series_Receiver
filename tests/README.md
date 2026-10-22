# Tests and benchmarks

`benchmark_main.cpp` is the command-line DSP regression and performance entry point built as `ASRTU1_Benchmark` on Windows.

Large satellite recordings, generated logs, screenshots, build outputs, callsigns, and station coordinates are intentionally not stored in this repository. Regression recordings should be supplied locally and referenced by path when running the benchmark.

Maintenance scripts that generate translations, icons, and release archives belong in `tools/` and are version controlled. One-off investigation output belongs in ignored `outputs/` or `work/` directories.

Windows bridge regression checks:

```powershell
ctest --test-dir build-vs-fix -C Release -R '^(shared_bridge|shared_iq_source)$' --output-on-failure
./tests/run_bridge_plugin_test.ps1 -SdrSharpApiRoot ./packaging/payload/sdrsharp-api -NativeBridgeTest ./build-vs-fix/Release/ASRTU1_SharedBridgeTest.exe
```

The native bridge test uses private mapping names and tests ownership, crash
recovery, ring wraparound, and Doppler freshness. The managed test verifies
SDR# contention and interoperability with the native ownership token. It exits
without changing anything if production I/Q shared memory already exists.
