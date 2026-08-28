#!/usr/bin/env bash
set -euo pipefail

base=${1:?missing base revision}
script=$(find /usr -path '*clang-format-diff.py' -print -quit)
style='{BasedOnStyle: LLVM, UseTab: Always, IndentWidth: 8, TabWidth: 8, ColumnLimit: 80, BreakBeforeBraces: Linux, AllowShortBlocksOnASingleLine: Never, AllowShortCaseLabelsOnASingleLine: false, AllowShortFunctionsOnASingleLine: None, AllowShortIfStatementsOnASingleLine: Never, AllowShortLoopsOnASingleLine: false, IndentCaseLabels: false, PointerAlignment: Right, SortIncludes: Never, SpaceBeforeParens: ControlStatements, SpacesBeforeTrailingComments: 1}'

if [[ -z "$script" ]]; then
	echo 'clang-format-diff.py was not found' >&2
	exit 1
fi

diff_file=$(mktemp)
git diff -U0 "$base" HEAD -- '*.c' '*.cc' '*.cpp' '*.h' >"$diff_file"
if [[ ! -s "$diff_file" ]]; then
	exit 0
fi

if python3 "$script" -p1 -style="$style" <"$diff_file" | tee /dev/stderr | grep -q .; then
	exit 1
fi
