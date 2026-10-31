#!/usr/bin/env python3
"""Hash release assets and maintain one checksum section in release notes."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

PATTERNS = (
    "plugins/SDR_Software_Plugins-*-AllPlatform.zip",
    "linux-bundle/ASRTU_Series_Receiver-*-*-LinuxAppImage.zip",
    "windows-installer/ASRTU_Series_Receiver-*-*-WinInstaller.zip",
    "windows-portable/ASRTU_Series_Receiver-*-*-WinPortable.zip",
    "macos/ASRTU_Series_Receiver-*-x86_64-macOS.zip",
    "macos/ASRTU_Series_Receiver-*-arm64-macOS.zip",
)
START = "<!-- asrtu-sha256:start -->"
END = "<!-- asrtu-sha256:end -->"


def collect_assets(root):
    files = []
    for pattern in PATTERNS:
        matches = sorted(root.glob(pattern))
        if len(matches) != 1 or not matches[0].is_file():
            raise ValueError(f"Expected exactly one release asset: {pattern}")
        files.extend(matches)
    return sorted(files, key=lambda path: path.name)


def checksum_section(files):
    lines = [START, "## SHA256", "", "以下校验值对应本次发布的 ZIP 文件。",
             "These checksums apply to the ZIP assets attached to this release.",
             "", "| File | SHA256 |", "| --- | --- |"]
    for path in files:
        digest = hashlib.sha256()
        with path.open("rb") as source:
            for block in iter(lambda: source.read(1024 * 1024), b""):
                digest.update(block)
        lines.append(f"| `{path.name}` | `{digest.hexdigest()}` |")
    return "\n".join([*lines, END])


def merge_notes(body, section):
    if START in body or END in body:
        if body.count(START) != 1 or body.count(END) != 1:
            raise ValueError("Ambiguous checksum markers in release notes")
        start, end = body.index(START), body.index(END)
        if end < start:
            raise ValueError("Reversed checksum markers in release notes")
        return body[:start] + section + body[end + len(END):]
    return body + ("\n\n" if body else "") + section + "\n"


def publish_section(section, repository, release_id):
    endpoint = f"repos/{repository}/releases/{int(release_id)}"
    result = subprocess.run(["gh", "api", endpoint], check=True,
                            capture_output=True, encoding="utf-8")
    release = json.loads(result.stdout)
    body = merge_notes(release.get("body") or "", section)
    subprocess.run(["gh", "api", "--method", "PATCH", endpoint, "--input", "-"],
                   input=json.dumps({"body": body}), encoding="utf-8", check=True,
                   stdout=subprocess.DEVNULL)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("generate", "publish"))
    parser.add_argument("--root", type=Path, default=Path("dist"))
    parser.add_argument("--notes", type=Path, required=True)
    args = parser.parse_args()
    if args.mode == "generate":
        files = collect_assets(args.root)
        args.notes.parent.mkdir(parents=True, exist_ok=True)
        args.notes.write_text(checksum_section(files), encoding="utf-8")
        # The same exact file list drives hashing and upload.
        with open(os.environ["GITHUB_OUTPUT"], "a", encoding="utf-8") as output:
            output.write("files<<ASRTU_RELEASE_FILES\n")
            output.write("\n".join(path.as_posix() for path in files))
            output.write("\nASRTU_RELEASE_FILES\n")
    else:
        publish_section(args.notes.read_text(encoding="utf-8"),
                        os.environ["GITHUB_REPOSITORY"], os.environ["RELEASE_ID"])


if __name__ == "__main__":
    main()
