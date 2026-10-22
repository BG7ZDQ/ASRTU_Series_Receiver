# SDR++ shared-memory bridge (Windows, Linux, macOS)

`asrtu_sink` registers the **ASRTU Receiver** sink. It sends 48 kHz complex
float32 I/Q to the existing ASRTU V1 shared-memory ring and reads TinyDoppler
commands from shared memory. No UDP I/Q or Rigctl connection is used by this
plugin. The Windows launcher selects `--shared-iq-bridge` for SDR++;
`--sdrsharp-iq-bridge` remains a compatible decoder alias.

## Use

1. Build against the headers and import libraries matching your SDR++ binary.
   Copy `asrtu_sink.dll` into that installation's `modules/` directory.
2. In SDR++ Module Manager, add an instance of `asrtu_sink` (for example
   **ASRTU Bridge**). In **Radio**, choose **RAW**, with 48 kHz bandwidth.
3. In **Sinks**, select **ASRTU Receiver** for the Radio stream. Start SDR++.
4. In the ASRTU launcher select the single
   **本地共享内存 RAW 模式 I/Q 桥接** input and start
   reception.
5. Open TinyDoppler and enable **Automatic Doppler tuning** in the sink panel.
   This preference is saved per stream in `asrtu_sink_config.json`. The plugin
   tunes its own Radio/VFO; it does not follow whichever VFO is selected.

I/Q is tapped before audio volume and mute, without 16-bit PCM conversion.
Doppler is polled on the GUI thread even when the sink panel is collapsed.
A disabled, invalid, incomplete, future-dated or older-than-three-seconds
command is ignored. Stop bridge disables both I/Q and automatic tuning.
On module disable/unload or stream stop, ownership and mappings are released.

## Drivers and PlutoSDR URI

The Windows payload uses libiio 0.26 with USB and IP backends, current
libad9361, Airspy/AirspyHF+, LimeSuite and rtl-sdr Blog 1.4.0 libraries. The same
RTL driver is installed into both SDR++ and SDR# and recognizes RTL-SDR Blog V4
/ R828D hardware.

The bundled `plutosdr_source` module adds an **IIO URI** field to SDR++'s
PlutoSDR source menu. Enter either `192.168.1.10` or the complete
`ip:192.168.1.10` URI and press **Run**. A bare IP address or hostname is
automatically normalized to the `ip:` backend. This works with network
Pluto-compatible devices that do not appear in automatic mDNS discovery.
Automatic scanning still runs first and a failed scan no longer prevents the
manual URI from being used. New Pluto devices default to the AD9361
`hybrid` AGC mode, a 2 MHz sample rate and a 2 MHz RF bandwidth; Manual and
the other AGC modes remain selectable. The packaged default SDR++ spectrum
range is -150 to -65 dB.

The installer also carries the signed Analog Devices PlutoSDR-M2K USB driver
and an elevated installer shortcut.

## Competition protection

Both updated SDR++ and SDR# plugins atomically create the same Windows named
lifetime token, `Local\ASRTU_BRIDGE_OWNER_V1`, before touching the I/Q map.
Only the first succeeds. A later instance shows an occupancy message and
cannot publish samples or tune. This applies to multiple SDR++ streams,
multiple SDR++ processes, and SDR++ versus SDR#. Other unrelated SDR++ modules
are unaffected. A refused sink continues draining its input so it cannot stall
the rest of SDR++.

Stop the active bridge, then explicitly retry **Start bridge** in the other
instance (in SDR#, toggle **Enable shared-memory bridge**). The kernel releases
the token when its owning process exits or crashes; no persistent lock file or
timeout-based takeover is used. Paused reception does not permit stealing
ownership from a plugin that still has the bridge enabled.

Upgrade the SDR# plugin as well. The new bridge detects a running legacy
producer before initializing an existing ring, but an old plugin started
later does not implement this protocol and cannot be forced to cooperate.

## Protocol

The I/Q mapping is `Local\ASRTU_IQ_BRIDGE_V1`: a 64-byte header followed by
262144 pairs of float32 values, I then Q. Header offsets: magic 0, version 4,
48 kHz sample rate 8, capacity 16, sample size 20, monotonic write cursor 24,
source sample rate 32, enabled flag 40, performance-counter heartbeat 48,
producer PID 56. A failed contender never changes the header. A handover keeps
the cursor when a decoder retains the mapping; PID and enabled flag are cleared
before releasing ownership.

The control mapping is `Local\ASRTU_DOPPLER_CONTROL_V1` (64 bytes): magic
`0x504f4441` at 0, version 1 at 4, signed 64-bit target Hz at 8, correction Hz
at 16, UTC milliseconds at 24, valid int32 at 32. Offset 36 adds a seqlock
without changing existing V1 field offsets: the writer publishes an odd sequence
before changing fields and an even sequence afterward. Readers accept only
unchanged even sequences. Use the updated TinyDoppler for coherent updates.

## Build

Windows uses named shared memory; Linux/macOS use a per-user mmap file at
`/tmp/asrtu-iq-<uid>.v1`, with an exclusive producer lock. Unix heartbeat values
are monotonic nanoseconds. The decoder opens the same file read-only. Existing
SDR++ Rigctl Server tuning stays on port 4532; the Unix plugin does not add a
second tuning path. Windows keeps its shared-memory Doppler control.

CI builds the plugin for Windows x64, Linux x86_64/arm64 and macOS x86_64/arm64.
See [artifact layout](../../docs/RELEASE_LAYOUT.md). Each binary records the
SDR++ source revision and dependencies. On Unix install the matching VOLK
runtime; on macOS bundled dependent dylibs stay beside the plugin. Windows
plugins target the bundled SDR++ host. A different host ABI requires rebuilding.

Standalone Windows example:

```powershell
cmake -S integrations/sdrpp/asrtu_sink -B integrations/sdrpp/build -A x64 `
  -DSDRPP_CORE_ROOT=C:/src/SDRPlusPlus/core `
  -DSDRPP_CORE_LIBRARY=C:/sdk/lib/sdrpp_core.lib `
  -DSDRPP_DEPENDENCY_INCLUDE_DIR=C:/sdk/include `
  -DSDRPP_VOLK_LIBRARY=C:/sdk/lib/volk.lib
cmake --build integrations/sdrpp/build --config Release
```

Use matching architectures and release/debug runtimes. SDR++ module ABI is
build-dependent; a successful build alone does not prove binary compatibility.
The public sink and tuning APIs are defined in the
[official SDR++ source](https://github.com/AlexandreRouma/SDRPlusPlus).

## Validation

`ASRTU1_SharedBridgeTest` tests float I/Q, wraparound, ownership, rejected-writer
non-interference, handover, legacy-producer detection, cross-process crash
recovery and control freshness/seqlock checks using isolated mapping names.
`ASRTU1_SharedIqSourceTest` exercises the existing receiver.
`tests/run_bridge_plugin_test.ps1` tests the actual SDR# processor and C#/C++
ownership interoperability; it refuses to disturb existing production mappings.
