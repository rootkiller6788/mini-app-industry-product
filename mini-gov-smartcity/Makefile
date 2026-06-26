CC       = gcc
CFLAGS   = -std=c99 -Wall -Wextra -pedantic -Iinclude
LDFLAGS  = -lm

SRCS = src/smartcity_core.c src/smartcity_infra.c src/smartcity_traffic.c src/smartcity_env.c src/smartcity_ems.c
OBJS = $(SRCS:src/%.c=build/%.o)
TEST_SRC = tests/test_smartcity.c
TARGET = build/test_smartcity

.PHONY: all clean test

all: build/libsmartcity.a

build:
	mkdir -p build

build/%.o: src/%.c include/*.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/libsmartcity.a: $(OBJS)
	ar rcs $@ $^

test: $(SRCS) $(TEST_SRC) | build
	$(CC) $(CFLAGS) $(TEST_SRC) $(SRCS) -o $(TARGET) $(LDFLAGS)
	$(TARGET)

clean:
	rm -rf build
