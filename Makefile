CC=gcc

#CFLAGS=-Wall -W -g -Werror -pthread
CFLAGS=-g -pthread

all: server client

client: client.c list_manager.c raw.c
	$(CC) client.c list_manager.c raw.c $(CFLAGS) -o client

server: server.c client_manager.c servertree.c handle_request.c
	$(CC) server.c client_manager.c servertree.c handle_request.c $(CFLAGS) -o server

clean:
	rm -f server client *.o
