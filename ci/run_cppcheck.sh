#!/usr/bin/env bash
set -euo pipefail

build_dir=${1:?missing build directory}

# Qt's generated moc/rcc translation units are not maintained source. Keep
# all handwritten code (including submodules) and its compilation options.
database=$(mktemp "$build_dir/cppcheck-commands.XXXXXX.json")
trap 'rm -f -- "$database"' EXIT
jq '[.[] | select(.file | test("(^|/)[^/]*_autogen/") | not)]' \
    "$build_dir/compile_commands.json" > "$database"

cppcheck --project="$database" \
	--enable=warning,performance,portability \
	--error-exitcode=1 \
	--inline-suppr \
	--suppress=missingIncludeSystem \
	--suppress=syntaxError \
	--suppress=unknownMacro \
	--suppress='uninitvar:*external/TinyDoppler/third_party/sgp4/sgp4.c' \
	--suppress=checkersReport
