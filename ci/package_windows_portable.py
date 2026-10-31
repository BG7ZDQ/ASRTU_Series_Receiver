#!/usr/bin/env python3
"""Archive the clean installer stage as a complete, relocatable Windows suite."""
import argparse
from pathlib import Path
import re
import zipfile

REQUIRED = (
    "bin/ASRTU1_Launcher.exe", "bin/ASRTU1_Demod_CQt.exe",
    "bin/TinyDoppler.exe", "bin/ASRTU_SatnogsUploader.exe",
    "bin/msvcp140.dll", "bin/vcruntime140.dll", "bin/platforms/qwindows.dll",
    "proxy/ASRTU_Proxy.exe", "proxy/proxy_mmt_gui.exe",
    "sdrsharp/SDRSharp.exe", "sdrsharp/Plugins/SDRSharp.AstroSeriesBridge.dll",
)
MODULES = ("bin", "proxy", "sdrsharp", "sdrpp", "plutosdr-driver")
README = """ASRTU Series Receiver - Windows portable / Windows 绿色版

解压整个文件夹到可写目录，运行 bin\\ASRTU1_Launcher.exe。
无需安装本套件、radioconda 或 GNU Radio。不要单独移动 EXE 或 DLL。
上传前请在启动器中填写呼号和地面站信息。设备驱动仍需按硬件要求安装。
启动器和解码器设置保存于 config/*.ini，不使用注册表；语言和跟踪设置仍为独立 INI。

Extract the entire folder to a writable location and run bin\\ASRTU1_Launcher.exe.
No suite, radioconda or GNU Radio installation is required. Keep all DLLs beside
the executables. Configure your station before uploading. Hardware may still
require drivers. Launcher/decoder preferences use config/*.ini, not the registry.
Language and tracking preferences remain in their separate INI files.
This ZIP does not create shortcuts, register an uninstaller or install drivers.
See the included third-party notices for the bundled SDR applications.
"""


def package(stage, output, repo):
    for name in REQUIRED:
        if not (stage / name).is_file():
            raise ValueError(f"Missing portable runtime input: {name}")
    version = re.search(r"project\(ASRTU1Qt VERSION ([0-9.]+)",
                        (repo / "CMakeLists.txt").read_text(encoding="utf-8")).group(1)
    name = f"ASRTU_Series_Receiver-{version}-x86_64-WinPortable"
    output.mkdir(parents=True, exist_ok=True)
    archive = output / f"{name}.zip"
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as bundle:
        for module in MODULES:
            for path in sorted((stage / module).rglob("*")):
                if path.is_file():
                    relative = path.relative_to(stage).as_posix()
                    if relative != "bin/README.txt":  # Installer-input instructions.
                        bundle.write(path, f"{name}/{relative}")
        bundle.writestr(f"{name}/README.txt", README.encode("utf-8-sig"))
        bundle.write(repo / "LICENSE", f"{name}/LICENSE")
        for path in sorted((repo / "packaging/inno").glob("*NOTICE*.txt")):
            bundle.write(path, f"{name}/{path.name}")
    return archive


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("stage", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    print(package(args.stage, args.output, Path(__file__).resolve().parents[1]))


if __name__ == "__main__":
    main()
