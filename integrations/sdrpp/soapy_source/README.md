# SDR++ SoapySDR module

This is the upstream SDR++ 1.3.0 SoapySDR source module, retained so the
portable Windows build does not have to remove the SoapySDR configuration when
its module is missing.

The bundled launcher sets `SOAPY_SDR_ROOT` to the SDR++ directory. Soapy
support plugins are stored under `lib/SoapySDR/modules0.8`, keeping the bundled
drivers separate from a system or conda installation.

```powershell
cmake -S integrations/sdrpp/soapy_source `
  -B integrations/sdrpp/soapy_source/build -A x64 `
  -DSDRPP_SOURCE=C:/src/SDRPlusPlus `
  -DSDRPP_SDK=C:/sdk `
  -DRUNTIME_ROOT=C:/ProgramData/radioconda/Library
cmake --build integrations/sdrpp/soapy_source/build --config Release
```

The source remains under the SDR++ GPL-3.0-or-later license in `COPYING`.
