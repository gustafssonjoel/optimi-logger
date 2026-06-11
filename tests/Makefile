CC ?= cc
CFLAGS ?= -std=gnu11 -Wall -Wextra -Wpedantic -I../include

HEADER_TEST_BIN := test_optimi_logger_header
HEADER_TEST_SRC := test_optimi_logger_header.c
FLUSH_TEST_BIN := test_optimi_logger_flushing
FLUSH_TEST_SRC := test_optimi_logger_flushing.c
ACCEPTANCE_TEST_BIN := test_optimi_logger_acceptance
ACCEPTANCE_TEST_SRC := test_optimi_logger_acceptance.c
CONFIG_TEST_BIN := test_optimi_logger_config
CONFIG_TEST_SRC := test_optimi_logger_config.c
THREADING_TEST_BIN := test_optimi_logger_threading
THREADING_TEST_SRC := test_optimi_logger_threading.c
MEMORY_TEST_BIN := test_optimi_logger_memory
MEMORY_TEST_SRC := test_optimi_logger_memory.c

.PHONY: all test clean

all: test

test: $(HEADER_TEST_BIN) $(FLUSH_TEST_BIN) $(ACCEPTANCE_TEST_BIN) $(CONFIG_TEST_BIN) $(THREADING_TEST_BIN) $(MEMORY_TEST_BIN)
	./$(HEADER_TEST_BIN)
	./$(FLUSH_TEST_BIN)
	./$(ACCEPTANCE_TEST_BIN)
	./$(CONFIG_TEST_BIN)
	./$(THREADING_TEST_BIN)
	./$(MEMORY_TEST_BIN)

$(HEADER_TEST_BIN): $(HEADER_TEST_SRC)
	$(CC) $(CFLAGS) $< -o $@

$(FLUSH_TEST_BIN): $(FLUSH_TEST_SRC)
	$(CC) $(CFLAGS) $< -o $@

$(ACCEPTANCE_TEST_BIN): $(ACCEPTANCE_TEST_SRC)
	$(CC) $(CFLAGS) $< -o $@

$(CONFIG_TEST_BIN): $(CONFIG_TEST_SRC)
	$(CC) $(CFLAGS) $< -o $@

$(THREADING_TEST_BIN): $(THREADING_TEST_SRC) optimi_logger_stubs.c
	$(CC) $(CFLAGS) $< optimi_logger_stubs.c -o $@ -lpthread

$(MEMORY_TEST_BIN): $(MEMORY_TEST_SRC) optimi_logger_stubs.c
	$(CC) $(CFLAGS) $< optimi_logger_stubs.c -o $@

clean:
	rm -f $(HEADER_TEST_BIN) $(FLUSH_TEST_BIN) $(ACCEPTANCE_TEST_BIN) $(CONFIG_TEST_BIN) $(THREADING_TEST_BIN) $(MEMORY_TEST_BIN)
