#!/bin/sh
# Runs every host-side test script in tests/ and reports a summary.
# Exit status is non-zero if any test fails.
set -u
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
passed=0
failed=0
failed_names=""
for script in "$repo_dir"/tests/run_*.sh; do
  name=$(basename "$script")
  [ "$name" = "run_all.sh" ] && continue
  if output=$(sh "$script" 2>&1); then
    passed=$((passed + 1))
    printf 'PASS  %s\n' "$name"
  else
    failed=$((failed + 1))
    failed_names="$failed_names $name"
    printf 'FAIL  %s\n' "$name"
    printf '%s\n' "$output" | tail -n 20 | sed 's/^/      /'
  fi
done
printf '\n%d passed, %d failed\n' "$passed" "$failed"
if [ "$failed" -ne 0 ]; then
  printf 'Failed:%s\n' "$failed_names"
  exit 1
fi
