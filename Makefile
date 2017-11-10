CC=gcc

CFLAGS=-Wall -W -g -Werror -pthread
#CFLAGS=-pthread

all: server client

tests: tests/client_manager_test.c client_manager.c
	$(CC) tests/client_manager_test.c client_manager.c $(CFLAGS) -o client_manager_test

client: client.c
	$(CC) client.c $(CFLAGS) -o client

server: server.c client_manager.c 
	$(CC) server.c client_manager.c $(CFLAGS) -o server

clean:
	rm -f server client *.o
