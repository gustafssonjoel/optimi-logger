CC ?= cc
AR ?= ar
CFLAGS ?= -std=gnu11 -Wall -Wextra -Wpedantic -fPIC -Iinclude
LDFLAGS ?=

SRC_DIR := src
BUILD_DIR := build
SRC := $(SRC_DIR)/optimi_logger.c
OBJ := $(BUILD_DIR)/optimi_logger.o
EXAMPLE_DIR := examples
EXAMPLE_SRC := $(EXAMPLE_DIR)/all_features_demo.c
EXAMPLE_BIN := $(BUILD_DIR)/all_features_demo

STATIC_LIB := $(BUILD_DIR)/liboptimi_logger.a
SHARED_LIB := $(BUILD_DIR)/liboptimi_logger.so

.PHONY: all static shared example clean test

all: static shared

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(OBJ): $(SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $(SRC) -o $(OBJ)

static: $(STATIC_LIB)

$(STATIC_LIB): $(OBJ)
	$(AR) rcs $(STATIC_LIB) $(OBJ)

shared: $(SHARED_LIB)

$(SHARED_LIB): $(OBJ)
	$(CC) -shared $(OBJ) -o $(SHARED_LIB) $(LDFLAGS)

example: $(EXAMPLE_BIN)

$(EXAMPLE_BIN): $(EXAMPLE_SRC) $(SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(EXAMPLE_SRC) $(SRC) -o $(EXAMPLE_BIN) $(LDFLAGS) -pthread

test:
	$(MAKE) -C tests test

clean:
	rm -rf $(BUILD_DIR)
