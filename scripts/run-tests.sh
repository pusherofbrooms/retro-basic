#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$ROOT_DIR"

found_tests=0

run_test() {
  found_tests=1
  echo "==> $*"
  "$@"
}

ran_tests_run=0
if [[ -x "./tests/run.sh" ]]; then
  run_test "./tests/run.sh"
  ran_tests_run=1
fi

if [[ -d "./tests" ]]; then
  for test_file in ./tests/*.sh; do
    if [[ -f "$test_file" && -x "$test_file" ]]; then
      if [[ $ran_tests_run -eq 1 && "$test_file" == "./tests/run.sh" ]]; then
        continue
      fi
      run_test "$test_file"
    fi
  done
fi

if [[ -x "./build/tests" ]]; then
  run_test "./build/tests"
fi

if [[ $found_tests -eq 0 ]]; then
  echo "No tests found. Add tests in ./tests or a Makefile test target."
fi
