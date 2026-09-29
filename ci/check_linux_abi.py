#!/usr/bin/env python3
"""Reject ELF dependencies newer than the oldest supported glibc (22.04)."""
import argparse
import os
from pathlib import Path
import re
import subprocess


def requirements(output):
    """Read requirements, not the versions exported by a bundled library."""
    needed = False
    names = set()
    for line in output.splitlines():
        if line.startswith("Version "):
            needed = line.startswith("Version needs section")
        if needed:
            names.update(re.findall(r"Name: (GLIBC_\S+)", line))
    return names


def incompatible(names, maximum=(2, 35)):
    failures = []
    for name in sorted(names):
        match = re.fullmatch(r"GLIBC_(\d+(?:\.\d+)+)", name)
        if not match:
            # Includes private ABI and named requirements such as DT_RELR.
            failures.append(name)
            continue
        version = tuple(map(int, match.group(1).split(".")))
        if version > maximum:
            failures.append(name)
    return failures


def check(paths):
    seen = set()
    count = 0
    failures = []
    for root in paths:
        if not root.exists():
            raise RuntimeError(f"Missing ABI check input: {root}")
        for path in (root.rglob("*") if root.is_dir() else [root]):
            if path.is_dir():
                continue
            resolved = path.resolve(strict=True)
            if resolved in seen:
                continue
            seen.add(resolved)
            with path.open("rb") as stream:
                if stream.read(4) != b"\x7fELF":
                    continue
            count += 1
            result = subprocess.run(
                ["readelf", "--version-info", "--wide", str(path)],
                capture_output=True, text=True,
                env={**os.environ, "LC_ALL": "C"}, check=False)
            if result.returncode:
                raise RuntimeError(f"Unable to inspect ELF {path}: {result.stderr}")
            for name in incompatible(requirements(result.stdout)):
                failures.append(f"{path}: requires {name}")
    if not count:
        raise RuntimeError("No ELF files found; refusing an empty ABI check")
    if failures:
        raise RuntimeError("Ubuntu 22.04 ABI check failed:\n" + "\n".join(failures))
    print(f"Checked {count} ELF files: required glibc <= 2.35")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paths", nargs="+", type=Path)
    arguments = parser.parse_args()
    try:
        check(arguments.paths)
    except (OSError, RuntimeError) as error:
        parser.exit(1, f"{error}\n")


if __name__ == "__main__":
    main()
