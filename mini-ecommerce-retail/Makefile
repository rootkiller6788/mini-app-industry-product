CC       = gcc
CFLAGS   = -std=c99 -Wall -Wextra -O2 -I include
LDFLAGS  = -lm
OUTDIR   = bin

SRC      = src/ecommerce_core.c src/ecommerce_payment.c src/ecommerce_search.c \
           src/ecommerce_analytics.c src/ecommerce_advanced.c
TEST_SRC = tests/test_ecom.c
EXAMPLES = examples/example_checkout.c examples/example_search.c \
           examples/example_analytics.c examples/example_fraud_flow.c

TARGET   = $(OUTDIR)/test_ecom
EXE1     = $(OUTDIR)/example_checkout
EXE2     = $(OUTDIR)/example_search
EXE3     = $(OUTDIR)/example_analytics
EXE4     = $(OUTDIR)/example_fraud_flow

.PHONY: all test clean examples

all: test

test: $(SRC) $(TEST_SRC)
	mkdir -p $(OUTDIR)
	$(CC) $(CFLAGS) $(TEST_SRC) $(SRC) -o $(TARGET) $(LDFLAGS)
	$(TARGET)

examples: $(EXE1) $(EXE2) $(EXE3) $(EXE4)

$(EXE1): examples/example_checkout.c $(SRC)
	mkdir -p $(OUTDIR)
	$(CC) $(CFLAGS) examples/example_checkout.c $(SRC) -o $@ $(LDFLAGS)

$(EXE2): examples/example_search.c $(SRC)
	mkdir -p $(OUTDIR)
	$(CC) $(CFLAGS) examples/example_search.c $(SRC) -o $@ $(LDFLAGS)

$(EXE3): examples/example_analytics.c $(SRC)
	mkdir -p $(OUTDIR)
	$(CC) $(CFLAGS) examples/example_analytics.c $(SRC) -o $@ $(LDFLAGS)

$(EXE4): examples/example_fraud_flow.c $(SRC)
	mkdir -p $(OUTDIR)
	$(CC) $(CFLAGS) examples/example_fraud_flow.c $(SRC) -o $@ $(LDFLAGS)

clean:
	rm -rf $(OUTDIR)
