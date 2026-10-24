#!/usr/bin/env python3
"""Exercise packaged Windows binaries without developer DLLs or user settings."""

import argparse
import ctypes
import os
from pathlib import Path
import subprocess
import tempfile
import wave


def run(executable, arguments, output, environment):
    startup = subprocess.STARTUPINFO()
    startup.dwFlags = subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    print(f"Starting {executable.name}", flush=True)
    with (output / f"{executable.stem}.stdout.txt").open("w") as stdout, (
        output / f"{executable.stem}.stderr.txt"
    ).open("w") as stderr:
        with subprocess.Popen(
            [str(executable), *map(str, arguments)], cwd=output, env=environment,
            stdout=stdout, stderr=stderr, startupinfo=startup,
        ) as process:
            try:
                code = process.wait(timeout=30)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
                raise RuntimeError(f"{executable.name}: startup timed out") from None
    if code != 0:
        raise RuntimeError(f"{executable.name}: exit status 0x{code & 0xffffffff:08X}")


def check(runtime, output):
    # Child processes inherit this mode: loader failures must not wait in a dialog.
    ctypes.windll.kernel32.SetErrorMode(0x8003)
    environment = {
        key: value for key, value in os.environ.items()
        if key.upper() not in {"PATH", "TEMP", "TMP", "LOCALAPPDATA", "APPDATA"}
        and not key.upper().startswith(("QT_", "CONDA", "PYTHON", "GR_"))
    }
    windows = Path(os.environ["SystemRoot"])
    environment["PATH"] = f"{windows / 'System32'};{windows}"
    for variable, folder in (("TEMP", "temp"), ("TMP", "temp"),
                             ("LOCALAPPDATA", "local"), ("APPDATA", "roaming")):
        directory = output / folder
        directory.mkdir(exist_ok=True)
        environment[variable] = str(directory)

    wav = output / "silence.wav"
    with wave.open(str(wav), "wb") as stream:
        stream.setparams((2, 2, 48000, 0, "NONE", "not compressed"))
        stream.writeframes(bytes(48000 * 4 * 10))

    run(runtime / "ASRTU1_Launcher.exe", ["--check-session-directory"],
        output, environment)
    ready = output / "demod-ready.txt"
    screenshot = output / "demod.png"
    # The marker is written after MainWindow constructs the GNU Radio flow graph.
    # A live process alone is insufficient: it could be stuck in an error dialog.
    run(runtime / "ASRTU1_Demod_CQt.exe",
        ["--screenshot", screenshot, "--wav", wav, "--ready-file", ready],
        output, environment)
    if not ready.is_file() or ready.read_text() != "ready":
        raise RuntimeError("Demodulator did not confirm initialization")
    require_png(screenshot)

    # Preview mode renders the uploader without sending any observations.
    screenshot = output / "satnogs.png"
    run(runtime / "ASRTU_SatnogsUploader.exe",
        ["--satellite", "JAMX", "--norad-id", "100470", "--source", "CI-TEST",
         "--longitude", "0", "--latitude", "0", "--altitude", "0",
         "--screenshot", screenshot], output, environment)
    require_png(screenshot)
    print(f"Packaged Windows startup checks passed: {output}", flush=True)


def require_png(path):
    if not path.is_file() or not path.read_bytes().startswith(b"\x89PNG\r\n\x1a\n"):
        raise RuntimeError(f"Missing or invalid startup screenshot: {path}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("runtime", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    # Every invocation gets fresh settings/markers, including when rerunning CI.
    output = Path(tempfile.mkdtemp(prefix="startup-", dir=args.output.resolve()))
    print(f"Startup diagnostics: {output}", flush=True)
    check(args.runtime.resolve(), output)


if __name__ == "__main__":
    main()
