CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -Werror -O2
LDFLAGS ?= -lm
CLANG_TIDY ?= clang-tidy
VALGRIND ?= valgrind

SRC = src/main.c src/basic.c src/terminal.c src/screen.c
OBJ = build/main.o build/basic.o build/terminal.o build/screen.o
DEP = $(OBJ:.o=.d)

build/basic: $(OBJ)
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS)

build/%.o: src/%.c
	@mkdir -p build
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

test: build/basic
	./scripts/run-tests.sh

lint:
	@command -v $(CLANG_TIDY) >/dev/null 2>&1 || { echo "$(CLANG_TIDY) not found"; exit 1; }
	$(CLANG_TIDY) $(SRC) -- -std=c99 -Isrc

test-mem: build/basic
	@VALGRIND="$(VALGRIND)" ./scripts/run-memcheck.sh

clean:
	rm -rf build

.PHONY: test lint test-mem clean

-include $(DEP)
