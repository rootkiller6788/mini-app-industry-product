# mini-scientific-research Makefile (C99)
CC       = gcc
CFLAGS   = -std=c99 -Wall -Wextra -pedantic -Iinclude
LDFLAGS  = -lm

SRCS = src/sci_numeric.c src/sci_statistics.c src/sci_optimize.c src/sci_linearalgebra.c src/sci_research.c
OBJS = $(SRCS:src/%.c=build/%.o)

.PHONY: all clean test

all: build/libsciresearch.a

build:
	mkdir -p build

build/%.o: src/%.c include/*.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/libsciresearch.a: $(OBJS)
	ar rcs $@ $^

test: build/libsciresearch.a tests/test_sci.c
	@$(CC) $(CFLAGS) -o build/test_sci tests/test_sci.c $(SRCS) $(LDFLAGS)
	@./build/test_sci
	@echo ""
	@echo "ALL TESTS PASSED"

clean:
	rm -rf build
