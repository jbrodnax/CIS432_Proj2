CC=gcc
CLDIR=client_modules/
SRDIR=server_modules/

CFLAGSDB=-Wall -W -g -Werror -pthread
CFLAGS=-pthread

all: server client

debug: clientdb serverdb

client: client.c $(CLDIR)channels.c $(CLDIR)raw.c
	$(CC) client.c $(CLDIR)channels.c $(CLDIR)raw.c $(CFLAGS) -o client

server: server.c $(SRDIR)linkedlist.c $(SRDIR)servertree.c $(SRDIR)handle_request.c
	$(CC) server.c $(SRDIR)linkedlist.c $(SRDIR)servertree.c $(SRDIR)handle_request.c $(CFLAGS) -o server

clientdb: client.c $(CLDIR)channels.c $(CLDIR)raw.c
	$(CC) client.c $(CLDIR)channels.c $(CLDIR)raw.c $(CFLAGSDB) -o client

serverdb: server.c $(SRDIR)linkedlist.c $(SRDIR)servertree.c $(SRDIR)handle_request.c
	$(CC) server.c $(SRDIR)linkedlist.c $(SRDIR)servertree.c $(SRDIR)handle_request.c $(CFLAGSDB) -o server

clean:
	rm -f server client *.o
