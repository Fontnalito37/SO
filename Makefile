CC = gcc
CFLAGS = -Wall -Wextra -std=gnu11

PROGRAM = p2
SOURCES = p2.c dynamic_list.c mem_list.c
HEADERS = dynamic_list.h mem_list.h

.PHONY: all clean

all: $(PROGRAM)

$(PROGRAM): $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $(SOURCES)

clean:
	rm -f $(PROGRAM)
