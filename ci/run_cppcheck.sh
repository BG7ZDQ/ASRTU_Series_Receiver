#!/usr/bin/env bash
set -euo pipefail

build_dir=${1:?missing build directory}

cppcheck --project="$build_dir/compile_commands.json" \
	--enable=warning,performance,portability \
	--error-exitcode=1 \
	--inline-suppr \
	--suppress=missingIncludeSystem \
	--suppress=syntaxError \
	--suppress=unknownMacro \
	--suppress=checkersReport
