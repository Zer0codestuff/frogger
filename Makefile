CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2 -D_XOPEN_SOURCE_EXTENDED
LDFLAGS = -lncursesw

SRC = src/main.c
BIN = cursor

all: $(BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(BIN)


