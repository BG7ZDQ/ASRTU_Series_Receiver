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

copy_oot_library()
{
	local name=$1
	local -a files

	mapfile -t files < <(find "$oot_prefix" \( -type f -o -type l \) \
		-name "$name*" -print)
	if ((${#files[@]} == 0)); then
		echo "unable to find $name in $oot_prefix" >&2
		exit 1
	fi
	cp -a "${files[@]}" "$appdir/usr/lib/"
}

copy_oot_library libgnuradio-lilacsat.so
copy_oot_library libgnuradio-hyacinthsat.so
oot_library_dirs=$(find "$oot_prefix" -type f -name 'libgnuradio-*.so*' \
	-printf '%h\n' | sort -u | paste -sd:)

pushd "$output_dir" >/dev/null
PATH="$tool_dir:$PATH" \
LD_LIBRARY_PATH="$appdir/usr/lib:$oot_library_dirs${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
LINUXDEPLOY_PLUGIN_QT="$linuxdeploy_plugin_qt" \
LINUXDEPLOY_OUTPUT_VERSION="$version" \
NO_STRIP=1 ARCH=x86_64 "$linuxdeploy" \
	--appdir "$appdir" \
	--executable "$appdir/usr/bin/ASRTU1_Launcher" \
	--executable "$appdir/usr/bin/ASRTU1_Demod_CQt" \
	--executable "$appdir/usr/bin/ASRTU_Doppler" \
	--desktop-file "$appdir/usr/share/applications/asrtu-series-receiver.desktop" \
	--icon-file "$appdir/usr/share/icons/hicolor/512x512/apps/asrtu-series-receiver.png" \
	--library "$appdir/usr/lib/libgnuradio-lilacsat.so" \
	--library "$appdir/usr/lib/libgnuradio-hyacinthsat.so" \
	--plugin qt \
	--output appimage
popd >/dev/null
