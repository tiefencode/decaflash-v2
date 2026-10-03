#!/bin/sh
set -eu
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/decaflash-spectral-onset.XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror \
  -I "$repo_dir/apps/mainframe/src" \
  "$repo_dir/apps/mainframe/src/spectral_onset_features.cpp" \
  "$repo_dir/tests/spectral_onset_features.cpp" \
  -o "$test_dir/spectral_onset_features"
"$test_dir/spectral_onset_features"
