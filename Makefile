# Compiler
CC := gcc
CFLAGS := -Wall -Wpedantic -Wextra -std=c11 -I./src -I./test -I./src/core -g

# Directories
CORE_SRC_DIR := src/core
UTILS_SRC_DIR := src/utils
TEST_DIR := test
BUILD_DIR := build

# Target name
TEST_TARGET := $(BUILD_DIR)/test

# Source files
CORE_SRCS := $(wildcard $(CORE_SRC_DIR)/*.c)
UTILS_SRCS := $(wildcard $(UTILS_SRC_DIR)/*.c)
TEST_SRCS := $(wildcard $(TEST_DIR)/*.c)

# Object files (placed in build/)
CORE_SRC_OBJS := $(CORE_SRCS:$(CORE_SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
UTILS_SRC_OBJS := $(UTILS_SRCS:$(UTILS_SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
TEST_OBJS := $(TEST_SRCS:$(TEST_DIR)/%.c=$(BUILD_DIR)/%.o)

OBJS := $(CORE_SRC_OBJS) $(UTILS_SRC_OBJS) $(TEST_OBJS)

# Default target (optional)
.PHONY: all
all: test

# Test target
.PHONY: test
test: $(TEST_TARGET)

# Link
$(TEST_TARGET): $(OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^

# Compile core src files
$(BUILD_DIR)/%.o: $(CORE_SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile utils src files
$(BUILD_DIR)/%.o: $(UTILS_SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile test files
$(BUILD_DIR)/%.o: $(TEST_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Ensure build directory exists
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Clean
.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
