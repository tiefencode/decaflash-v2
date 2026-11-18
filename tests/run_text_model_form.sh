#!/bin/sh
set -eu
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/decaflash-text-model-form.XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror \
  -I "$repo_dir/apps/mainframe/src" -I "$repo_dir/apps/mainframe/tools" \
  "$repo_dir/tests/text_model_form.cpp" \
  "$repo_dir/apps/mainframe/src/message_panel.cpp" \
  "$repo_dir/apps/mainframe/src/panel_glyphs.cpp" -o "$test_dir/text_model_form"
"$test_dir/text_model_form"
# The command-line tool builds and reports a failing line.
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror \
  -I "$repo_dir/apps/mainframe/src" -I "$repo_dir/apps/mainframe/tools" \
  "$repo_dir/apps/mainframe/tools/text_model_form_check.cpp" -o "$test_dir/form_check"
printf '\360\237\231\202 HUHU\nhuhu\n' > "$test_dir/texts.txt"
if "$test_dir/form_check" "$test_dir/texts.txt" > "$test_dir/out.txt"; then
  echo "form check accepted a bad line"; exit 1
fi
grep -q "FAIL line 2" "$test_dir/out.txt"
grep -q "2 texts, 1 failed" "$test_dir/out.txt"
