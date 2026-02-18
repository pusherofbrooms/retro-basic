# C64-Inspired BASIC Interpreter

This is a pragmatic, C64-inspired BASIC interpreter written in C. It supports line-numbered programs, immediate mode, classic control flow, and a plain-text `LOAD` command.

## Build

```sh
make build/basic
```

The binary will be at `build/basic`.

### Nix

```sh
nix develop
make build/basic
```

Or build/package directly with:

```sh
nix build
./result/bin/basic
```

## Run

```sh
./build/basic
```

### Example (interactive)

```
10 LET A = 2+3*4
20 PRINT A
30 FOR I = 1 TO 3
40 PRINT I;
50 NEXT I
60 PRINT ""
RUN
```

### Example (LOAD)

```
LOAD "tests/fixtures/sample.bas"
RUN
```

## Features (with short examples)

- Line-numbered program mode: `10 PRINT "HELLO"`
- Immediate mode: `PRINT 2+2`
- Assignment (`LET` optional): `LET A = 10` / `A = 10`
- PRINT with separators: `PRINT "A"; 1, 2`
- INPUT: `INPUT NAME$`
- IF/THEN (line target): `IF A > 3 THEN 100`
- IF/THEN (inline statement): `IF A > 3 THEN PRINT "OK"`
- GOTO: `GOTO 200`
- GOSUB/RETURN: `GOSUB 900` / `RETURN`
- FOR/NEXT: `FOR I = 1 TO 3 : PRINT I : NEXT I`
- DATA/READ/RESTORE: `DATA 1, "HI" : READ A, B$ : RESTORE`
- DIM arrays: `DIM A(10) : A(3) = 7`
- Numeric functions: `PRINT ABS(-2)`, `PRINT SQR(9)`
- String functions: `PRINT LEFT$("HELLO", 2)`, `PRINT LEN("HI")`
- Program control: `RUN`, `LIST`, `NEW`, `END`, `STOP`
- LOAD from file: `LOAD "path/to/program.bas"`

## Tests

```sh
scripts/run-tests.sh
```

Static analysis:

```sh
make lint
```

Valgrind memcheck path:

```sh
make test-mem
```

## Notes

- Plain-text program files only (no tokenized storage).
- Variables are multi-letter and case-insensitive; `$` suffix denotes strings.
- `RUN` resets variables and arrays before execution (C64-style) while keeping the stored program.
