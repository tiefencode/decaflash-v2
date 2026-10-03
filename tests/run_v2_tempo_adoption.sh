#!/bin/sh
set -eu
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/decaflash-v2-adoption.XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror \
  -I "$repo_dir/apps/mainframe/src" \
  "$repo_dir/tests/v2_tempo_adoption.cpp" \
  -o "$test_dir/v2_tempo_adoption"
"$test_dir/v2_tempo_adoption"
