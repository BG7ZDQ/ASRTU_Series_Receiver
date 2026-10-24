#!/usr/bin/env python3
"""Fail closed unless every supported native bridge is present in the archive."""
import argparse
from pathlib import Path
import re
import zipfile

EXPECTED = (
    "SDR#/Windows-x86/SDRSharp.AstroSeriesBridge.dll",
    "SDR++/Windows-x64/asrtu_sink.dll",
    "SDR++/Linux-x86_64/asrtu_sink.so",
    "SDR++/Linux-arm64/asrtu_sink.so",
    "SDR++/macOS-x86_64/asrtu_sink.dylib",
    "SDR++/macOS-arm64/asrtu_sink.dylib",
)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("root", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    for name in EXPECTED:
        if not (args.root / name).is_file():
            parser.error(f"Missing precompiled plugin: {name}")
    repo = Path(__file__).resolve().parents[1]
    version = re.search(r"project\(ASRTU1Qt VERSION ([0-9.]+)",
                        (repo / "CMakeLists.txt").read_text(encoding="utf-8")).group(1)
    args.output.mkdir(parents=True, exist_ok=True)
    archive = args.output / f"SDR_Software_Plugins-{version}-AllPlatform.zip"
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as output:
        for path in sorted(args.root.rglob("*")):
            if path.is_file():
                output.write(path, path.relative_to(args.root).as_posix())
    print(archive)


if __name__ == "__main__":
    main()
