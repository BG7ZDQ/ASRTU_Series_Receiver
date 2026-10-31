#!/usr/bin/env python3
"""Exercise packaged Windows binaries without developer DLLs or user settings."""

import argparse
import configparser
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


def run(executable, arguments, output, environment, verify_crt=False, expected_exit=0):
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
    if code != expected_exit:
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
    # Mirror the installer's executable-only post-install normalization.
    # A copied file otherwise inherits Low integrity in a sandbox workspace.
    icacls = Path(os.environ["SystemRoot"]) / "System32" / "icacls.exe"
    for name in ("ASRTU1_Launcher.exe", "ASRTU1_Demod_CQt.exe", "TinyDoppler.exe",
                 "ASRTU_SatnogsUploader.exe", "sndfile-convert.exe"):
        path = destination / "bin" / name
        if path.is_file():
            subprocess.run([str(icacls), str(path), "/setintegritylevel", "M"],
                           check=True, capture_output=True)
    return destination


def check(runtime, output, installed_layout=False):
    # Child processes inherit this mode: loader failures must not wait in a dialog.
    ctypes.windll.kernel32.SetErrorMode(0x8003)
    environment = {
        key: value for key, value in os.environ.items()
        if key.upper() not in {"PATH", "TEMP", "TMP", "LOCALAPPDATA", "APPDATA", "ASRTU_CONFIG_DIR"}
        and not key.upper().startswith(("QT_", "CONDA", "PYTHON", "GR_", "TINY_DOPPLER_"))
    }
    windows = Path(os.environ["SystemRoot"])
    environment["PATH"] = f"{windows / 'System32'};{windows}"
    for variable, folder in (("TEMP", "temp"), ("TMP", "temp"),
                             ("LOCALAPPDATA", "local"), ("APPDATA", "roaming")):
        directory = output / folder
        directory.mkdir(exist_ok=True)
        environment[variable] = str(directory)

    environment["ASRTU_CONFIG_DIR"] = str(output / "config")

    application_dir = runtime / "bin" if installed_layout else runtime

    def executable(name):
        return application_dir / name

    # Missing local CRT files must fail even on CI hosts with a new system CRT.
    for name in ("msvcp140.dll", "vcruntime140.dll"):
        if not (application_dir / name).is_file():
            raise RuntimeError(f"Missing application-local runtime: {application_dir / name}")
    if installed_layout:
        for module in ("launcher", "decoder", "doppler", "satnogs-uploader"):
            if (runtime / module).exists():
                raise RuntimeError(f"Unexpected duplicated runtime directory: {module}")
        for path in runtime.rglob("*"):
            if path.is_file() and (path.is_symlink() or path.stat().st_nlink != 1):
                raise RuntimeError(f"Installed file must be an ordinary single-link file: {path}")

    wav = output / "silence.wav"
    with wave.open(str(wav), "wb") as stream:
        stream.setparams((2, 2, 48000, 0, "NONE", "not compressed"))
        stream.writeframes(bytes(48000 * 4 * 10))

    run(executable("ASRTU1_Launcher.exe"), ["--check-session-directory"],
        output, environment)
    # Exercise the installer's station-to-INI adapter without installing or
    # changing real station settings. ASRTU_CONFIG_DIR isolates every write.
    station_fixture = output / "station.cfg"
    station_fixture.write_text(
        'proxy_nickname = "CI-TEST";\nproxy_long = 120.5;\n'
        'proxy_lat = -12.5;\nproxy_alt = 42;\nsat_name = "JAMX";\n',
        encoding="utf-8")
    run(executable("ASRTU1_Launcher.exe"), ["--import-proxy-config", station_fixture],
        output, environment)
    station_ini = configparser.ConfigParser()
    station_ini.read(output / "config" / "launcher.ini", encoding="utf-8-sig")
    if (station_ini.get("General", "nickname", fallback="") != "CI-TEST"
            or station_ini.get("General", "latitude", fallback="") != "-12.5"):
        raise RuntimeError("Installer station import did not persist INI settings")
    ready = output / "demod-ready.txt"
    screenshot = output / "demod.png"
    # The marker is written after MainWindow constructs the GNU Radio flow graph.
    # A live process alone is insufficient: it could be stuck in an error dialog.
    run(executable("ASRTU1_Demod_CQt.exe"),
        ["--screenshot", screenshot, "--wav", wav, "--ready-file", ready],
        output, environment, verify_crt=True)
    if not ready.is_file() or ready.read_text() != "ready":
        raise RuntimeError("Demodulator did not confirm initialization")
    require_png(screenshot)

    # Preview mode renders the uploader without sending any observations.
    screenshot = output / "satnogs.png"
    run(executable("ASRTU_SatnogsUploader.exe"),
        ["--satellite", "JAMX", "--norad-id", "100470", "--source", "CI-TEST",
         "--longitude", "0", "--latitude", "0", "--altitude", "0",
         "--screenshot", screenshot], output, environment)
    require_png(screenshot)
    screenshot = output / "doppler.png"
    if not installed_layout:
        environment["TINY_DOPPLER_CONFIG_DIR"] = str(output / "doppler-config")
    run(executable("TinyDoppler.exe"), ["--screenshot-catalog", screenshot],
        output, environment, verify_crt=True)
    require_png(screenshot)
    if installed_layout:
        check_live_startup(application_dir, output, environment)
    print(f"Packaged Windows startup checks passed: {output}", flush=True)


def check_live_startup(application_dir, output, environment):
    # --wav bypasses the live-instance lock, so playback-only smoke tests
    # miss permissions failures. Use a fresh directory in the real user TEMP,
    # not another Low-labelled directory under the source workspace.
    user_temp = Path(os.environ["LOCALAPPDATA"]) / "Temp"
    with tempfile.TemporaryDirectory(prefix="asrtu-live-check-", dir=user_temp) as temporary:
        live_environment = environment.copy()
        live_environment.update(TEMP=temporary, TMP=temporary)
        live = output / "live-startup"
        live.mkdir()
        ready = live / "ready.txt"
        run(application_dir / "ASRTU1_Demod_CQt.exe",
            ["--screenshot", live / "live.png", "--shared-iq-bridge", "--no-record",
             "--session-dir", live, "--ready-file", ready],
            live, live_environment, verify_crt=True)
        if ready.read_text() != "ready":
            raise RuntimeError("Live decoder did not confirm initialization")
        require_png(live / "live.png")

        # A lock-path failure must be reported as an error, never as another
        # running instance (exit 3). No actual microphone/SDR input is needed.
        unavailable = Path(temporary) / "not-a-directory"
        unavailable.write_text("fixture")
        live_environment.update(TEMP=str(unavailable), TMP=str(unavailable))
        failure = output / "lock-path-failure"
        failure.mkdir()
        status = failure / "status.txt"
        run(application_dir / "ASRTU1_Demod_CQt.exe",
            ["--shared-iq-bridge", "--no-record", "--ready-file", status],
            failure, live_environment, expected_exit=4)
        if not status.read_text(encoding="utf-8").startswith("error:4\n"):
            raise RuntimeError("Lock-path failure was not published to the launcher")


def require_png(path):
    if not path.is_file() or not path.read_bytes().startswith(b"\x89PNG\r\n\x1a\n"):
        raise RuntimeError(f"Missing or invalid startup screenshot: {path}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("runtime", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    layout = parser.add_mutually_exclusive_group()
    layout.add_argument("--installed-layout", action="store_true",
                        help="Test an already extracted shared-bin layout")
    layout.add_argument("--installer-stage", action="store_true",
                        help="Recreate and test the generated Inno core file layout")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    # Every invocation gets fresh settings/markers, including when rerunning CI.
    output = Path(tempfile.mkdtemp(prefix="startup-", dir=args.output.resolve()))
    print(f"Startup diagnostics: {output}", flush=True)
    runtime = args.runtime.resolve()
    if args.installer_stage:
        runtime = materialize_installer(runtime, output / "installed")
    check(runtime, output, installed_layout=args.installer_stage or args.installed_layout)


if __name__ == "__main__":
    main()
