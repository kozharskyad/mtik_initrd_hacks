CROSS_COMPILE:=$(CROSS_COMPILE)
CC=$(CROSS_COMPILE)gcc
CFLAGS=-std=c99 -march=mips32r2 -pedantic -pedantic-errors -Wall -Werror -I. -O2 -D_POSIX_SOURCE=1 -DSYSDIR_SUFFIX=$(SYSDIR_SUFFIX)
LD=$(CROSS_COMPILE)gcc
LDFLAGS=-static -no-pie
STRIP=$(CROSS_COMPILE)strip
STRIPFLAGS=-s
RM=rm
RMFLAGS=-f

all: build

build: init

init: init.o
	$(LD) $(LDFLAGS) $< -o $@
	$(STRIP) $(STRIPFLAGS) $@

init.o: init.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(RMFLAGS) init init.o

.PHONY: all build clean
