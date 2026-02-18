# The Little BASIC Book (C64 Style)

This is a friendly guide for learning this BASIC interpreter.
If you are brand new, start at Lesson 1 and type exactly what you see.

Build and start BASIC first:

```sh
make build/basic
./build/basic
```

## How to read this guide

- `YOU TYPE` means the lines you enter.
- `YOU SEE` means the output BASIC prints.
- Line numbers (like `10`, `20`, `30`) are program steps.
- `RUN` starts your stored program.
- `NEW` clears your stored program.

## Lesson 1: Your first commands

### YOU TYPE
```basic
PRINT "HELLO, PROGRAMMER!"
PRINT 2+3*4
```

### YOU SEE
```text
HELLO, PROGRAMMER!
14
```

Why `14`? Multiplication happens before addition.

## Lesson 2: Your first real program

### YOU TYPE
```basic
NEW
10 FOR I=1 TO 5
20 PRINT I;
30 NEXT I
40 PRINT ""
RUN
```

### YOU SEE
```text
12345
```

`FOR`/`NEXT` repeats the block. `PRINT I;` keeps printing on the same line.

## Lesson 3: Variables and decisions

### YOU TYPE
```basic
NEW
10 SCORE=7
20 IF SCORE>=5 THEN PRINT "LEVEL UP"
RUN
```

### YOU SEE
```text
LEVEL UP
```

Variables remember values, and `IF` lets your program make choices.

## Lesson 4: Subroutines (`GOSUB` / `RETURN`)

### YOU TYPE
```basic
NEW
10 PRINT "START"
20 GOSUB 100
30 PRINT "END"
40 END
100 PRINT "IN SUBROUTINE"
110 RETURN
RUN
```

### YOU SEE
```text
START
IN SUBROUTINE
END
```

Think of a subroutine as a mini-program you can jump into, then come back from.

## Lesson 5: DATA and READ

### YOU TYPE
```basic
NEW
10 DATA 3,4,5
20 READ A,B,C
30 PRINT A+B+C
40 RESTORE
50 READ A
60 PRINT A
RUN
```

### YOU SEE
```text
12
3
```

`DATA` stores values in your program. `READ` pulls them out in order. `RESTORE` rewinds.

## Lesson 6: Arrays

### YOU TYPE
```basic
NEW
10 DIM N(3)
20 N(1)=10
30 N(2)=20
40 N(3)=N(1)+N(2)
50 PRINT N(3)
RUN
```

### YOU SEE
```text
30
```

`DIM` creates an array. Here we store several numbers under one name (`N`).

## Lesson 7: Make your own function

### YOU TYPE
```basic
NEW
10 DEF FNSQ(X)=X*X
20 PRINT FNSQ(6)
RUN
```

### YOU SEE
```text
36
```

`DEF FN...` defines your own reusable math function.

## Lesson 8: String powers

### YOU TYPE
```basic
PRINT LEFT$("COMMODORE",4)
PRINT MID$("COMMODORE",5,3)
PRINT RIGHT$("COMMODORE",4)
PRINT CHR$(65)
PRINT ASC("Z")
```

### YOU SEE
```text
COMM
ODO
DORE
A
90
```

These let you cut text apart and convert letters to numbers (and back).

## Lesson 9: Random numbers you can repeat

### YOU TYPE
```basic
RANDOMIZE 42
PRINT RND(1)
PRINT RND(0)
```

### YOU SEE
```text
0.252345
0.252345
```

`RANDOMIZE 42` gives a repeatable sequence. `RND(0)` repeats the previous random value.

## Lesson 10: Save and load a program

### YOU TYPE
```basic
SAVE "build/manual-save.bas"
NEW
LOAD "build/manual-save.bas"
LIST
RUN
```

### YOU SEE
```text
10 DEF FNSQ(X)=X*X
20 PRINT FNSQ(6)
36
```

Now your program lives in a file and can come back later.

## Lesson 11: Ask the player for input

### YOU TYPE
```basic
INPUT NAME$
Ada
PRINT "HI ";NAME$
```

### YOU SEE
```text
? HI Ada
```

`INPUT` prints `? ` and waits for a line of text.

## Lesson 12: Screen drawing (like old-school graphics)

These commands draw on the virtual screen:

- `CLS` clears the screen.
- `COLOR` picks foreground/background colors.
- `PRINT@ row,column,text` prints at a position.
- `PLOT row,column,ch` places one character.
- `LINE row0,col0,row1,col1,ch` draws a line.

### YOU TYPE
```basic
NEW
10 CLS
20 COLOR 2,4
30 PRINT@ 2,3,"HI"
40 PLOT 2,4,"X"
50 LINE 3,1,3,6,"-"
RUN
```

### YOU SEE (fallback mode)
```text
SCREEN 2   HI
SCREEN 2   HX
SCREEN 3 ------
```

If you run in a real ANSI terminal, the screen drawing appears visually instead of `SCREEN ...` lines.

## Tiny challenge set

Try these next:

1. Change Lesson 2 to count backward with `STEP -1`.
2. Make Lesson 3 print a different message for scores under 5.
3. Turn Lesson 4 into a 3-question quiz with `INPUT` and `IF`.
4. Save your own game to a `.bas` file and reload it.

## Verified examples

All `YOU TYPE` and `YOU SEE` examples in this manual were verified against this interpreter with:

```sh
./build/basic < tests/cases/manual_tutorial.in
BASIC_SCREEN_FALLBACK=1 ./build/basic < tests/cases/manual_screen.in
```

and checked by the automated test case:

- `tests/cases/manual_tutorial.in`
- `tests/cases/manual_tutorial.out`
- `tests/cases/manual_screen.in` (with `BASIC_SCREEN_FALLBACK=1`)
- `tests/cases/manual_screen.out`
