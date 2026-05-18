CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=c11 -D_GNU_SOURCE
LDFLAGS =
PREFIX  = /usr/local

SRCS = pgcache.c process.c mincore.c format.c
OBJS = $(SRCS:.c=.o)
TARGET = pgcache

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c pgcache.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

install: $(TARGET)
	install -m 755 $(TARGET) $(PREFIX)/bin/

.PHONY: all clean install
