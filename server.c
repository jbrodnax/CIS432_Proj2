#include "server.h"

struct _client_manager client_manager;

void test_clientlist(){
	struct sockaddr_in clientaddr;
	struct client_entry *client;
	char test_name1[]="John";
	char test_name2[]="Bobby";
	char test_name3[]="Shoe";
	char test_name4[]="HotMop";

	memset(&clientaddr, 0, sizeof(struct sockaddr_in));
	memset(&client_manager, 0, sizeof(struct _client_manager));

	client_add(test_name1, &clientaddr, &client_manager);
	client_add(test_name2, &clientaddr, &client_manager);
	client_add(test_name3, &clientaddr, &client_manager);
	client_add(test_name4, &clientaddr, &client_manager);
	client_remove(test_name1, &client_manager);
	client_remove(test_name2, &client_manager);
	client_remove(test_name1, &client_manager);
	client_add(test_name3, &clientaddr, &client_manager);
	client_add(test_name1, &clientaddr, &client_manager);
	//client_clean(&client_manager);
	//client_clean(client_list);
}

void main(int argc, char *argv[]){
	test_clientlist();
	return;
};
