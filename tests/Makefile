CC=gcc

#CFLAGS=-Wall -W -g -Werror -pthread
CFLAGS=-g

all: clm

clm: client_manager_test.c ../client_manager.c
	$(CC) client_manager_test.c ../client_manager.c $(CFLAGS) -o client_manager_test

clean:
	rm -f client_manager_test *.o
