# @file Makefile 
#
# @author Jacob Leider 
#
# @date 04/04/2026
#
# @description Builds only the core btree library 

# Compiler
CC := gcc
CFLAGS := -Wall -Wpedantic -Wextra -std=c11 -I./src -I../../src/core -g

# Directories
CORE_SRC_DIR := ../../src/core

# TODO: Rename this to something more conventional for python extension libraries
DLL_DIR := ./shared

# Target name
TARGET := $(DLL_DIR)/jl_btree

# Source files
CORE_SRCS := $(wildcard $(CORE_SRC_DIR)/*.c)

# Object files (placed in build/)
OBJS := $(CORE_SRCS:$(CORE_SRC_DIR)/%.c=$(DLL_DIR)/%.o)

# Default target (optional)
.PHONY: all
all: jl_btree.so

# Test target
.PHONY: jl_btree.so
jl_btree: $(TARGET)

# Link
$(TARGET): $(OBJS) | $(DLL_DIR)
	$(CC) $(CFLAGS) -shared -o $@ $^

# Compile core src files
$(DLL_DIR)/%.o: $(CORE_SRC_DIR)/%.c | $(DLL_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Ensure build directory exists
$(DLL_DIR):
	mkdir -p $(DLL_DIR)

# Clean
.PHONY: clean
clean:
	rm -rf $(DLL_DIR)
