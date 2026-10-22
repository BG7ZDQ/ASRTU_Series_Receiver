#!/usr/bin/env python3
"""Build the bridge against pinned SDR++ headers; record its exact ABI inputs."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess

REVISION = "53230af19102e600148a18af9d21e8a92b8701d0"
REPO = Path(__file__).resolve().parents[1]


def run(*args):
    try:
        return subprocess.check_output([str(a) for a in args], text=True, encoding="utf-8", errors="replace")
    except subprocess.CalledProcessError as error:
        print(error.output)
        raise


def import_library(dll, directory):
    exports = run("dumpbin", "/nologo", "/exports", dll)
    names = re.findall(r"^\s+\d+\s+[0-9A-F]+\s+[0-9A-F]+\s+(\S+)", exports, re.M)
    if not names:
        raise RuntimeError(f"No exports in {dll}")
    definition = directory / (dll.stem + ".def")
    definition.write_text(f"LIBRARY {dll.name}\nEXPORTS\n" + "\n".join(names), encoding="utf-8")
    library = directory / (dll.stem + ".lib")
    run("lib", "/nologo", "/machine:x64", f"/def:{definition}", f"/out:{library}")
    return library


def bundle_macos(binary, output):
    """Make Homebrew runtime paths relocatable next to the SDR++ module."""
    target = output / binary.name
    shutil.copy2(binary, target)
    pending = [(binary, target)]
    copied = {}
    while pending:
        source, destination = pending.pop()
        for line in run("otool", "-L", source).splitlines()[1:]:
            dependency = line.strip().split(" (")[0]
            if dependency.startswith(("/usr/lib/", "/System/")):
                continue
            if dependency.endswith("/" + source.name):
                continue  # dylib identity, not a dependency
            if not dependency.startswith("/"):
                raise RuntimeError(f"Unresolved module dependency: {dependency}")
            origin = Path(dependency)
            name = origin.name
            if name not in copied:
                copied[name] = origin.resolve()
                shutil.copy2(origin, output / name)
                pending.append((origin, output / name))
            elif copied[name] != origin.resolve():
                raise RuntimeError(f"Conflicting runtime libraries: {name}")
            run("install_name_tool", "-change", dependency, "@loader_path/" + name, destination)
        run("install_name_tool", "-id", "@loader_path/" + destination.name, destination)
    for path in output.glob("*.dylib"):
        run("codesign", "--force", "--sign", "-", path)
        run("codesign", "--verify", "--strict", path)
    return target


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--dependency-prefix", type=Path)
    args = parser.parse_args()
    revision = run("git", "-C", args.source, "rev-parse", "HEAD").strip()
    if revision != REVISION:
        raise RuntimeError(f"Expected SDR++ {REVISION}, got {revision}")
    args.build.mkdir(parents=True, exist_ok=True)
    args.output.mkdir(parents=True, exist_ok=True)
    options = [f"-DSDRPP_CORE_ROOT={args.source.resolve() / 'core'}"]
    metadata = {"sdrpp_revision": revision, "system": platform.system(),
                "architecture": platform.machine(), "compiler": os.environ.get("CXX", "platform default")}
    if os.name == "nt":
        if not args.dependency_prefix:
            parser.error("Windows requires --dependency-prefix (contains include/volk)")
        host = REPO / "packaging/payload/sdrpp"
        for stem, variable in (("sdrpp_core", "SDRPP_CORE_LIBRARY"), ("volk", "SDRPP_VOLK_LIBRARY")):
            dll = host / (stem + ".dll")
            library = import_library(dll, args.build.resolve())
            options.append(f"-D{variable}={library}")
            metadata[dll.name + "_sha256"] = hashlib.sha256(dll.read_bytes()).hexdigest()
        options.append(f"-DSDRPP_DEPENDENCY_INCLUDE_DIR={args.dependency_prefix.resolve() / 'include'}")
    run("cmake", "-S", REPO / "integrations/sdrpp/asrtu_sink", "-B", args.build,
        "-DCMAKE_BUILD_TYPE=Release", *options)
    run("cmake", "--build", args.build, "--config", "Release", "--parallel", "2")
    extension = ".dll" if os.name == "nt" else ".dylib" if platform.system() == "Darwin" else ".so"
    binaries = list(args.build.rglob("asrtu_sink" + extension))
    if len(binaries) != 1:
        raise RuntimeError(f"Expected one plugin, found {binaries}")
    binary = binaries[0]
    if platform.system() == "Darwin":
        binary = bundle_macos(binary, args.output)
        metadata["dependencies"] = run("otool", "-L", binary)
    elif os.name != "nt":
        metadata["dependencies"] = run("ldd", binary)
    metadata["plugin_sha256"] = hashlib.sha256(binary.read_bytes()).hexdigest()
    if binary.resolve() != (args.output / binary.name).resolve():
        shutil.copy2(binary, args.output / binary.name)
    (args.output / "build-info.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
