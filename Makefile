CC=gcc

# CFLAGS=-Wall -W -g -Werror -pthread
CFLAGS=-pthread

all: server client

client: client.c
	$(CC) client.c $(CFLAGS) -o client

server: server.c client_manager.c 
	$(CC) server.c client_manager.c $(CFLAGS) -o server

clean:
	rm -f server client *.o
