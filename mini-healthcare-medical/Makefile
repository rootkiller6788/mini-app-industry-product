CC       = gcc
CFLAGS   = -std=c99 -Wall -Wextra -O2 -I include
LDFLAGS  = -lm
OUTDIR   = bin
SRC      = src/healthcare_core.c src/clinical_dss.c src/pharmacy_mgmt.c src/hospital_ops.c src/patient_monitor.c src/lab_analyzer.c src/drug_interaction.c
TEST_SRC = tests/test_med.c
TARGET   = $(OUTDIR)/test_med.exe

.PHONY: all test clean

all: test

test: $(SRC) $(TEST_SRC)
	mkdir -p $(OUTDIR)
	$(CC) $(CFLAGS) $(TEST_SRC) $(SRC) -o $(TARGET) $(LDFLAGS)
	$(TARGET)

clean:
	rm -rf $(OUTDIR)
