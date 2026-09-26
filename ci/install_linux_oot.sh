#!/usr/bin/env bash
set -euo pipefail

prefix=${1:?missing install prefix}
workdir=${2:?missing work directory}
config_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck disable=SC1091
source "$config_dir/oot-dependencies.env"

build_module()
{
	local name=$1
	local repository=$2
	local revision=$3
	shift 3
	local source_dir="$workdir/$name"
	local build_dir="$source_dir/build"

	rm -rf "$source_dir"
	git clone --no-checkout "$repository" "$source_dir"
	git -C "$source_dir" checkout --detach "$revision"
	if [[ "$name" == gr-lilacsat ]]; then
		git -C "$source_dir" apply "$config_dir/gr-lilacsat-python-off.patch"
	fi
	cmake -S "$source_dir" -B "$build_dir" -G Ninja \
		-DCMAKE_BUILD_TYPE=RelWithDebInfo \
		-DCMAKE_INSTALL_PREFIX="$prefix" \
		-DCMAKE_PREFIX_PATH="$prefix" \
		-DCMAKE_PROJECT_INCLUDE="$config_dir/gnuradio_python_module.cmake" \
		"$@"
	cmake --build "$build_dir" --parallel
	cmake --install "$build_dir"
}

mkdir -p "$prefix" "$workdir"
build_module gr-lilacsat "$LILACSAT_REPOSITORY" "$LILACSAT_REVISION" \
	-DENABLE_PYTHON=OFF -DENABLE_DOXYGEN=OFF
