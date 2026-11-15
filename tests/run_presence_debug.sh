#!/bin/sh
set -eu
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/decaflash-presence-debug.XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror \
  -I "$repo_dir/apps/mainframe/src" "$repo_dir/tests/presence_debug.cpp" \
  "$repo_dir/apps/mainframe/src/presence_debug.cpp" -o "$test_dir/presence_debug"
"$test_dir/presence_debug"
