#!/usr/bin/env bash
set -euo pipefail

build_dir=${1:?missing build directory}
oot_prefix=${2:?missing OOT prefix}
output_dir=${3:?missing output directory}
linuxdeploy=${LINUXDEPLOY:?missing linuxdeploy path}
linuxdeploy_plugin_qt=${LINUXDEPLOY_PLUGIN_QT:?missing linuxdeploy Qt plugin path}
version=${VERSION:?missing version}
version=${version#v}
output_dir=$(realpath -m "$output_dir")
oot_prefix=$(realpath "$oot_prefix")
tool_dir=$(dirname "$linuxdeploy")
appdir="$output_dir/AppDir"

if [[ ! -x "$linuxdeploy_plugin_qt" ]]; then
	echo "linuxdeploy Qt plugin is not executable: $linuxdeploy_plugin_qt" >&2
	exit 1
fi

ln -sfn "$(basename "$linuxdeploy_plugin_qt")" \
	"$tool_dir/linuxdeploy-plugin-qt"

rm -rf "$appdir"
mkdir -p "$output_dir"
cmake --install "$build_dir" --prefix "$appdir/usr"
install -d "$appdir/usr/lib"

for library in libgnuradio-lilacsat.so libgnuradio-hyacinthsat.so; do
	cp -a "$oot_prefix/lib/$library"* "$appdir/usr/lib/"
done

pushd "$output_dir" >/dev/null
PATH="$tool_dir:$PATH" \
LD_LIBRARY_PATH="$appdir/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
LINUXDEPLOY_PLUGIN_QT="$linuxdeploy_plugin_qt" \
LINUXDEPLOY_OUTPUT_VERSION="$version" \
NO_STRIP=1 ARCH=x86_64 "$linuxdeploy" \
	--appdir "$appdir" \
	--executable "$appdir/usr/bin/ASRTU1_Demod_CQt" \
	--desktop-file "$appdir/usr/share/applications/asrtu-series-receiver.desktop" \
	--icon-file "$appdir/usr/share/icons/hicolor/512x512/apps/asrtu-series-receiver.png" \
	--library "$appdir/usr/lib/libgnuradio-lilacsat.so" \
	--library "$appdir/usr/lib/libgnuradio-hyacinthsat.so" \
	--plugin qt \
	--output appimage
popd >/dev/null
