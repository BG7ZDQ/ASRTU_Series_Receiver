#!/usr/bin/env bash
set -euo pipefail

appimage=${1:?missing AppImage}
plugin=${2:?missing Linux asrtu_sink.so}
sdrpp_deb=${3:-}
output_dir=${4:?missing output directory}
version=${5:?missing version}

[[ -f "$appimage" ]] || { echo "AppImage not found: $appimage" >&2; exit 1; }
[[ -f "$plugin" ]] || { echo "SDR++ plugin not found: $plugin" >&2; exit 1; }
if [[ -n "$sdrpp_deb" ]]; then
	[[ -f "$sdrpp_deb" ]] || { echo "SDR++ package not found: $sdrpp_deb" >&2; exit 1; }
fi

setup_script="$(cd "$(dirname "$0")/../packaging/linux" && pwd)/setup.sh"
[[ -f "$setup_script" ]] || { echo "setup.sh not found: $setup_script" >&2; exit 1; }

bundle_name="ASRTU_Series_Receiver-${version}-LinuxAppImage"
output_dir=$(realpath -m "$output_dir")
work=$(mktemp -d "${TMPDIR:-/tmp}/asrtu-linux-bundle.XXXXXX")
trap 'rm -rf -- "$work"' EXIT
mkdir -p "$work/$bundle_name/sdrpp" "$work/$bundle_name/plugins" "$output_dir"

cp "$appimage" "$work/$bundle_name/"
cp "$plugin" "$work/$bundle_name/plugins/asrtu_sink.so"
if [[ -n "$sdrpp_deb" ]]; then
	cp "$sdrpp_deb" "$work/$bundle_name/sdrpp/"
fi
cp "$setup_script" "$work/setup.sh"
chmod 755 "$work/setup.sh"

archive="$output_dir/${bundle_name}.zip"
rm -f -- "$archive"
(cd "$work" && zip -qr "$archive" setup.sh "$bundle_name")
echo "$archive"
