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

run_case_env() {
  local name="$1"
  local env_var="$2"
  local input_file="$3"
  local expected_file="$4"
  local output

  output=$(env "$env_var" ./build/basic < "$input_file")
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
run_case "load_malformed" "tests/cases/load_malformed.in" "tests/cases/load_malformed.out"
run_case "load_overlong" "tests/cases/load_overlong.in" "tests/cases/load_overlong.out"
run_case "merge_program" "tests/cases/merge_program.in" "tests/cases/merge_program.out"
run_case "save_program" "tests/cases/save_program.in" "tests/cases/save_program.out"
run_case "list_ranges" "tests/cases/list_ranges.in" "tests/cases/list_ranges.out"
run_case "comments" "tests/cases/comments.in" "tests/cases/comments.out"
run_case "arg_overflow" "tests/cases/arg_overflow.in" "tests/cases/arg_overflow.out"
run_case "assignment_consistency" "tests/cases/assignment_consistency.in" "tests/cases/assignment_consistency.out"
run_case "builtin_matrix" "tests/cases/builtin_matrix.in" "tests/cases/builtin_matrix.out"
run_case "dispatch_coverage" "tests/cases/dispatch_coverage.in" "tests/cases/dispatch_coverage.out"
run_case "exponent_assoc" "tests/cases/exponent_assoc.in" "tests/cases/exponent_assoc.out"
run_case "for_validation" "tests/cases/for_validation.in" "tests/cases/for_validation.out"
run_case "arrays_2d" "tests/cases/arrays_2d.in" "tests/cases/arrays_2d.out"
run_case "malformed_trailing" "tests/cases/malformed_trailing.in" "tests/cases/malformed_trailing.out"
run_case "repl_overlong_line" "tests/cases/repl_overlong_line.in" "tests/cases/repl_overlong_line.out"
run_case_env "error_context" "BASIC_ERROR_CONTEXT=1" "tests/cases/error_context.in" "tests/cases/error_context.out"
run_case "line_tokenize_validation" "tests/cases/line_tokenize_validation.in" "tests/cases/line_tokenize_validation.out"
run_case "delete_find" "tests/cases/delete_find.in" "tests/cases/delete_find.out"
run_case "boolean_ops" "tests/cases/boolean_ops.in" "tests/cases/boolean_ops.out"
run_case "if_else" "tests/cases/if_else.in" "tests/cases/if_else.out"
run_case "input_prompt" "tests/cases/input_prompt.in" "tests/cases/input_prompt.out"
run_case "print_tab_spc" "tests/cases/print_tab_spc.in" "tests/cases/print_tab_spc.out"
run_case "run_reset" "tests/cases/run_reset.in" "tests/cases/run_reset.out"
run_case "rnd_randomize" "tests/cases/rnd_randomize.in" "tests/cases/rnd_randomize.out"
run_case "on_dispatch" "tests/cases/on_dispatch.in" "tests/cases/on_dispatch.out"
run_case "def_fn" "tests/cases/def_fn.in" "tests/cases/def_fn.out"
run_case "tracing" "tests/cases/tracing.in" "tests/cases/tracing.out"
run_case "cont" "tests/cases/cont.in" "tests/cases/cont.out"
run_case "renum" "tests/cases/renum.in" "tests/cases/renum.out"
run_case "inkey_getkey" "tests/cases/inkey_getkey.in" "tests/cases/inkey_getkey.out"
run_case "manual_tutorial" "tests/cases/manual_tutorial.in" "tests/cases/manual_tutorial.out"
run_case_env "manual_screen" "BASIC_SCREEN_FALLBACK=1" "tests/cases/manual_screen.in" "tests/cases/manual_screen.out"
run_case "screen_cmds" "tests/cases/screen_cmds.in" "tests/cases/screen_cmds.out"
run_case_env "screen_fallback" "BASIC_SCREEN_FALLBACK=1" "tests/cases/screen_fallback.in" "tests/cases/screen_fallback.out"

if [[ $failures -ne 0 ]]; then
  echo "$failures test(s) failed."
  exit 1
fi

echo "All tests passed."
