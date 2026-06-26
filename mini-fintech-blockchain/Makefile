CC       = gcc
CFLAGS   = -std=c99 -Wall -Wextra -O2 -I include
LDFLAGS  = -lm
OUTDIR   = bin
SRC      = src/fintech_core.c
TEST_SRC = tests/test_fintech.c
TARGET   = $(OUTDIR)/test_fintech

.PHONY: all test clean

all: test

test: $(SRC) $(TEST_SRC)
	mkdir -p $(OUTDIR)
	$(CC) $(CFLAGS) $(TEST_SRC) $(SRC) -o $(TARGET) $(LDFLAGS)
	$(TARGET)

clean:
	rm -rf $(OUTDIR)
