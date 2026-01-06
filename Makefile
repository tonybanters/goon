CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99
PREFIX = /usr/local

SRC = src/main.c src/goon.c
OBJ = $(SRC:.c=.o)

all: goon

goon: $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC)

debug: CFLAGS = -Wall -Wextra -g -std=c99
debug: goon

install: goon
	install -Dm755 goon $(DESTDIR)$(PREFIX)/bin/goon

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/goon

clean:
	rm -f goon $(OBJ)

check: goon
	@./tests/run_tests.sh

.PHONY: all debug install uninstall clean check
