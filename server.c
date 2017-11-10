#include "server.h"

struct _client_manager client_manager;

int main(int argc, char *argv[]){
	int portno;

	if(argc != 2){
		printf("Usage: \n");
		exit(0);
	}
	portno = atoi(argv[1]);
	printf("port # %d\n", portno);
	return 0;
};
