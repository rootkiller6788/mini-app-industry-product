# mini-enterprise-system Makefile (C99)
# Builds: ERP core + MRP + HRM + CRM + Finance
# Tests: assertion-based test suite covering all APIs

CC       = gcc
CFLAGS   = -std=c99 -Wall -Wextra -pedantic -Iinclude
CFLAGS  += -Wno-unused-variable
LDFLAGS  = -lm

SRCS = src/erp_core.c src/mrp_engine.c src/hrm_system.c src/crm_pipeline.c src/fin_report.c
OBJS = $(SRCS:src/%.c=build/%.o)

# Test objects (no main - compiled to .o, linked via test_runner.c)
TEST_OBJS = build/test_erp_core.o build/test_mrp_engine.o \
            build/test_hrm_system.o build/test_crm_pipeline.o \
            build/test_fin_report.o

EX_SRCS = examples/example_full_erp_cycle.c examples/example_manufacturing.c \
          examples/example_workforce.c examples/example_crm_funnel.c
EX_BINS = $(EX_SRCS:examples/%.c=build/%)

.PHONY: all clean test examples

all: build/libenterprise.a

build:
	mkdir -p build

build/%.o: src/%.c include/*.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/libenterprise.a: $(OBJS)
	ar rcs $@ $^

# Compile test objects
build/test_%.o: tests/test_%.c include/*.h | build
	$(CC) $(CFLAGS) -c $< -o $@

# Link test runner binary: runner + all test objects + library
build/test_runner: tests/test_runner.c $(TEST_OBJS) build/libenterprise.a | build
	$(CC) $(CFLAGS) tests/test_runner.c $(TEST_OBJS) build/libenterprise.a $(LDFLAGS) -o $@

test: build/test_runner
	@echo "=== Running mini-enterprise-system Test Suite ==="
	./build/test_runner
	@echo "=== ALL TESTS PASSED ==="

examples: $(EX_BINS)
	@for ex in $(EX_BINS); do echo "Running $$ex"; ./$$ex; done

build/example_%: examples/example_%.c build/libenterprise.a | build
	$(CC) $(CFLAGS) $< build/libenterprise.a $(LDFLAGS) -o $@

clean:
	rm -rf build
