# SDR++ PlutoSDR manual URI module

This is the upstream SDR++ 1.3.0 PlutoSDR source module with one addition: an
**IIO URI** field in the source menu. It accepts full libiio URIs such as
`ip:192.168.1.10`, which is required for network Pluto-compatible devices that
are not advertised through Windows mDNS discovery.

The module keeps automatic IIO scanning and adds the configured manual URI as a
second device entry. The selection is persisted in
`plutosdr_source_config.json`.

```powershell
cmake -S integrations/sdrpp/plutosdr_source `
  -B integrations/sdrpp/plutosdr_source/build -A x64 `
  -DSDRPP_SOURCE=C:/src/SDRPlusPlus `
  -DSDRPP_SDK=C:/sdk `
  -DRUNTIME_ROOT=C:/ProgramData/radioconda/Library
cmake --build integrations/sdrpp/plutosdr_source/build --config Release
```

Use SDR++ headers and `sdrpp_core.lib` from the same release as the target
installation. The bundled binary is built for SDR++ 1.3.0 x64.
