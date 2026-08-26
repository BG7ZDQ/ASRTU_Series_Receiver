# ASRTU Series Satellite Receiver 1.5

[中文](README.md) · **English** · [日本語](README_JA.md)

A C++/Qt suite for receiving, demodulating, forwarding telemetry, and automatically correcting Doppler shift for ASRTU-series satellites. It includes a receiver/decoder, desktop launcher, telemetry-proxy wrapper, and an SDR# local RAW I/Q bridge plugin.

Author: **BG7ZDQ**

## Features

- Shared-memory RAW I/Q, stereo zero-IF I/Q, mono 12 kHz real IF, and recorded-file inputs
- BPSK demodulation, convolutional/Viterbi decoding, and FEC frame output
- Live waterfall, spectrum, constellation, SNR, RSSI, loop frequency offset, and synchronization state
- Optional automatic WAV recording with complete frames, SVR output, and per-session logs
- Multiple TLE sources, satellite selection, frequency presets, SGP4 tracking, and automatic SDR# Doppler control
- Telemetry upload proxy with ground-station callsign and location configuration
- Automatic Chinese, Japanese, or English UI selection from the operating-system language
- Optional direct submission of live decoded FEC frames to SatNOGS; recording playback is never submitted

## Usage

### 1. Installation and initial setup

1. Run `ASRTU_Series_Receiver_Setup.exe` and choose whether to install the bundled SDR# telemetry preset.
2. Open **ASRTU Series Satellite Launcher** from the desktop.
3. Enter your callsign, longitude, latitude, and altitude. These coordinates are used for satellite tracking and Doppler calculation and are stored locally.
4. Select an input appropriate for your receiving chain. The sound-card selector is shown only for sound-card modes.

### 2. Input modes

| Input | Intended use | Signal format |
| --- | --- | --- |
| Local shared-memory RAW I/Q bridge | Bundled SDR# with the ASRTU plugin | Complex zero-IF RAW I/Q from SDR# |
| Stereo zero-IF RAW I/Q input | SDR software feeding a virtual sound card | I and Q in the left and right channels, centered at 0 Hz |
| Mono real 12 kHz radio IF input | Radio, receiver, or recorder real-IF output | Desired signal centered at +12 kHz |

For sound-card input, select the actual capture device. Stop the current reception before changing or reconnecting a sound card, then select the device again and restart.

### 3. SDR# RAW I/Q bridge

1. Click **Open SDR# Telemetry Preset**.
2. Select the receiver in SDR#, tune the satellite downlink, and start reception.
3. Enable the local RAW I/Q bridge in the ASRTU plugin. Enable automatic Doppler there when frequency correction is required.
4. In the launcher, select **Local Shared-Memory RAW I/Q Bridge** and click **Start Receiver**.

The decoder remains `NOSYNC` until SDR# supplies samples. A stale SNR value after SDR# is paused or closed must not be interpreted as an active signal.

### 4. Sound-card input

Stereo I/Q requires a device carrying two-channel zero-IF RAW data; ordinary speaker audio is not I/Q. For radio real IF, use the mono mode and place the signal near 12 kHz in the audio spectrum. The real-input plots display 12 kHz as 0 Hz so that frequency error can be read directly.

After choosing the mode and sound card, click **Start Receiver**. Enable **Automatically save this reception recording** first if the raw input should be retained.

### 5. Confirming reception and decoding

- The input waterfall should show a persistent target signal rather than only broadband noise.
- BPSK constellation points should cluster on the two sides of the I axis.
- Rising SNR and a stabilizing loop offset indicate that the carrier and timing loops are entering a useful operating region.
- `SYNCED` means usable frames have been received recently; `NOSYNC` means synchronization has not been obtained or the frame timeout has elapsed.
- Use `FEC frame`, the complete hexadecimal PDU, and SVR entries in the log to confirm actual decoding. SNR alone is insufficient.

### 6. Telemetry upload

The two upload paths are optional and independent of local decoding:

- **Start Upload Proxy** opens the existing WebSocket proxy. Select the destination satellite in the dialog shown before launch.
- **Also upload to SatNOGS** submits each newly decoded live FEC frame directly to the SatNOGS telemetry API using the selected satellite, callsign, and station coordinates.

Keep the computer clock, callsign, and coordinates accurate. SatNOGS submission is disabled during file playback so historical recordings are not reported as current observations.

### 7. Tracking and automatic Doppler

Click **Satellite / Doppler Tracking**. The application downloads and merges all configured TLE sources, then allows selection of a satellite and frequency preset or a custom downlink frequency. Verify azimuth, elevation, range, and TLE epoch before enabling automatic Doppler in the SDR# plugin.

Doppler correction only adjusts the receiver frequency; it does not prove successful decoding. Use `SYNCED`, FEC frames, and logs as the final criteria.

### 8. Recording and playback

- Automatic recording stores WAV and log files under `ASRTU1_Records/<timestamp>/`.
- **Open Recordings and Logs** opens that directory.
- **Play Recording File** accepts supported recordings. Mono files are treated as 12 kHz real IF; stereo files are treated as zero-IF I/Q.
- **Quick Play Recording File** provides a shorter path for selecting a recording and running regression checks. It does not switch to live sound-card input.

### 9. Troubleshooting

- **Always NOSYNC:** Check the pass, tuned frequency and Doppler, input mode, and whether the waterfall actually contains a signal.
- **Mirrored or aliased spectrum:** Confirm that mono real IF was not selected as stereo I/Q and that the I/Q channels are not swapped.
- **No samples after startup:** For shared memory, ensure SDR# is running and its bridge is enabled. For sound-card mode, select an input device that is currently present.
- **SNR but no frames:** Inspect the constellation, loop offset, and FEC log. A carrier does not guarantee frame synchronization or successful correction.
- **No upload wanted:** Leave SatNOGS unchecked and do not start the upload proxy.

## Repository layout

- `asrtu-qt/` — C++/Qt launcher, decoder, tracker, and benchmark helper
- `sdrsharp-iq-bridge/` — legacy-compatible SDR# RAW I/Q and Doppler bridge plugin
- `asrtu-suite/` — Windows staging and Inno Setup packaging scripts
- `tools/` — translation and asset-maintenance scripts
- `docs/` — architecture, build, translation, and release documentation

## Building

Quick Windows build:

```powershell
.\asrtu-qt\build_release.ps1
```

See [docs/BUILDING.md](docs/BUILDING.md) for the complete environment, current Linux/macOS status, and dependencies. See [docs/PACKAGING.md](docs/PACKAGING.md) for installer packaging.

## License and third-party software

BG7ZDQ publishes this project under the [MIT License](LICENSE). Third-party components remain under their respective upstream licenses; see [THIRD_PARTY.md](THIRD_PARTY.md). The MIT license does not replace, modify, or extend any third-party grant.

GNU Radio 3.x, `gr-lilacsat`, and `gr-hyacinthsat` are GPL-family dependencies. MIT-licensed source may interoperate with them, but binary distributions containing or linking them must also satisfy the applicable GPL source, license, and attribution obligations. SDR# and the upload proxy are outside this project's MIT grant.

This project is not officially affiliated with or endorsed by SDR#, GNU Radio, or the authors and operators of supported satellites unless separately stated in writing by the relevant rights holder.
