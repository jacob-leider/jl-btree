#TODO: Make this work better

#Defines
CC=gcc
CFLAGS=-c -Wall
CFLAGS := -Wall -Wextra -std=c11 -I../../test -g
EXE=main
SOURCES=test.c ../json_parser/parse_json.c
OBJECTS=$(SOURCES:.c=.o)

#Build Rules

all: $(SOURCES) $(EXE)

.o:
	$(CC) $(CFLAGS) $< -o $@

$(EXE): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(EXE)

clean: 
	rm -f $(OBJECTS)  $(EXE)