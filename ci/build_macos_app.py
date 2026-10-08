#!/usr/bin/env python3
"""Assemble a self-contained ASRTU Series Receiver.app from a macOS build tree.

Counterpart of ci/build_appimage.sh.  Everything the suite needs ends up inside
the bundle: the launcher (the entry point, as on Windows), the decoder, Tiny
Doppler, the SatNOGS uploader, the upload proxy, every non-system dylib they
pull in, and the Qt plugins.  Install names and rpaths are rewritten so the app
runs on a Mac with none of the build-time prefixes present.

usage:
  ci/build_macos_app.py --build-dir BUILD --output DIR \
      --dep-prefix ~/miniforge3/envs/asrtu-receiver --dep-prefix /path/to/oot-install

The first --dep-prefix must be the Qt installation (its plugins/ directory is
used).  The bundle version is read from the project's CMakeLists.txt.
"""
import argparse
import os
import re
import shutil
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Entry point first: CFBundleExecutable.  The launcher finds the others beside
# it via QCoreApplication::applicationDirPath() (suite_launcher.cpp).
EXECUTABLES = [
    "ASRTU1_Launcher",
    "ASRTU1_Demod_CQt",
    "TinyDoppler",
    "ASRTU_SatnogsUploader",
    "ASRTU_UploadProxy",
]
QT_PLUGINS = {
    "platforms": ["libqcocoa.dylib", "libqoffscreen.dylib", "libqminimal.dylib"],
    "imageformats": ["libqjpeg.dylib", "libqgif.dylib", "libqico.dylib"],
    "styles": ["libqmacstyle.dylib"],
}
TRANSLATIONS = ["asrtu_en.qm", "asrtu_ja.qm"]
ICON_SOURCE = os.path.join(REPO, "assets", "branding", "astro_series_launcher.png")
BUNDLE_ID = "org.asrtu.receiver"
MACHO_MAGIC = (b"\xcf\xfa\xed\xfe", b"\xca\xfe\xba\xbe")
SYSTEM_PREFIXES = ("/usr/lib", "/System")
BUNDLE_RPATH_PREFIXES = ("@executable_path", "@loader_path")


def sh(*argv):
    return subprocess.run(argv, capture_output=True, text=True).stdout


def is_macho(path):
    try:
        with open(path, "rb") as handle:
            return handle.read(4) in MACHO_MAGIC
    except OSError:
        return False


def dependencies(path):
    result = []
    for line in sh("otool", "-L", path).splitlines()[1:]:
        name = line.strip().split(" ")[0]
        if name.startswith(SYSTEM_PREFIXES) or name.endswith(os.path.basename(path)):
            continue
        result.append(name)
    return result


def rpaths(path):
    return re.findall(r"path (.+?) \(offset \d+\)", sh("otool", "-l", path))


def project_version():
    text = open(os.path.join(REPO, "CMakeLists.txt"), encoding="utf-8").read()
    match = re.search(r"project\(\w+\s+VERSION\s+([0-9.]+)", text)
    if not match:
        sys.exit("cannot read project VERSION from CMakeLists.txt")
    return match.group(1)


def find_executable(build_dir, name):
    # add_subdirectory() targets (Tiny Doppler) land in a sub-tree of the build
    # directory, so search rather than assume a flat layout.
    for root, _, files in os.walk(build_dir):
        if "CMakeFiles" in root:
            continue
        if name in files and is_macho(os.path.join(root, name)):
            return os.path.join(root, name)
    sys.exit(f"executable {name} not found under {build_dir}")


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--build-dir", required=True)
    parser.add_argument("--output", required=True, help="directory to create the .app in")
    parser.add_argument("--dep-prefix", action="append", required=True,
                        help="prefix whose lib/ resolves @rpath dependencies; first one supplies Qt plugins")
    parser.add_argument("--name", default="ASRTU Series Receiver")
    args = parser.parse_args()

    version = project_version()
    search_dirs = [os.path.join(p, "lib") for p in args.dep_prefix]
    qt_plugins_dir = os.path.join(args.dep_prefix[0], "plugins")

    def resolve(name):
        base = name[len("@rpath/"):] if name.startswith("@rpath/") else name
        if base.startswith(("@loader_path/", "@executable_path/")):
            return None
        if os.path.isabs(base):
            return base if os.path.exists(base) else None
        for directory in search_dirs:
            candidate = os.path.join(directory, base)
            if os.path.exists(candidate):
                return candidate
        return None

    # ---- skeleton ------------------------------------------------------------
    app = os.path.join(args.output, f"{args.name}.app")
    if os.path.exists(app):
        shutil.rmtree(app)
    macos = os.path.join(app, "Contents", "MacOS")
    frameworks = os.path.join(app, "Contents", "Frameworks")
    resources = os.path.join(app, "Contents", "Resources")
    for directory in (macos, frameworks, resources):
        os.makedirs(directory)

    executables = []
    for name in EXECUTABLES:
        destination = os.path.join(macos, name)
        shutil.copy2(find_executable(args.build_dir, name), destination)
        os.chmod(destination, 0o755)
        executables.append(destination)

    translations = os.path.join(macos, "translations")
    os.makedirs(translations)
    for catalogue in TRANSLATIONS:
        shutil.copy2(os.path.join(REPO, "assets", "translations", catalogue), translations)

    # ---- Qt plugins, before the dependency walk ------------------------------
    # libqcocoa needs libQt5DBus, which no executable references directly.
    # Walking only the executables leaves it out and the app aborts under Finder.
    plugins = []
    for group, names in QT_PLUGINS.items():
        group_dir = os.path.join(app, "Contents", "PlugIns", group)
        os.makedirs(group_dir, exist_ok=True)
        for name in names:
            source = os.path.join(qt_plugins_dir, group, name)
            if not os.path.exists(source):
                continue
            destination = os.path.join(group_dir, name)
            shutil.copy2(source, destination)
            os.chmod(destination, 0o755)
            plugins.append(destination)
    with open(os.path.join(resources, "qt.conf"), "w") as handle:
        handle.write("[Paths]\nPlugins = PlugIns\n")

    # ---- icon ----------------------------------------------------------------
    if os.path.exists(ICON_SOURCE):
        iconset = os.path.join(args.output, "asrtu.iconset")
        os.makedirs(iconset, exist_ok=True)
        for size in (16, 32, 64, 128, 256, 512):
            sh("sips", "-z", str(size), str(size), ICON_SOURCE,
               "--out", os.path.join(iconset, f"icon_{size}x{size}.png"))
            sh("sips", "-z", str(size * 2), str(size * 2), ICON_SOURCE,
               "--out", os.path.join(iconset, f"icon_{size}x{size}@2x.png"))
        sh("iconutil", "-c", "icns", iconset, "-o", os.path.join(resources, "app.icns"))
        shutil.rmtree(iconset, ignore_errors=True)

    # ---- Info.plist ----------------------------------------------------------
    # CFBundleLocalizations matters for language selection: on macOS
    # QLocale::system() reports the bundle's best declared localization, not the
    # user's system language.  Without this list a Chinese-language Mac is
    # reported as en_US and the UI comes up in English.  The development region
    # mirrors translation.cpp's fallback for other languages.
    #
    # NSRequiresAquaSystemAppearance keeps every program in the bundle in the
    # light appearance.  The stylesheets paint light cards and buttons but
    # leave most label and checkbox text to the palette, which macOS dark
    # mode turns white, so that text would vanish against the cards.
    with open(os.path.join(app, "Contents", "Info.plist"), "w") as handle:
        handle.write(f"""<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
  <key>CFBundleExecutable</key><string>{EXECUTABLES[0]}</string>
  <key>CFBundleIdentifier</key><string>{BUNDLE_ID}</string>
  <key>CFBundleName</key><string>ASRTU Receiver</string>
  <key>CFBundleDisplayName</key><string>{args.name}</string>
  <key>CFBundlePackageType</key><string>APPL</string>
  <key>CFBundleShortVersionString</key><string>{version}</string>
  <key>CFBundleVersion</key><string>{version}</string>
  <key>CFBundleIconFile</key><string>app.icns</string>
  <key>CFBundleDevelopmentRegion</key><string>en</string>
  <key>CFBundleLocalizations</key>
  <array>
    <string>zh-Hans</string>
    <string>zh-Hant</string>
    <string>zh</string>
    <string>en</string>
    <string>ja</string>
  </array>
  <key>LSMinimumSystemVersion</key><string>11.0</string>
  <key>NSHighResolutionCapable</key><true/>
  <key>NSRequiresAquaSystemAppearance</key><true/>
  <key>NSMicrophoneUsageDescription</key>
  <string>ASRTU Series Receiver captures receiver audio to demodulate satellite telemetry.</string>
</dict></plist>
""")

    # ---- dependency closure over executables AND plugins --------------------
    # Keyed by the name each library is requested under: conda ships
    # libFoo.5.dylib as a symlink to libFoo.5.15.15.dylib, and the requested
    # soname is what has to exist in Frameworks/.
    wanted, stack = {}, list(executables) + list(plugins)
    while stack:
        current = stack.pop()
        for name in dependencies(current):
            base = os.path.basename(name)
            if base in wanted:
                continue
            resolved = resolve(name)
            if not resolved:
                continue
            wanted[base] = resolved
            stack.append(resolved)
    for base, source in wanted.items():
        destination = os.path.join(frameworks, base)
        if not os.path.exists(destination):
            shutil.copy2(source, destination)   # follows symlinks: real file, requested name
            os.chmod(destination, 0o755)
    libraries = [os.path.join(frameworks, base) for base in wanted]

    # ---- rewrite install names and rpaths ------------------------------------
    for path in executables + plugins + libraries:
        if path in libraries:
            sh("install_name_tool", "-id", "@rpath/" + os.path.basename(path), path)
        for name in dependencies(path):
            base = os.path.basename(name)
            if base in wanted and not name.startswith("@rpath/"):
                sh("install_name_tool", "-change", name, "@rpath/" + base, path)
        # CMake bakes the build-time library directory into the binaries.  Left
        # in place it shadows the bundled copies on the build machine, so the
        # bundle is never really exercised there, and is absent everywhere else.
        for rpath in rpaths(path):
            if not rpath.startswith(BUNDLE_RPATH_PREFIXES):
                sh("install_name_tool", "-delete_rpath", rpath, path)
        if path in executables:
            wanted_rpath = "@executable_path/../Frameworks"
        elif path in plugins:
            wanted_rpath = "@loader_path/../../Frameworks"
        else:
            wanted_rpath = "@loader_path"
        if wanted_rpath not in rpaths(path):
            sh("install_name_tool", "-add_rpath", wanted_rpath, path)

    # Ad-hoc signature: arm64 refuses to run unsigned code at all.  This is not
    # a Developer ID signature and does not satisfy Gatekeeper on other Macs.
    sh("codesign", "--force", "--deep", "--sign", "-", app)

    # ---- audit: nothing may point outside the bundle --------------------------
    external = []
    for root, _, files in os.walk(app):
        for name in files:
            path = os.path.join(root, name)
            if not is_macho(path):
                continue
            for dependency in dependencies(path):
                if not dependency.startswith(("@rpath", "@loader_path", "@executable_path")):
                    external.append((path, dependency))
            for rpath in rpaths(path):
                if not rpath.startswith(BUNDLE_RPATH_PREFIXES):
                    external.append((path, "rpath " + rpath))
    total = sum(os.path.getsize(os.path.join(root, name))
                for root, _, files in os.walk(app) for name in files)
    print(f"{app}\n  version {version}, {len(wanted)} libraries, {total / 1e6:.0f} MB")
    if external:
        for path, dependency in external[:20]:
            print(f"  EXTERNAL: {os.path.relpath(path, app)} -> {dependency}")
        sys.exit(f"{len(external)} reference(s) point outside the bundle")
    print("  self-containment audit: OK")


if __name__ == "__main__":
    main()
