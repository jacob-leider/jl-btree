CC = gcc
CFLAGS = -g -Wall
OBJS = main.o btree.o btree_node.o search.o serialize.o testutils.o btree_tests.o printutils.o 
TARGET = main

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Implicit rule for building .o files from .c files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f *.o main
