#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$ROOT_DIR"

if [[ ! -x "./build/basic" ]]; then
  make build/basic >/dev/null
fi

tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

run_parser_fuzz() {
  local seed="$1"
  local input_file="$tmp_dir/fuzz_${seed}.in"

  python3 - "$seed" >"$input_file" <<'PY'
import random
import sys

seed = int(sys.argv[1])
random.seed(seed)
ops = ["+", "-", "*", "/", "^", "=", "<", ">", "<=", ">=", "<>"]
idents = ["A", "B", "C", "X$", "Y", "LEFT$", "MID$", "RIGHT$", "LEN", "RND"]

def atom():
    kind = random.randint(0, 4)
    if kind == 0:
        return str(random.randint(-50, 50))
    if kind == 1:
        return f'"S{random.randint(0, 999)}"'
    if kind == 2:
        return random.choice(idents)
    if kind == 3:
        return "(" + expr(0) + ")"
    return random.choice([".", "@", "#", "\\", "??"])

def expr(depth):
    if depth > 2:
        return atom()
    out = atom()
    for _ in range(random.randint(0, 3)):
        out += " " + random.choice(ops) + " " + atom()
    return out

for _ in range(400):
    stmt_kind = random.randint(0, 4)
    if stmt_kind == 0:
        print("PRINT " + expr(0))
    elif stmt_kind == 1:
        print("LET A = " + expr(0))
    elif stmt_kind == 2:
        print("IF " + expr(0) + " THEN PRINT " + expr(0))
    elif stmt_kind == 3:
        print("REM " + expr(0))
    else:
        # raw tokenizer junk in immediate mode
        junk = ''.join(random.choice('ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789+-*/^<>=() :,;\"\\@#?') for _ in range(32))
        print(junk)
PY

  ./build/basic <"$input_file" >/dev/null
}

run_memory_stress() {
  local input_file="$tmp_dir/memory_stress.in"

  {
    for i in $(seq 1 2500); do
      printf "%d LET A=%d\n" "$i" "$i"
    done
    echo "RUN"
    echo "NEW"
    for i in $(seq 1 2500); do
      printf "%d DATA %d\n" "$i" "$i"
    done
    echo "10 READ X"
    echo "20 IF X<2500 THEN GOTO 10"
    echo "RUN"
  } >"$input_file"

  ./build/basic <"$input_file" >/dev/null
}

run_parser_fuzz 101
run_parser_fuzz 202
run_parser_fuzz 303
run_memory_stress

echo "fuzz_stress: ok"
