CC       = gcc
CFLAGS   = -std=c99 -Wall -Wextra -O2 -Iinclude
LDFLAGS  = -lm
AR       = ar rcs
BUILD    = build

SRCS     = src/energy_core.c src/energy_carbon.c src/energy_market.c \
           src/energy_optimize.c src/energy_solar.c src/energy_wind.c \
           src/energy_forecast.c
OBJS     = $(SRCS:src/%.c=$(BUILD)/%.o)
LIB      = $(BUILD)/libenergy.a

TEST_SRC = tests/test_energy.c
TEST_BIN = $(BUILD)/test_energy

.PHONY: all lib test clean

all: lib

$(BUILD):
	mkdir -p "$(BUILD)"

lib: $(BUILD) $(LIB)

$(LIB): $(OBJS)
	$(AR) $@ $^

$(BUILD)/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

test: lib $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): $(TEST_SRC) $(LIB)
	$(CC) $(CFLAGS) -o $@ $< -L$(BUILD) -lenergy $(LDFLAGS)

clean:
	rm -rf "$(BUILD)" 2>/dev/null; true
