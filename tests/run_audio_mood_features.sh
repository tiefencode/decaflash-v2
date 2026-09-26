#!/bin/sh
set -eu
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/decaflash-audio-mood.XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror \
  -I "$repo_dir/apps/mainframe/src" "$repo_dir/tests/audio_mood_features.cpp" \
  "$repo_dir/apps/mainframe/src/audio_mood_features.cpp" -o "$test_dir/audio_mood_features"
"$test_dir/audio_mood_features"
