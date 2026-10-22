# macOS port status

Issue #25 asks to merge the macOS branch and add SDR++ support. The current
`main` branch has not merged a macOS implementation. The portable DSP core is
C++17/Qt 5/GNU Radio based, but the full application still needs platform
work before a release build can be claimed:

- replace the Windows-only WinMM capture and named shared-memory bridge with
  Core Audio or a portable input path;
- build and validate GNU Radio 3.10 plus `gr-lilacsat` on the target macOS
  version;
- replace Windows-only SDR# and Inno Setup packaging;
- verify Qt GUI, audio capture, WAV playback, FEC, SSDV output, and the
  TinyDoppler catalog/tracking UI on macOS;
- decide whether SDR++ is supported through its macOS module ABI or through a
  local network I/Q bridge.

The repository therefore documents macOS as a porting branch and keeps it out
of the release workflow for now. The SDR++ integration under
`integrations/sdrpp/` is source-only and does not imply macOS binary
compatibility.
