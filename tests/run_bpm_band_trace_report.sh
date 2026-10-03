#!/bin/sh
set -eu
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/decaflash-bpm-band-trace.XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror \
  -I "$repo_dir/apps/mainframe/src" \
  "$repo_dir/apps/mainframe/src/bpm_tracker.cpp" \
  "$repo_dir/tests/bpm_band_trace_report.cpp" \
  -o "$test_dir/bpm_band_trace_report"
for bpm in 98 100 120 160 180; do
  "$test_dir/bpm_band_trace_report" "$repo_dir/tests/fixtures/bpm-bands/atom-$bpm.csv" "$bpm"
done
