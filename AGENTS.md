# Project Instructions: retro-basic

C64-inspired BASIC interpreter in C. Preserve the classic BASIC feel while keeping behavior explicit, testable, and maintainable.

## Product Scope

- Dialect: pragmatic C64-style BASIC, not full system emulation.
- Program storage: plain-text, line-numbered source; no PRG/tokenized files.
- Variables: case-insensitive multi-letter names; `$` suffix means string.
- Numbers: `float`-style numeric values; integers where required for indices, line numbers, and screen coordinates.
- No hardware emulation: avoid `PEEK`, `POKE`, `SYS`, `WAIT`, etc.
- File commands operate on local plain-text BASIC files.

## Current Feature Surface

Keep README examples and tests aligned when changing any of these areas:

- Program control: `RUN`, `CONT`, `LIST`, `NEW`, `END`, `STOP`, `DELETE`, `FIND`, `RENUM`, `TRON`, `TROFF`.
- Flow: `IF/THEN/ELSE`, `GOTO`, `GOSUB`, `RETURN`, `ON ... GOTO/GOSUB`, `FOR/NEXT`.
- I/O: `PRINT`, `INPUT`, `INKEY$()`, `GETKEY$()`.
- Data: `DATA`, `READ`, `RESTORE`.
- Variables/arrays: `LET` optional, implicit assignment, `DIM` 1D/2D arrays.
- User functions: `DEF FN...`.
- Files: `LOAD`, `MERGE`, `SAVE`.
- Screen commands: `CLS`, `LOCATE`, `COLOR`, `PRINT@`, `PLOT`, `LINE`.
- Numeric/string functions: keep parser, evaluator, and manual examples in sync.

## Implementation Guidelines

- Keep parsing/execution errors centralized and report program line numbers when available.
- Preserve immediate mode and line-numbered program mode.
- `RUN` executes stored lines in ascending order and resets runtime state while keeping the program.
- `GOTO`/`GOSUB` jump by BASIC line number, not source array index.
- Prefer small, direct C changes over broad rewrites.
- Keep memory ownership obvious; free strings, arrays, runtime stacks, and loaded program state on replacement paths.
- Update `README.md`, `MANUAL.md`, or fixtures when visible behavior changes.

## Build, Run, Test

Use the project targets:

- Build: `make build/basic`
- Run: `./build/basic`
- Tests: `scripts/run-tests.sh` or `make test`
- Lint: `make lint`
- Memory checks: `make test-mem`

Before finishing code changes, run the relevant tests. For behavior changes, run `scripts/run-tests.sh`; run `make lint` and `make test-mem` when practical or when touching C ownership/control-flow code.

## TDD Expectations

- Follow red → green → refactor for behavior changes.
- Add or update a failing test first.
- Prefer high-value golden I/O tests, then add focused unit/regression coverage as needed.
- Cover errors such as undefined lines, type mismatch, syntax errors, and out-of-data cases.
- Do not update expected output to hide regressions.

## Nix Discipline

This machine uses Nix with flakes.

- Use `nix develop --command ...`, `nix run ...`, `nix shell ... --command ...`, or `nix build`.
- Do not use `nix-env`, `nix-shell`, `nix-channel`, `nix profile`, or global installs.
- No interactive shells/PTY assumptions.
- If dependencies change, update `flake.nix`/`flake.lock` declaratively.

## Task Tracking With Beads

Use `bd --no-daemon ...` in agent sessions.

- Check health/location: `bd --no-daemon doctor`, `bd --no-daemon where`.
- Pick work: `bd --no-daemon ready`; inspect with `bd --no-daemon show <id>`.
- Create focused work items: `bd --no-daemon create "Title"`.
- Track dependencies with `bd --no-daemon dep add <blocked-id> <blocking-id>`.
- Close completed items only after tests/docs are done: `bd --no-daemon close <id>`.
- Avoid `bd init`; it rewrites `AGENTS.md`. If unavoidable, restore this file afterward.

Keep `.beads/issues.jsonl` tracked so bead sync remains reliable.
