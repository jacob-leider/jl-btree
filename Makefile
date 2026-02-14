# Compiler
CC := gcc
CFLAGS := -Wall -Wextra -std=c11 -I./src -I./test -g

# Directories
SRC_DIR := src
TEST_DIR := test
BUILD_DIR := build

# Target name
TEST_TARGET := $(BUILD_DIR)/test

# Source files
SRC_SRCS := $(wildcard $(SRC_DIR)/*.c)
TEST_SRCS := $(wildcard $(TEST_DIR)/*.c)

# Object files (placed in build/)
SRC_OBJS := $(SRC_SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
TEST_OBJS := $(TEST_SRCS:$(TEST_DIR)/%.c=$(BUILD_DIR)/%.o)

OBJS := $(SRC_OBJS) $(TEST_OBJS)

# Default target (optional)
.PHONY: all
all: test

# Test target
.PHONY: test
test: $(TEST_TARGET)

# Link
$(TEST_TARGET): $(OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^

# Compile src files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
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
