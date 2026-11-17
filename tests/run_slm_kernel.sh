#!/bin/sh
set -eu
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/decaflash-v2-slm-kernel.XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror \
  "$repo_dir/apps/mainframe/tools/slm_make_image.cpp" \
  -o "$test_dir/slm_make_image"
# Tiny model: dim 16, 2 layers, 2 heads, ffn 32, ple 8, vocab 40 in / 20 out,
# 24 positions, group 8. Written twice to check that a seed gives one file.
"$test_dir/slm_make_image" "$test_dir/tiny.bin" 1 16 2 2 32 8 40 20 24 8 >/dev/null
"$test_dir/slm_make_image" "$test_dir/tiny_again.bin" 1 16 2 2 32 8 40 20 24 8 >/dev/null
cmp "$test_dir/tiny.bin" "$test_dir/tiny_again.bin"
"$test_dir/slm_make_image" "$test_dir/other_seed.bin" 2 16 2 2 32 8 40 20 24 8 >/dev/null
if cmp -s "$test_dir/tiny.bin" "$test_dir/other_seed.bin"; then
  echo "different seeds gave the same image" >&2
  exit 1
fi
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror \
  -I "$repo_dir/apps/mainframe/src" \
  "$repo_dir/tests/slm_kernel.cpp" \
  -o "$test_dir/slm_kernel"
"$test_dir/slm_kernel" "$test_dir/tiny.bin"
