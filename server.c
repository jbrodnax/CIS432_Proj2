#include "server.h"

struct client_entry *client_list;

void test_clientlist(){
	struct sockaddr_in clientaddr;
	memset(&clientaddr, 0, sizeof(struct sockaddr_in));
	client_list = NULL;

	client_add("John", &clientaddr, client_list);
	client_add("Bobby", &clientaddr, client_list);
	client_remove("Bobby", client_list);
	client_clean(client_list);
}

void main(int argc, char *argv[]){
	test_clientlist();
	return;
};
