#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$ROOT_DIR"

make build/basic

failures=0

run_case() {
  local name="$1"
  local input_file="$2"
  local expected_file="$3"
  local output

  output=$(./build/basic < "$input_file")
  if ! diff -u "$expected_file" - <<<"$output"; then
    echo "[FAIL] $name"
    failures=$((failures + 1))
  else
    echo "[PASS] $name"
  fi
}

run_case "basic_flow" "tests/cases/basic_flow.in" "tests/cases/basic_flow.out"
run_case "data_read" "tests/cases/data_read.in" "tests/cases/data_read.out"
run_case "load_program" "tests/cases/load_program.in" "tests/cases/load_program.out"

if [[ $failures -ne 0 ]]; then
  echo "$failures test(s) failed."
  exit 1
fi

echo "All tests passed."
