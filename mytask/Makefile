CC = cc
CFLAGS = -std=c17 -Wall -Wextra -Wshadow -Wvla -Wno-unused-parameter -Wno-unused-const-variable -g -O0 -fsanitize=address,undefined,leak
LDFLAGS = -lpthread

all: sop-chamber

sop-chamber: sop-chamber.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f sop-chamber

.PHONY: all clean