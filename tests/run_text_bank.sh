#!/bin/sh
# Text bank (add-text-bank): the generated table matches the corpus, the
# selection code passes its scenarios, and the hand try-out tool builds and runs.
set -eu
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
python3 -I "$repo_dir/apps/mainframe/tools/text_bank.py" --check
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/decaflash-text-bank.XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror \
  -I "$repo_dir/apps/mainframe/src" \
  "$repo_dir/tests/text_bank.cpp" \
  "$repo_dir/apps/mainframe/src/text_bank.cpp" \
  "$repo_dir/apps/mainframe/src/panel_glyphs.cpp" -o "$test_dir/text_bank"
"$test_dir/text_bank"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror \
  -I "$repo_dir/apps/mainframe/src" \
  "$repo_dir/apps/mainframe/tools/text_bank_try.cpp" \
  "$repo_dir/apps/mainframe/src/text_bank.cpp" -o "$test_dir/text_bank_try"
printf '2\n30 90 50 40 60\nq\n' | "$test_dir/text_bank_try" > "$test_dir/try.txt"
grep -q "ANNOYANCE_HIGH" "$test_dir/try.txt"
