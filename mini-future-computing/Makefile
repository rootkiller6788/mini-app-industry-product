# mini-future-computing Makefile
# Future Computing Technologies -- C99 Implementation

CC       = gcc
CFLAGS   = -std=c99 -Wall -Wextra -pedantic -Iinclude -O2
LDFLAGS  = -lm
OUTDIR   = build
TESTDIR  = build/tests

SRCS     = $(wildcard src/*.c)
OBJS     = $(SRCS:src/%.c=$(OUTDIR)/%.o)
TESTS    = $(wildcard tests/*.c)
TESTEXE  = $(TESTS:tests/%.c=$(TESTDIR)/%)
EXAMPLES = $(wildcard examples/*.c)
EXE      = $(EXAMPLES:examples/%.c=$(OUTDIR)/%)

.PHONY: all test clean examples

all: $(OUTDIR) $(OBJS)

$(OUTDIR):
	mkdir -p $(OUTDIR) $(TESTDIR)

$(OUTDIR)/%.o: src/%.c include/*.h | $(OUTDIR)
	$(CC) $(CFLAGS) -c $< -o $@

test: $(OUTDIR) $(OBJS) $(TESTEXE)
	@echo "=== Running Tests ==="
	@for t in $(TESTEXE); do \
		if [ -f "$$t.exe" ]; then t="$$t.exe"; fi; \
		if eval "$$t"; then echo "PASS: $$t"; \
		else echo "FAIL: $$t"; exit 1; fi; \
	done
	@echo "=== All Tests Passed ==="

$(TESTDIR)/%: tests/%.c $(OBJS) | $(OUTDIR)
	$(CC) $(CFLAGS) $< $(OBJS) -o $@ $(LDFLAGS)

examples: $(OUTDIR) $(OBJS) $(EXE)

$(OUTDIR)/%: examples/%.c $(OBJS) | $(OUTDIR)
	$(CC) $(CFLAGS) $< $(OBJS) -o $@ $(LDFLAGS)

clean:
	rm -rf $(OUTDIR)
