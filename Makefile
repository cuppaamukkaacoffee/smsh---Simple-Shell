CC=gcc
CFLAGS=-o
TARGET=smsh
all: $(TARGET)
.PHONY: all
%:
	$(CC) -o $@ $@.c

clean:
	rm -f $(TARGET)
