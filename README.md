# C64-Inspired BASIC Interpreter

This is a pragmatic, C64-inspired BASIC interpreter written in C. It supports line-numbered programs, immediate mode, classic control flow, and a plain-text `LOAD` command.

Want a kid-friendly, C64-manual-style walkthrough? See [`MANUAL.md`](MANUAL.md).

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

### Example (Screen Mode)

```
LOAD "tests/fixtures/screen_demo.bas"
RUN
```

The demo program uses `CLS`, `LOCATE`, `COLOR`, `PRINT@`, `PLOT`, `LINE`, and `GETKEY$`.

## Features (with short examples)

- Line-numbered program mode: `10 PRINT "HELLO"`
- Immediate mode: `PRINT 2+2`
- Assignment (`LET` optional): `LET A = 10` / `A = 10`
- PRINT with separators/helpers: `PRINT "A"; 1, 2`, `PRINT "A";TAB(10);"B"`, `PRINT "X";SPC(3);"Y"`
- INPUT: `INPUT NAME$` and prompt form `INPUT "NAME? "; NAME$`
- IF/THEN (line target): `IF A > 3 THEN 100`
- IF/THEN/ELSE: `IF A > 3 THEN PRINT "OK" ELSE PRINT "NO"`
- GOTO: `GOTO 200`
- GOSUB/RETURN: `GOSUB 900` / `RETURN`
- ON dispatch: `ON X GOTO 100,200` / `ON X GOSUB 500,600`
- User functions: `DEF FNX(A)=A*2 : PRINT FNX(5)`
- FOR/NEXT: `FOR I = 1 TO 3 : PRINT I : NEXT I`
- DATA/READ/RESTORE: `DATA 1, "HI" : READ A, B$ : RESTORE`
- Comments: `REM THIS IS A COMMENT` and `' SHORTHAND COMMENT`
- DIM arrays (1D/2D): `DIM A(10) : A(3) = 7`, `DIM M(2,3) : M(1,2)=42`
- Numeric/boolean expressions: `PRINT ABS(-2)`, `PRINT SQR(9)`, `PRINT NOT 0`, `PRINT 1 AND 0`, `PRINT 1 OR 0`
- Random control: `RANDOMIZE 42 : PRINT RND(1), RND(0), RND(-7)`
- String functions: `PRINT LEFT$("HELLO", 2)`, `PRINT LEN("HI")`
- Program control: `RUN`, `CONT`, `LIST`, `NEW`, `END`, `STOP`
- LIST ranges: `LIST 100-200`, `LIST 100-`, `LIST -200`
- Program editing helpers: `DELETE 100-200`, `FIND "PRINT"`, `RENUM`, `RENUM 100,5`
- Tracing: `TRON` / `TROFF`
- LOAD from file (replace current program): `LOAD "path/to/program.bas"`
- MERGE from file (insert/replace matching lines): `MERGE "path/to/program.bas"`
- SAVE to file: `SAVE "path/to/program.bas"`
- Key input functions: `INKEY$()` (non-blocking), `GETKEY$()` (blocking with line-input fallback)
- Screen commands: `CLS`, `LOCATE 5,10`, `COLOR 14,4`, `PRINT@ 3,2,"HI"`, `PLOT 10,20,"*"`, `LINE 2,2,2,20,"-"`

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

On macOS (Darwin), `make test-mem` is skipped with a message because this workflow does not support valgrind there.

## Notes

- Plain-text program files only (no tokenized storage).
- Variables are multi-letter and case-insensitive; `$` suffix denotes strings.
- `RUN` resets variables and arrays before execution (C64-style) while keeping the stored program.
- Screen coordinates are 1-based integers: row 1, column 1 is top-left.
- Program source line length limit: 255 characters per line.
- Optional error context: set `BASIC_ERROR_CONTEXT=1` to append token column info (for example, `SYNTAX ERROR IN 100 AT COLUMN 9`).
- Screen output modes:
  - ANSI-capable TTY: cursor/color drawing is rendered directly.
  - Non-ANSI or redirected output: screen redraw is suppressed by default.
  - Optional fallback: set `BASIC_SCREEN_FALLBACK=1` to emit row diffs as `SCREEN <row> <text>`.
