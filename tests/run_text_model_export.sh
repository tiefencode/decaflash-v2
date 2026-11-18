#!/bin/sh
# Exporter check of the text model (add-text-model, task 4.2): trains a tiny
# model on today's fixed texts, exports it and compares the host kernel with
# the Python twin of the kernel's arithmetic. Needs the training environment
# /.venv-text-model/ (apps/mainframe/tools/text_model_requirements.txt); without
# it the test is skipped and says so.
set -eu
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
python="$repo_dir/.venv-text-model/bin/python"
if [ ! -x "$python" ]; then
  echo "SKIP: no .venv-text-model, exporter check not run"
  exit 0
fi
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/decaflash-text-model-export.XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
tools="$repo_dir/apps/mainframe/tools"
"${CXX:-c++}" -std=c++17 -O2 -Wall -Wextra -Werror -I "$repo_dir/apps/mainframe/src" \
  "$tools/text_model_generate.cpp" -o "$test_dir/text_model_generate"
"$python" -I "$tools/text_model.py" train --layers 2 --dim 32 --seed 7 --steps 400 \
  --smoke --out "$test_dir/tiny.pt" > "$test_dir/train.log"
"$python" -I "$tools/text_model_export.py" "$test_dir/tiny.pt" "$test_dir/tiny.bin" > /dev/null
"$python" -I "$tools/text_model_check.py" "$test_dir/tiny.pt" "$test_dir/tiny.bin" \
  "$test_dir/text_model_generate"
