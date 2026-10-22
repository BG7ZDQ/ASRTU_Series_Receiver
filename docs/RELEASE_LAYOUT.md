# 1.5.5 development artifacts

The version comes from `CMakeLists.txt`. These changes target **1.5.5**, not
1.5.4. Branch builds upload CI artifacts; no release is created without a `v*` tag.
Do not create a release tag while this version is under validation.

| Target | Artifact |
| --- | --- |
| Windows x64 | Inno Setup installer |
| Linux x86_64 | AppImage |
| macOS x86_64 / arm64 | Separate application bundle ZIPs |
| SDR bridges | `ASRTU_Plugins-1.5.5.zip` |

Windows no longer produces a portable release ZIP. The tested runtime directory
is an internal input to the installer. Legacy script/directory names containing
`portable` remain compatible with local build automation.

The plugin archive contains two top-level directories:

```text
SDR#/
  Windows-x86/SDRSharp.AstroSeriesBridge.dll
SDR++/
  Windows-x64/asrtu_sink.dll
  Linux-x86_64/asrtu_sink.so
  Linux-arm64/asrtu_sink.so
  macOS-x86_64/asrtu_sink.dylib
  macOS-arm64/asrtu_sink.dylib
```

Every SDR++ directory also records the pinned header revision, architecture,
dependencies and binary hash in `build-info.json`. The Windows build uses the
bundled host DLL's exports. SDR++ does not guarantee a stable module ABI; use
matching host builds or rebuild against your installation. Packaging fails if
any architecture is missing. macOS packages are ad-hoc signed, not notarized.
