# BASIC Implementation Plan (C64-Inspired)

This repository targets a C64-inspired BASIC interpreter written in C. The goal is to capture the feel of classic BASIC while keeping the implementation pragmatic and maintainable.

## Scope and Behavior
- Dialect: C64-style “rich” BASIC with practical simplifications
- Variables: multi-letter, case-insensitive; `$` suffix for strings
- Numeric model: 32-bit float for numbers; 32-bit int where needed (indices)
- Storage: plain-text program files (no PRG/tokenized storage)
- System emulation: none (no PEEK/POKE/SYS/WAIT)
- File I/O: `LOAD` reads a plain-text BASIC program from local filesystem

## Language Feature Set
Statements
- Program control: `RUN`, `LIST`, `NEW`, `END`, `STOP`
- Flow: `IF ... THEN`, `GOTO`, `GOSUB`, `RETURN`, `FOR`, `NEXT`
- I/O: `PRINT`, `INPUT`
- Data: `DATA`, `READ`, `RESTORE`
- Variables/arrays: `DIM`, `LET` (optional; implicit assignment)

Functions
- Numeric: `ABS`, `INT`, `RND`, `SIN`, `COS`, `TAN`, `SQR`, `LOG`, `EXP`, `ATN`, `SGN`
- String: `LEFT$`, `MID$`, `RIGHT$`, `LEN`, `ASC`, `CHR$`

## Execution Model
- REPL supports immediate mode and line-numbered program mode
- Program stored as ordered map: line number -> token list
- `RUN` executes lines in ascending order; `GOTO`/`GOSUB` jump by line number

## Data Model
- Types: number (`float`), string (char* with length), boolean via numeric truth
- Variables: map keyed by uppercase name; `$` suffix denotes string
- Arrays: allocated on `DIM`; map by name; start with 1D arrays

## Parser and Evaluator
- Tokenizer converts a line to tokens (keywords, identifiers, numbers, strings, operators)
- Expression parser uses shunting-yard or Pratt for precedence
- Store token lists per line and parse on execution initially

## Runtime Stacks
- `FOR` stack: control var, limit, step, return pointer
- `GOSUB` stack: return pointer
- `DATA` cursor per program run

## Error Handling
- Central error enum and classic messages
- Include line number for program errors

## File Handling
- `LOAD "path"` reads a plain-text file of line-numbered BASIC
- Replace program by default

## Phased Implementation
1. Tokenizer + line store + `LIST/NEW/RUN`
2. Expressions + variables + `PRINT/INPUT`
3. Flow control (`IF`, `GOTO`, `GOSUB`, `FOR/NEXT`)
4. `DATA/READ/RESTORE` + arrays
5. String + math functions
6. `LOAD` and file parsing
7. Error polish + tests

## Testing
- Bash test harness: `scripts/run-tests.sh` (runs any executable tests and reports when none exist)
- Tokenization and expression precedence unit tests
- Golden program output tests
- Error case coverage (undefined line, type mismatch, out of data)
- Static analysis: `make lint`
- Memory checks: `make test-mem`

## Build and Run
- Build: `make build/basic`
- Run: `./build/basic`
- Nix dev shell: `nix develop`
- Nix build: `nix build` (binary at `./result/bin/basic`)

## Task Tracking With Beads
Use `beads` (`bd`) as the project task tracker with dependency-aware issues.

### Setup
- Check installation health first: `bd doctor`
- Initialize in this repo (one-time): `bd init`
- Confirm active database location: `bd where`

### Daily Workflow
- Create a task: `bd create "Implement tokenizer line storage"`
- Quick-capture a task and return only ID: `bd q "Fix FOR/NEXT edge case"`
- List open work: `bd list`
- Show work ready to pick up (no blockers): `bd ready`
- Inspect one task: `bd show <id>`
- Update status/fields: `bd update <id> --status in_progress` (or other `bd update` flags)
- Use `bd set-state` only for dimensioned operational state labels, e.g. `bd set-state <id> mode=in_progress`
- Close when done: `bd close <id>`

### Dependencies and Planning
- Block one task on another: `bd dep add <blocked-id> <blocking-id>`
- Visualize dependency graph: `bd graph <issue-id>`
- Find currently blocked tasks: `bd blocked`

### Sync Prerequisite
- Ensure `.beads/issues.jsonl` is tracked by git (not ignored in global excludes or `.git/info/exclude`) before relying on `bd sync`.

### Recommended Conventions
- Keep one issue per concrete, testable change.
- Represent phase work (from the implementation plan above) as epics with child tasks.
- Encode ordering with dependencies instead of long checklist comments.
- Before starting new work, run `bd ready` and pick from unblocked tasks.

## Landing the Plane (Session Completion)

**When ending a work session**, you MUST complete ALL steps below. Work is NOT complete until `git push` succeeds.

**MANDATORY WORKFLOW:**

1. **File issues for remaining work** - Create issues for anything that needs follow-up
2. **Run quality gates** (if code changed) - Tests, linters, builds
3. **Update issue status** - Close finished work, update in-progress items
4. **PUSH TO REMOTE** - This is MANDATORY:
   ```bash
   git pull --rebase
   bd sync
   git push
   git status  # MUST show "up to date with origin"
   ```
   If no remote/upstream is configured yet, document this explicitly in handoff as `push blocked: no remote configured`, and stop after commit + clean `git status`.
5. **Clean up** - Clear stashes, prune remote branches
6. **Verify** - All changes committed AND pushed
7. **Hand off** - Provide context for next session

**CRITICAL RULES:**
- Work is NOT complete until `git push` succeeds
- NEVER stop before pushing - that leaves work stranded locally
- NEVER say "ready to push when you are" - YOU must push
- If push fails, resolve and retry until it succeeds
