CC=gcc
CFLAGS=-std=c89 -pedantic -pedantic-errors -Wall -Werror -I. -O2 -D_POSIX_SOURCE=1
LD=gcc
LDFLAGS=-static
STRIP=strip
STRIPFLAGS=-s
RM=rm
RMFLAGS=-f

all: build

build: kernel-extractor

kernel-extractor: kernel-extractor.o
	$(LD) $(LDFLAGS) $< -o $@
	$(STRIP) $(STRIPFLAGS) $@

kernel-extractor.o: kernel-extractor.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(RMFLAGS) kernel-extractor kernel-extractor.o

.PHONY: all build clean
