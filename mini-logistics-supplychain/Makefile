CC       = gcc
CFLAGS   = -Wall -Wextra -O2 -I include -I.
LDFLAGS  = -lm
OUTDIR   = bin

SRC      = src
EXP      = examples
DM       = demos
TST      = tests
BNCH     = benches

# Object files for all source modules
OBJS     = $(OUTDIR)/logistics_core.o \
           $(OUTDIR)/inventory.o \
           $(OUTDIR)/transportation.o \
           $(OUTDIR)/warehouse.o \
           $(OUTDIR)/demand_forecast.o \
           $(OUTDIR)/scm_optimize.o

# Example executables
EXAMPLES = $(OUTDIR)/example_inventory.exe \
           $(OUTDIR)/example_routing.exe \
           $(OUTDIR)/example_warehouse.exe

# Demo executables
DEMOS    = $(OUTDIR)/supply_chain_demo.exe

# Benchmark executable
BENCHES  = $(OUTDIR)/bench_routing.exe

# Test executable
TEST_EXE = $(OUTDIR)/test_all.exe

.PHONY: all clean test run-examples run-demo run-bench help

all: $(OUTDIR) $(EXAMPLES) $(DEMOS) $(BENCHES) $(TEST_EXE)

$(OUTDIR):
	mkdir -p $(OUTDIR)

# ── Source Object Files ──────────────────────────────────────

$(OUTDIR)/logistics_core.o: $(SRC)/logistics_core.c include/logistics_core.h
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/inventory.o: $(SRC)/inventory.c include/inventory.h include/logistics_core.h
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/transportation.o: $(SRC)/transportation.c include/transportation.h include/logistics_core.h
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/warehouse.o: $(SRC)/warehouse.c include/warehouse.h include/logistics_core.h
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/demand_forecast.o: $(SRC)/demand_forecast.c include/demand_forecast.h
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTDIR)/scm_optimize.o: $(SRC)/scm_optimize.c include/scm_optimize.h include/logistics_core.h include/inventory.h
	$(CC) $(CFLAGS) -c $< -o $@

# ── Example Executables ──────────────────────────────────────

$(OUTDIR)/example_inventory.exe: $(EXP)/example_inventory.c $(OBJS)
	$(CC) $(CFLAGS) $< $(OBJS) -o $@ $(LDFLAGS)

$(OUTDIR)/example_routing.exe: $(EXP)/example_routing.c $(OBJS)
	$(CC) $(CFLAGS) $< $(OBJS) -o $@ $(LDFLAGS)

$(OUTDIR)/example_warehouse.exe: $(EXP)/example_warehouse.c $(OBJS)
	$(CC) $(CFLAGS) $< $(OBJS) -o $@ $(LDFLAGS)

# ── Demo Executable ──────────────────────────────────────────

$(OUTDIR)/supply_chain_demo.exe: $(DM)/supply_chain_demo.c $(OBJS)
	$(CC) $(CFLAGS) $< $(OBJS) -o $@ $(LDFLAGS)

# ── Benchmark Executable ─────────────────────────────────────

$(OUTDIR)/bench_routing.exe: $(BNCH)/bench_routing.c $(OBJS)
	$(CC) $(CFLAGS) $< $(OBJS) -o $@ $(LDFLAGS)

# ── Test Executable ──────────────────────────────────────────

$(OUTDIR)/test_all.exe: $(TST)/test_all.c $(OBJS)
	$(CC) $(CFLAGS) $< $(OBJS) -o $@ $(LDFLAGS)

# ── Test Target ─────────────────────────────────────────────

test: $(OUTDIR) $(OUTDIR)/test_all.exe
	./$(OUTDIR)/test_all.exe

# ── Run Targets ──────────────────────────────────────────────

run-examples: $(EXAMPLES)
	@echo "=== Running Example: Inventory ==="
	./$(OUTDIR)/example_inventory.exe
	@echo ""
	@echo "=== Running Example: Routing ==="
	./$(OUTDIR)/example_routing.exe
	@echo ""
	@echo "=== Running Example: Warehouse ==="
	./$(OUTDIR)/example_warehouse.exe

run-demo: $(OUTDIR)/supply_chain_demo.exe
	./$(OUTDIR)/supply_chain_demo.exe

run-bench: $(OUTDIR)/bench_routing.exe
	./$(OUTDIR)/bench_routing.exe

# ── Clean ───────────────────────────────────────────────────

clean:
	rm -rf $(OUTDIR)

# ── Help ────────────────────────────────────────────────────

help:
	@echo ""
	@echo "mini-logistics-supplychain Makefile"
	@echo "===================================="
	@echo ""
	@echo "  make             Build all (examples + demo + bench + test)"
	@echo "  make test        Run comprehensive test suite"
	@echo "  make run-examples Run all example programs"
	@echo "  make run-demo    Run supply chain end-to-end demo"
	@echo "  make run-bench   Run routing algorithm benchmarks"
	@echo "  make clean       Remove build output"
	@echo "  make help        Show this help"
	@echo ""
