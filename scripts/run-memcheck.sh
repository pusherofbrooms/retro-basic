#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$ROOT_DIR"

: "${VALGRIND:=valgrind}"

if ! command -v "$VALGRIND" >/dev/null 2>&1; then
  echo "$VALGRIND not found"
  exit 1
fi

failures=0

run_case() {
  local name="$1"
  local input_file="$2"
  local expected_file="$3"
  local output_file

  output_file=$(mktemp)
  if ! "$VALGRIND" --leak-check=full --error-exitcode=1 --quiet ./build/basic < "$input_file" > "$output_file"; then
    echo "[FAIL] $name (memcheck)"
    failures=$((failures + 1))
    rm -f "$output_file"
    return
  fi

  if ! diff -u "$expected_file" "$output_file"; then
    echo "[FAIL] $name (output mismatch)"
    failures=$((failures + 1))
  else
    echo "[PASS] $name"
  fi

  rm -f "$output_file"
}

run_case "basic_flow" "tests/cases/basic_flow.in" "tests/cases/basic_flow.out"
run_case "data_read" "tests/cases/data_read.in" "tests/cases/data_read.out"
run_case "load_program" "tests/cases/load_program.in" "tests/cases/load_program.out"

if [[ $failures -ne 0 ]]; then
  echo "$failures memcheck test(s) failed."
  exit 1
fi

echo "Memcheck tests passed."
