# mini-industry-solution Makefile (C99)
# Industrial Solutions: SCADA, PLC, Predictive Maintenance,
# Quality Control, Safety Analysis, Production Scheduling

CC       = gcc
CFLAGS   = -std=c99 -Wall -Wextra -pedantic -Iinclude
LDFLAGS  = -lm

SRCS = src/industrial_core.c src/plc_protocol.c \
       src/predictive_maintenance.c src/quality_control.c \
       src/safety_analysis.c src/production_schedule.c

OBJS = $(SRCS:src/%.c=build/%.o)

TESTS = tests/test_industry.c
EXAMPLES = examples/demo_scada.c examples/demo_predictive.c \
           examples/demo_quality.c

.PHONY: all clean test examples

all: build/libindustry.a

build:
	mkdir -p build

build/%.o: src/%.c include/*.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/libindustry.a: $(OBJS)
	ar rcs $@ $(OBJS)

build/test_industry: $(TESTS) build/libindustry.a
	$(CC) $(CFLAGS) $(TESTS) -Lbuild -lindustry $(LDFLAGS) -o $@

build/demo_scada: examples/demo_scada.c build/libindustry.a
	$(CC) $(CFLAGS) examples/demo_scada.c -Lbuild -lindustry $(LDFLAGS) -o $@

build/demo_predictive: examples/demo_predictive.c build/libindustry.a
	$(CC) $(CFLAGS) examples/demo_predictive.c -Lbuild -lindustry $(LDFLAGS) -o $@

build/demo_quality: examples/demo_quality.c build/libindustry.a
	$(CC) $(CFLAGS) examples/demo_quality.c -Lbuild -lindustry $(LDFLAGS) -o $@

test: build/test_industry
	@echo "=== mini-industry-solution Test Suite ==="
	./build/test_industry
	@echo ""
	@echo "ALL TESTS PASSED"

examples: build/demo_scada build/demo_predictive build/demo_quality
	@echo "=== Running SCADA Demo ==="
	./build/demo_scada
	@echo ""
	@echo "=== Running Predictive Maintenance Demo ==="
	./build/demo_predictive
	@echo ""
	@echo "=== Running Quality Control Demo ==="
	./build/demo_quality
	@echo ""
	@echo "ALL EXAMPLES COMPLETE"

clean:
	rm -rf build
