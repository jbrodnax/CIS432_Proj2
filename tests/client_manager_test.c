#include "../server.h"

struct _client_manager client_manager;

void test_clientlist(){
	struct sockaddr_in clientaddr;
	struct client_entry *client;
	int ret;
	char test_name1[]="John";
	char test_name2[]="Bobby";
	char test_name3[]="Shoe";
	char test_name4[]="HotMop";

	memset(&clientaddr, 0, sizeof(struct sockaddr_in));
	memset(&client_manager, 0, sizeof(struct _client_manager));

	client = client_add(test_name1, &clientaddr, &client_manager);
	if(client)
		client_print(client);
	else
		puts("client_add failed");

	client = client_add(test_name2, &clientaddr, &client_manager);
	if(client)
                client_print(client);
        else
                puts("client_add failed");

	client = client_add(test_name3, &clientaddr, &client_manager);
	if(client)
                client_print(client);
        else
                puts("client_add failed");

	client = client_add(test_name4, &clientaddr, &client_manager);
	if(client)
                client_print(client);
        else
                puts("client_add failed");

	if(client_remove(test_name1, &client_manager) != 0)
		puts("client_remove failed");
	else
		printf("removed client: (%s)\n", test_name1);

	if(client_remove(test_name2, &client_manager) != 0)
		puts("client_remove failed");
	else
		printf("removed client: (%s)\n", test_name2);

	if(client_remove(test_name1, &client_manager) != -1)
		puts("client_remove failed to catch error");
	else
		puts("client_remove successfully caught error");

	client_add(test_name3, &clientaddr, &client_manager);
	client_add(test_name1, &clientaddr, &client_manager);
	client_clean(&client_manager);
	puts("[!] Client Manager test finished.");
}

void main(){
	test_clientlist();
	return;
};
