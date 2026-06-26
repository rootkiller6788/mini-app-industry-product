# mini-smart-manufacturing Makefile (C99)
# Smart Manufacturing: MES, Scheduling, OEE, Quality, Supply Chain

CC       = gcc
CFLAGS   = -std=c99 -Wall -Wextra -pedantic -Iinclude
LDFLAGS  = -lm

SRCS = src/smart_manufacturing.c        src/production_sched.c        src/production_johnson.c        src/production_analysis.c        src/production_workorder.c        src/quality_control.c        src/oee_monitor.c        src/mes_core.c        src/supply_chain.c

OBJS = $(SRCS:src/%.c=build/%.o)
TEST_SRC = tests/test_main.c
TEST_BIN = build/test_runner

.PHONY: all clean test

all: build/libsmartmfg.a

build:
	mkdir -p build

build/%.o: src/%.c include/*.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/libsmartmfg.a: $(OBJS)
	ar rcs $@ $^

$(TEST_BIN): build/libsmartmfg.a $(TEST_SRC)
	$(CC) $(CFLAGS) $(TEST_SRC) -Lbuild -lsmartmfg $(LDFLAGS) -o $@

test: $(TEST_BIN)
	@echo "Running tests..."
	./$(TEST_BIN)
	@echo ""
	@echo "ALL TESTS PASSED"

clean:
	rm -rf build
