CFLAGS=-std=c17 -Wall -Wextra -Wshadow -Wvla -Wno-unused-parameter -Wno-unused-const-variable -g -O0 -fsanitize=address,undefined,leak
CC=gcc

ifdef CI
CFLAGS=-std=c17 -Wall -Wextra -Wshadow -Wvla -Werror -Wno-unused-parameter -Wno-unused-const-variable
endif

# Lista programów do zbudowania
TARGETS=server client

.PHONY: all clean

all: ${TARGETS}

# Reguła kompilacji dla każdego z programów
server: server.c l8_common.h
	${CC} ${CFLAGS} -o server server.c

client: client.c l8_common.h
	${CC} ${CFLAGS} -o client client.c

clean:
	rm -f ${TARGETS}