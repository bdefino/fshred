CC := cc
CFLAGS := -std=c89 -Wall -Werror
NAME := fshred
INSTALL_PATH := /usr/local/bin/$(NAME)
SOURCES := fshred-tiny.c

install:
	$(CC) $(CFLAGS) -o $(INSTALL_PATH) $(SOURCES)
	chmod 700 $(INSTALL_PATH)

