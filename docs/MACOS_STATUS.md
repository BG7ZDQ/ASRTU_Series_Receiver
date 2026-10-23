# macOS support in 1.5.5 (development)

The CoreAudio device enumeration and application bundler build on
[PR #23 by doublehan07](https://github.com/BG7ZDQ/ASRTU_Series_Receiver/pull/23).
The launcher lists input devices by name; GNU Radio's CoreAudio source opens
the selected device. Mono 12 kHz IF, stereo I/Q and WAV/OGG playback use the
same decoder pipeline. SDR++ has a native POSIX shared-memory I/Q bridge and reads Doppler tuning from
the shared-memory control record. Optional physical radio tuning is an
independently configured Hamlib endpoint (default 4534); SDR tuning never uses
rigctl.

CI builds Intel and Apple Silicon independently with Qt 5, GNU Radio 3.10 and
the pinned gr-lilacsat revision. `ci/build_macos_app.py` assembles the launcher,
decoder, TinyDoppler, both uploaders, audio converter, Qt plugins and dylibs in
one `.app`; unresolved dependencies and signing errors fail packaging.

```sh
bash ci/install_linux_oot.sh "$PWD/work/oot-install" "$PWD/work/oot"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$PWD/work/oot-install;$CONDA_PREFIX" \
  -DASRTU_BUILD_BENCHMARK=OFF -DBUILD_TESTING=ON
cmake --build build --parallel 3
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
python3 ci/build_macos_app.py --build-dir build --output dist/macos \
  --dep-prefix "$CONDA_PREFIX" --dep-prefix "$PWD/work/oot-install"
```

The app requests microphone permission for live receiver audio. Recordings and
configuration go to the user's writable application-data locations, not inside
the signed bundle. SDR# remains Windows-only. Uploaders are selected by the
user in the launcher.

This is not a released or notarized macOS product yet. CI success establishes
build/test coverage; microphone permission, USB audio devices, SDR++ module
loading and actual CAT operation still require hardware validation on a Mac.
