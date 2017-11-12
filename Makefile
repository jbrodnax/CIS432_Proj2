CC=gcc

#CFLAGS=-Wall -W -g -Werror -pthread
CFLAGS=-g -pthread

all: server client

client: client.c raw.c
	$(CC) client.c raw.c $(CFLAGS) -o client

server: server.c client_manager.c
	$(CC) server.c client_manager.c $(CFLAGS) -o server

clean:
	rm -f server client *.o
