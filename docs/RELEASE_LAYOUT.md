# 1.5.5 development artifacts

The version comes from `CMakeLists.txt`. These changes target **1.5.5**, not
1.5.4. Branch builds upload CI artifacts; no release is created without a `v*` tag.
Do not create a release tag while this version is under validation.

| Target | Artifact |
| --- | --- |
| Windows x86_64 portable | `ASRTU_Series_Receiver-1.5.5-x86_64-WinPortable.zip` |
| Windows x86_64 installer | `ASRTU_Series_Receiver-1.5.5-x86_64-WinInstaller.zip` |
| Linux x86_64 | `ASRTU_Series_Receiver-1.5.5-x86_64-LinuxAppImage.zip` |
| macOS Intel | `ASRTU_Series_Receiver-1.5.5-x86_64-macOS.zip` |
| macOS Apple Silicon | `ASRTU_Series_Receiver-1.5.5-arm64-macOS.zip` |
| SDR bridges | `SDR_Software_Plugins-1.5.5-AllPlatform.zip` |

Windows produces both installer and portable ZIPs from the same staged runtime.
The portable ZIP contains a single versioned folder with `bin/`, `proxy/`,
SDR payloads and license notices. Extract the whole folder to a writable location
and run `bin/ASRTU1_Launcher.exe`. No suite installation is required; hardware
may still need drivers. Launcher and decoder settings travel in `config/*.ini`;
see [file-based settings](SETTINGS.md) for the remaining files and other platforms.
CI extracts the actual ZIP and verifies startup and application-local CRT loading
before uploading it. Both Windows ZIPs are attached to tagged releases.

The Windows installer ZIP contains the matching `*-x86_64-WinInstaller.exe`; extract it
before running Setup. Each macOS ZIP contains `ASRTU Series Receiver.app`.
The Linux AppImage and per-platform plugin build outputs are intermediate CI
artifacts; only the ZIPs listed above are attached to a future tagged release.
Final GitHub Actions artifact names match the ZIP basename, without `.zip`.

Tagged releases include a SHA256 table in the release notes for all six ZIP
assets (Windows installer/portable, Linux, both macOS architectures, and plugins).
The hash and upload steps share the same file list; missing or ambiguous assets
fail the job. After successful upload, CI replaces only its marked SHA256 section,
preserving manually written notes and avoiding duplicate sections on reruns.
Local installer builds do not generate separate checksum files.

The Linux installer ZIP has exactly this shape:

```text
ASRTU_Series_Receiver-<version>-x86_64-LinuxAppImage.zip
  setup.sh
  ASRTU_Series_Receiver-<version>-x86_64-LinuxAppImage/
    ASRTU_Series_Receiver-<version>-x86_64-LinuxAppImage.AppImage
    plugins/asrtu_sink.so
    sdrpp/sdrpp_ubuntu_jammy_amd64.deb
```

`setup.sh` asks for 简体中文 / English / 日本語, installs the AppImage into the
user's home directory, creates the menu/desktop entries, and optionally installs
SDR++ plus the matching Linux bridge plugin. The SDR++ option is skipped when
the Debian package is absent or cannot be installed; ASRTU itself remains
usable.

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
