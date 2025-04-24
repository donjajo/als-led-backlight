CC			= gcc
PROG		= als-led-backlight
CFLAGS		= -Wall -lm -pthread
BUILDDIR	= build/
SRC			= $(wildcard ./*.c) $(wildcard ./devices/*.c) $(wildcard ./devices/*/*.c)
BINS		= $(SRC:%.c=%.o)
SHELL		= /bin/bash

.PHONY: clean
all: $(PROG)

debug: CFLAGS += -DDEBUG -g
debug: $(PROG)

$(PROG): $(BINS)
	$(CC) $^ $(CFLAGS)  -o $(BUILDDIR)$@

%.o: %.c
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -c $? -o $@

clean:
	rm -rf build/
	find ./ -type f -name '*.o' -exec rm {} +
