#!/bin/sh
set -eu
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
python3 -I "$repo_dir/apps/mainframe/tools/text_model_vocab.py" --check
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/decaflash-text-model-vocab.XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror \
  -I "$repo_dir/apps/mainframe/src" \
  "$repo_dir/tests/text_model_vocab.cpp" \
  "$repo_dir/apps/mainframe/src/panel_glyphs.cpp" -o "$test_dir/text_model_vocab"
"$test_dir/text_model_vocab"
