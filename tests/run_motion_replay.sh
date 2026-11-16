#!/bin/sh
set -eu
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/decaflash-motion-replay.XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror \
  -I "$repo_dir/apps/mainframe/src" "$repo_dir/tests/motion_replay.cpp" \
  "$repo_dir/apps/mainframe/src/motion_events.cpp" -o "$test_dir/motion_replay"
"$test_dir/motion_replay" "$repo_dir/tests/fixtures/motion"
