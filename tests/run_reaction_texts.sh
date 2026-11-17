#!/bin/sh
set -eu
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/decaflash-reaction-texts.XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror \
  -I "$repo_dir/apps/mainframe/src" \
  "$repo_dir/tests/reaction_texts.cpp" \
  "$repo_dir/apps/mainframe/src/reaction_texts.cpp" \
  "$repo_dir/apps/mainframe/src/message_panel.cpp" \
  "$repo_dir/apps/mainframe/src/panel_glyphs.cpp" -o "$test_dir/reaction_texts"
"$test_dir/reaction_texts"
