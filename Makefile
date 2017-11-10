CC=gcc

# CFLAGS=-Wall -W -g -Werror -pthread
CFLAGS=-pthread

all: server client

client: client.c
	$(CC) client.c $(CFLAGS) -o client

server: server.c 
	$(CC) server.c $(CFLAGS) -o server

clean:
	rm -f server client *.o
