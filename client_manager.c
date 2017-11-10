#include "duckchat.h"

char err1_client[]="Client list already initialized.";
char err2_client[]="client_add received NULL argument.";
char err3_client[]="client_remove received NULL clientaddr ptr."

struct client_entry{
	char username[NAME_LEN+STR_PADD];
	struct sockaddr_in clientaddr;
	struct hostent *hostp;
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

struct client_entry *client_search(struct sockaddr_in *clientaddr, struct client_entry *first){
/*For now, searches for client based on ip addr and port # by iterating through client list*/
	struct client_entry *client;
	unsigned long c_addr;
	unsigned short c_port;

	if(!clientaddr || !first)
		return NULL;

	memcpy(&c_addr, &clientaddr->sin_addr.s_addr, sizeof(unsigned long));
	c_port = clientaddr->sin_port;
	client = first;

	while(client){
		if(c_addr == client->clientaddr.sin_addr.s_addr){
			if(c_port == client->clientaddr.sin_port)
				return client;
		}
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

struct client_entry *client_add(char *name, struct sockaddr_in *clientaddr){
/*
* Allocate new client_entry, fill with client info and add it to the tail of the client list.
* Inits client_list if NULL;
*/
	struct client_entry *new_client;
	struct client_entry *list_tail;

	if(!name || !clientaddr || !hostent){
		error_msg(err2_client);
		return NULL;
	}
	/*Init client_list if empty, in which case list tail is just equal to client_list*/
	if(!client_list){
		if(client_init_list() < 0)
			exit(1);
		list_tail = client_list;
	}else{
		list_tail = client_list_tail();
		new_client = malloc(sizeof(struct client_entry));
		if(!new_client){
			error_msg(NULL);
			exit(1);
		}
		list_tail->next = new_client;
		new_client->prev = list_tail;
	}

	memset(new_client, 0, sizeof(struct client_entry));
	memcpy(new_client->username, name, NAME_LEN);
	memcpy(&new_client->clientaddr, clientaddr, sizeof(struct sockaddr_in));

	return new_client;	
}

int client_remove(struct sockaddr_in *clientaddr){
	struct client_entry *client;

	if(!clientaddr){
		error_msg(err3_client);
		return -1;
	}
	if(!client_list)
		return 0;

	client = client_search(clientaddr, client_list);
	if(!client)
		return -1;

	
	return 0;
}











