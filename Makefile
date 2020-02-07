CC := cc
CFLAGS := -Wall -Werror
NAME := fshred
INSTALL_PATH := /usr/local/bin/$(NAME)
SOURCES := fshred-tiny.c

install:
	$(CC) $(CFLAGS) -o $(INSTALL_PATH) $(SOURCES)
	chmod 700 $(INSTALL_PATH)
