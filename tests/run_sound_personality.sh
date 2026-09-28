#!/bin/sh
set -eu
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/decaflash-sound-personality.XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror \
  -I "$repo_dir/apps/mainframe/src" "$repo_dir/tests/sound_personality.cpp" \
  "$repo_dir/apps/mainframe/src/sound_personality.cpp" -o "$test_dir/personality"
"$test_dir/personality"
