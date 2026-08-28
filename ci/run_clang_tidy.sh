#!/usr/bin/env bash
set -euo pipefail

build_dir=${1:?missing build directory}
base=${2:?missing base revision}
checks='-*,clang-analyzer-*,-clang-analyzer-cplusplus.NewDelete,-clang-analyzer-cplusplus.NewDeleteLeaks,-clang-analyzer-optin.core.EnumCastOutOfRange,bugprone-undefined-memory-manipulation,bugprone-use-after-move,bugprone-suspicious-memory-comparison,bugprone-suspicious-memset-usage,bugprone-suspicious-missing-comma,bugprone-string-constructor,bugprone-sizeof-expression,performance-*,portability-*'
mapfile -t files < <(git diff --name-only --diff-filter=ACMR "$base" HEAD -- \
	'*.cc' '*.cpp')

if ((${#files[@]} == 0)); then
	exit 0
fi

clang-tidy -p "$build_dir" --checks="$checks" --header-filter='^(apps|libs|tests)/' \
	--warnings-as-errors='*' "${files[@]}"
