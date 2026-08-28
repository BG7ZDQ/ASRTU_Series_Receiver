#!/usr/bin/env bash
set -euo pipefail

default_branch=${DEFAULT_BRANCH:?missing default branch}
base_ref=${GITHUB_BASE_REF:-$default_branch}

git fetch --no-tags origin "$base_ref"
git merge-base HEAD FETCH_HEAD
