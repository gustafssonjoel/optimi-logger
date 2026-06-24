CC ?= cc
CFLAGS ?= -std=gnu11 -Wall -Wextra -Wpedantic -I../include
BUILD_DIR := ../build

HEADER_TEST_BIN := $(BUILD_DIR)/test_optimi_logger_header
HEADER_TEST_SRC := test_optimi_logger_header.c
FLUSH_TEST_BIN := $(BUILD_DIR)/test_optimi_logger_flushing
FLUSH_TEST_SRC := test_optimi_logger_flushing.c
ACCEPTANCE_TEST_BIN := $(BUILD_DIR)/test_optimi_logger_acceptance
ACCEPTANCE_TEST_SRC := test_optimi_logger_acceptance.c
CONFIG_TEST_BIN := $(BUILD_DIR)/test_optimi_logger_config
CONFIG_TEST_SRC := test_optimi_logger_config.c
THREADING_TEST_BIN := $(BUILD_DIR)/test_optimi_logger_threading
THREADING_TEST_SRC := test_optimi_logger_threading.c
MEMORY_TEST_BIN := $(BUILD_DIR)/test_optimi_logger_memory
MEMORY_TEST_SRC := test_optimi_logger_memory.c

.PHONY: all test clean

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

all: test

test: $(BUILD_DIR) $(HEADER_TEST_BIN) $(FLUSH_TEST_BIN) $(ACCEPTANCE_TEST_BIN) $(CONFIG_TEST_BIN) $(THREADING_TEST_BIN) $(MEMORY_TEST_BIN)
	$(BUILD_DIR)/test_optimi_logger_header
	$(BUILD_DIR)/test_optimi_logger_flushing
	$(BUILD_DIR)/test_optimi_logger_acceptance
	$(BUILD_DIR)/test_optimi_logger_config
	$(BUILD_DIR)/test_optimi_logger_threading
	$(BUILD_DIR)/test_optimi_logger_memory

$(HEADER_TEST_BIN): $(HEADER_TEST_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< -o $@

$(FLUSH_TEST_BIN): $(FLUSH_TEST_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< -o $@

$(ACCEPTANCE_TEST_BIN): $(ACCEPTANCE_TEST_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< -o $@

$(CONFIG_TEST_BIN): $(CONFIG_TEST_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< -o $@

$(THREADING_TEST_BIN): $(THREADING_TEST_SRC) optimi_logger_stubs.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< optimi_logger_stubs.c -o $@ -lpthread

$(MEMORY_TEST_BIN): $(MEMORY_TEST_SRC) optimi_logger_stubs.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< optimi_logger_stubs.c -o $@

clean:
	rm -f $(BUILD_DIR)/test_optimi_logger_*
