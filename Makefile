CC			= gcc
PROG		= als-led-backlight
CFLAGS		= -Wall -Wextra -Werror -lm -pthread
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

install: all
	cp -v $(BUILDDIR)$(PROG) /opt/$(PROG)
	cp -v res/config.sample /etc/$(PROG).conf
	cp -v res/systemd/$(PROG).service /etc/systemd/system/$(PROG).service
	systemctl enable $(PROG)
	systemctl start $(PROG)

uninstall:
	systemctl stop $(PROG)
	systemctl disable $(PROG)
	rm /etc/systemd/system/$(PROG).service
	rm /opt/$(PROG)