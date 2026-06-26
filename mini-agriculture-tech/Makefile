CC       = gcc
CFLAGS   = -std=c99 -Wall -Wextra -O2 -Iinclude
LDFLAGS  = -lm
AR       = ar rcs

BUILD    = build
INC      = include
SRC      = src
TESTS    = tests
EXAMPLES = examples
BENCHES  = benches
DEMOS    = demos

SRCS  = $(wildcard $(SRC)/*.c)
OBJS  = $(SRCS:$(SRC)/%.c=$(BUILD)/%.o)
LIB   = $(BUILD)/libagri.a
TEST_SRCS = $(wildcard $(TESTS)/*.c)
TEST_BINS = $(TEST_SRCS:$(TESTS)/%.c=$(BUILD)/test_%.exe)

EXAMPLES_SRC = $(wildcard $(EXAMPLES)/*.c)
EXAMPLES_BIN = $(EXAMPLES_SRC:$(EXAMPLES)/%.c=$(BUILD)/%)
BENCH_SRC = $(wildcard $(BENCHES)/*.c)
BENCH_BIN = $(BENCH_SRC:$(BENCHES)/%.c=$(BUILD)/%)
DEMO_SRC = $(wildcard $(DEMOS)/*.c)
DEMO_BIN = $(DEMO_SRC:$(DEMOS)/%.c=$(BUILD)/%)

.PHONY: all lib test examples benches demo clean

all: lib test examples

$(BUILD):
	mkdir -p "$(BUILD)"

lib: $(BUILD) $(LIB)

$(LIB): $(OBJS)
	$(AR) $@ $^

$(BUILD)/%.o: $(SRC)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

test: lib $(TEST_BINS)
	@total=0; failed=0; \
	for t in $(TEST_BINS); do \
		echo; echo "--- $$(basename $$t) ---"; \
		if $$t; then total=$$((total+1)); else failed=$$((failed+1)); fi; \
	done; \
	echo; echo "=== Test Suites: $$((total+failed)) total, $$total passed, $$failed failed ==="; \
	[ $$failed -eq 0 ]

$(BUILD)/test_%.exe: $(TESTS)/%.c $(LIB)
	$(CC) $(CFLAGS) -o $@ $< -L$(BUILD) -lagri $(LDFLAGS)

examples: lib $(EXAMPLES_BIN)
	@for e in $(EXAMPLES_BIN); do echo; echo "=== $$(basename $$e) ==="; $$e; done

$(BUILD)/example_%: $(EXAMPLES)/example_%.c $(LIB)
	$(CC) $(CFLAGS) -o $@ $< -L$(BUILD) -lagri $(LDFLAGS)

benches: lib $(BENCH_BIN)
	@for b in $(BENCH_BIN); do echo; $$b; done

$(BUILD)/bench_%: $(BENCHES)/bench_%.c $(LIB)
	$(CC) $(CFLAGS) -o $@ $< -L$(BUILD) -lagri $(LDFLAGS)

demo: lib $(DEMO_BIN)
	@for d in $(DEMO_BIN); do echo; $$d; done

$(BUILD)/demo_%: $(DEMOS)/demo_%.c $(LIB)
	$(CC) $(CFLAGS) -o $@ $< -L$(BUILD) -lagri $(LDFLAGS)

clean:
	rm -rf "$(BUILD)" 2>/dev/null; true
