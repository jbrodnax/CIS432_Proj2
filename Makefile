CC=gcc
CLDIR=client_modules/
SRDIR=server_modules/

CFLAGS=-Wall -W -g -Werror -pthread
#CFLAGS=-g -pthread

all: server client

client: client.c $(CLDIR)channels.c $(CLDIR)raw.c
	$(CC) client.c $(CLDIR)channels.c $(CLDIR)raw.c $(CFLAGS) -o client

server: server.c $(SRDIR)linkedlist.c $(SRDIR)servertree.c $(SRDIR)handle_request.c
	$(CC) server.c $(SRDIR)linkedlist.c $(SRDIR)servertree.c $(SRDIR)handle_request.c $(CFLAGS) -o server

clean:
	rm -f server client *.o
