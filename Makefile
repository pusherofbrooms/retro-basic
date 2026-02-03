CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -Werror -O2
LDFLAGS ?= -lm

SRC = src/main.c src/basic.c
OBJ = build/main.o build/basic.o

build/basic: $(OBJ)
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS)

build/%.o: src/%.c
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

test: build/basic
	./scripts/run-tests.sh

clean:
	rm -rf build

.PHONY: test clean
