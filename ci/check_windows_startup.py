#!/usr/bin/env python3
"""Exercise packaged Windows binaries without developer DLLs or user settings."""

import argparse
import ctypes
from ctypes import wintypes
import json
import re
import shutil
import time
import os
from pathlib import Path
import subprocess
import tempfile
import wave


def loaded_crt_modules(pid):
    """Observe this child process's actual loader choices, not PATH guesses."""
    class ModuleEntry(ctypes.Structure):
        _fields_ = [
            ("dwSize", wintypes.DWORD), ("th32ModuleID", wintypes.DWORD),
            ("th32ProcessID", wintypes.DWORD), ("GlblcntUsage", wintypes.DWORD),
            ("ProccntUsage", wintypes.DWORD), ("modBaseAddr", ctypes.c_void_p),
            ("modBaseSize", wintypes.DWORD), ("hModule", wintypes.HMODULE),
            ("szModule", wintypes.WCHAR * 256), ("szExePath", wintypes.WCHAR * 260),
        ]
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.CreateToolhelp32Snapshot.argtypes = [wintypes.DWORD, wintypes.DWORD]
    kernel.CreateToolhelp32Snapshot.restype = wintypes.HANDLE
    kernel.Module32FirstW.argtypes = [wintypes.HANDLE, ctypes.POINTER(ModuleEntry)]
    kernel.Module32NextW.argtypes = kernel.Module32FirstW.argtypes
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    snapshot = kernel.CreateToolhelp32Snapshot(0x08 | 0x10, pid)
    if snapshot == ctypes.c_void_p(-1).value:
        # A starting/exiting process can briefly have no readable module list.
        if ctypes.get_last_error() in (6, 24, 87, 299):
            return {}
        raise ctypes.WinError(ctypes.get_last_error())
    result = {}
    try:
        entry = ModuleEntry()
        entry.dwSize = ctypes.sizeof(entry)
        present = kernel.Module32FirstW(snapshot, ctypes.byref(entry))
        while present:
            name = entry.szModule.lower()
            if re.fullmatch(r"(?:msvcp|vcruntime|concrt)140(?:_\w+)?\.dll", name):
                result[name] = Path(entry.szExePath)
            present = kernel.Module32NextW(snapshot, ctypes.byref(entry))
    finally:
        kernel.CloseHandle(snapshot)
    return result


def run(executable, arguments, output, environment, verify_crt=False):
    startup = subprocess.STARTUPINFO()
    startup.dwFlags = subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    print(f"Starting {executable.name}", flush=True)
    observed = {}
    with (output / f"{executable.stem}.stdout.txt").open("w") as stdout, (
        output / f"{executable.stem}.stderr.txt"
    ).open("w") as stderr:
        with subprocess.Popen(
            [str(executable), *map(str, arguments)], cwd=output, env=environment,
            stdout=stdout, stderr=stderr, startupinfo=startup,
        ) as process:
            try:
                deadline = time.monotonic() + 30
                while process.poll() is None:
                    if verify_crt:
                        observed.update(loaded_crt_modules(process.pid))
                        for name, path in observed.items():
                            if path.parent.resolve() != executable.parent.resolve():
                                raise RuntimeError(f"{executable.name}: {name} loaded from {path}, not its application directory")
                    if time.monotonic() >= deadline:
                        raise RuntimeError(f"{executable.name}: startup timed out")
                    try:
                        process.wait(timeout=0.05)
                    except subprocess.TimeoutExpired:
                        pass
                code = process.returncode
            except BaseException:
                process.kill()
                process.wait()
                raise
            finally:
                if verify_crt:
                    (output / f"{executable.stem}.loaded-crt.json").write_text(
                        json.dumps({k: str(v) for k, v in observed.items()}, indent=2), encoding="utf-8")
    if code != 0:
        raise RuntimeError(f"{executable.name}: exit status 0x{code & 0xffffffff:08X}")
    if verify_crt and "msvcp140.dll" not in observed:
        raise RuntimeError(f"{executable.name}: could not verify loaded MSVCP140.dll")


def materialize_installer(stage, destination):
    """Recreate core [Files] destinations without installing or editing registry."""
    pattern = re.compile(r'Source: "([^"]+)"; DestDir: "\{app\}\\([^"]+)"; DestName: "([^"]+)";')
    entries = 0
    for line in (stage / "core-files.iss").read_text(encoding="utf-8-sig").splitlines():
        if not line.strip() or line.startswith(";"):
            continue
        match = pattern.match(line)
        if not match:
            raise RuntimeError(f"Unexpected installer file entry: {line}")
        source, directory, name = match.groups()
        source = (stage.parent / source).resolve()
        target = (destination / directory / name).resolve()
        if not source.is_relative_to(stage.resolve()) or not target.is_relative_to(destination.resolve()):
            raise RuntimeError("Installer file entry escapes its managed directory")
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
        entries += 1
    if not entries:
        raise RuntimeError("Installer contains no core files")
    return destination


def check(runtime, output, installed_layout=False):
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

    def executable(module, name):
        return runtime / module / name if installed_layout else runtime / name

    # Missing local CRT files must fail even on CI hosts with a new system CRT.
    for module in ("launcher", "decoder", "doppler", "satnogs-uploader"):
        directory = runtime / module if installed_layout else runtime
        for name in ("msvcp140.dll", "vcruntime140.dll"):
            if not (directory / name).is_file():
                raise RuntimeError(f"Missing application-local runtime: {directory / name}")

    wav = output / "silence.wav"
    with wave.open(str(wav), "wb") as stream:
        stream.setparams((2, 2, 48000, 0, "NONE", "not compressed"))
        stream.writeframes(bytes(48000 * 4 * 10))

    run(executable("launcher", "ASRTU1_Launcher.exe"), ["--check-session-directory"],
        output, environment)
    ready = output / "demod-ready.txt"
    screenshot = output / "demod.png"
    # The marker is written after MainWindow constructs the GNU Radio flow graph.
    # A live process alone is insufficient: it could be stuck in an error dialog.
    run(executable("decoder", "ASRTU1_Demod_CQt.exe"),
        ["--screenshot", screenshot, "--wav", wav, "--ready-file", ready],
        output, environment, verify_crt=True)
    if not ready.is_file() or ready.read_text() != "ready":
        raise RuntimeError("Demodulator did not confirm initialization")
    require_png(screenshot)

    # Preview mode renders the uploader without sending any observations.
    screenshot = output / "satnogs.png"
    run(executable("satnogs-uploader", "ASRTU_SatnogsUploader.exe"),
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
    parser.add_argument("--installer-stage", action="store_true",
                        help="Recreate and test the generated Inno core file layout")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    # Every invocation gets fresh settings/markers, including when rerunning CI.
    output = Path(tempfile.mkdtemp(prefix="startup-", dir=args.output.resolve()))
    print(f"Startup diagnostics: {output}", flush=True)
    runtime = args.runtime.resolve()
    if args.installer_stage:
        runtime = materialize_installer(runtime, output / "installed")
    check(runtime, output, installed_layout=args.installer_stage)


if __name__ == "__main__":
    main()
