#!/bin/sh
set -eu
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/decaflash-bpm-trace.XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror \
  -I "$repo_dir/apps/mainframe/src" \
  "$repo_dir/apps/mainframe/src/bpm_tracker.cpp" \
  "$repo_dir/tests/bpm_trace_report.cpp" \
  -o "$test_dir/bpm_trace_report"
"$test_dir/bpm_trace_report" "$repo_dir/tests/fixtures/bpm/atom-98.csv" 98
"$test_dir/bpm_trace_report" "$repo_dir/tests/fixtures/bpm/atom-100.csv" 100
"$test_dir/bpm_trace_report" "$repo_dir/tests/fixtures/bpm/atom-120.csv" 120
"$test_dir/bpm_trace_report" "$repo_dir/tests/fixtures/bpm/atom-160.csv" 160
"$test_dir/bpm_trace_report" "$repo_dir/tests/fixtures/bpm/atom-180.csv" 180
