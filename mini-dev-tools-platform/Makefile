CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99 -O2 -I./include
LDFLAGS =
BUILD_DIR = build

SRCS = src/logger.c src/config_mgr.c src/cli_parser.c src/profiler.c \
       src/feature_flags.c src/code_review.c src/deploy_manager.c \
       src/package_manager.c src/test_runner.c src/cicd_pipeline.c
TEST_SRC = tests/test_core.c
TEST_BIN = $(BUILD_DIR)/test_core
EXAMPLE_SRC = examples/example_demo.c
EXAMPLE_BIN = $(BUILD_DIR)/example_demo

.PHONY: all clean test examples

all: $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $(SRCS)

test: $(TEST_BIN)
	@echo "=== Running Tests ==="
	./$(TEST_BIN) || echo "Some tests failed"

examples: $(EXAMPLE_BIN)
	@echo "=== Running Examples ==="
	./$(EXAMPLE_BIN) || echo "Example failed"

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(TEST_BIN): $(BUILD_DIR) $(SRCS) $(TEST_SRC)
	$(CC) $(CFLAGS) $(SRCS) $(TEST_SRC) -o $(TEST_BIN) $(LDFLAGS)

$(EXAMPLE_BIN): $(BUILD_DIR) $(SRCS) $(EXAMPLE_SRC)
	$(CC) -Wall -Wextra -std=c99 -O2 -I./include $(SRCS) $(EXAMPLE_SRC) -o $(EXAMPLE_BIN) $(LDFLAGS)

check:
	$(CC) $(CFLAGS) -fsyntax-only $(SRCS) $(TEST_SRC)

clean:
	rm -rf $(BUILD_DIR) *.o
