# Integrated runtime payloads

This directory contains the verified Windows payload used to create the official installer:

- `decoder/` — portable C++/Qt/GNU Radio receiver and runtime libraries
- `proxy/` — telemetry upload proxy and its runtime libraries
- `sdrsharp/` — SDR# telemetry preset host and bundled bridge plugin
- `sdrsharp-api/` — compile-time-only SDR# API references; not installed

These components are included so a clean Windows checkout with Visual Studio 2022 Build Tools and Inno Setup 6 can reproduce the complete installer. They are **not** relicensed under this repository's MIT License. See `THIRD_PARTY.md` and the notices installed with the application.

Do not place operator configuration, callsigns, coordinates, recordings, logs, or other private runtime state in this directory. The packaging script removes `config.cfg` and SDR#'s last-opened recording path before compiling the installer.
