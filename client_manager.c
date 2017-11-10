#include "duckchat.h"

char err1_client[]="Client list already initialized.";
char err2_client[]="client_add received NULL argument.";

struct client_entry{
	char username[NAME_LEN];
	struct sockaddr_in clientaddr;
	char *hostent;
	struct client_info *next;
	struct client_info *prev;
};

/*Always points to first client in list*/
struct client_entry *client_list;

void error_msg(char *err_msg){
	if(!err_msg){
		perror("[-] ERROR:\n\t ");
		return;
	}

	printf("[-] ERROR:\n\t %s\n", err_msg);
	return;
}

struct client_entry *client_search(char *name, struct client_entry *first){
/*For now, searches for client based on username by iterating through client list */
	char * username;
	struct client_entry *client;

	if(!name || !first)
		return NULL;

	username = name;
	client = first;

	while(client){
		if((strlen(client->username)) == 0)
			continue;
		if(!strncmp(client->username, username, NAME_LEN))
			return client;
		client = client->next;
	}

	return NULL;
}

int client_init_list(){
/*If client_list is empty, allocate space for first client and zero it out*/
	if(client_list){
		error_msg(err1_client);
		return -1;
	}
	client_list = malloc(sizeof(struct client_entry));
	if(!client_list){
		error_msg(NULL);
		return -1;
	}
	memset(client_list, 0, sizeof(struct client_entry));
	return 0;
}

struct client_entry *client_list_tail(){
/*Returns ptr to last client_entry in list*/
	struct client_entry *client;

	if(!client_list)
		return NULL;
	else
		client = client_list;

	while(client->next)
		client = client->next;

	return client;
}

struct client_entry *client_add(char *name, struct sockaddr_in *clientaddr, char *hostent){
	struct client_entry *new_client;
	struct client_entry *list_tail;

	if(!name || !clientaddr || !hostent){
		error_msg(err2_client);
		return NULL;
	}
	if(!client_list){
		if(client_init_list() < 0)
			exit(1);
	}

	list_tail = client_list_tail();
	new_client = malloc(sizeof(struct client_entry));
	if(!new_client){
		error_msg(NULL);
		exit(1);
	}
	memset(new_client, 0, sizeof(struct client_entry));
}












