#include "server.h"

char client_err1[]="Client list already initialized.";
char client_err2[]="client_add received NULL argument.";
char client_err3[]="client_remove received NULL clientaddr ptr.";

void error_msg(char *err_msg){
	if(!err_msg){
		perror("[-] ERROR:\n\t ");
		return;
	}

	printf("[-] ERROR:\n\t %s\n", err_msg);
	return;
}

void client_print(struct client_entry *client){
	if(client){
		printf("[+] CLIENT-INFO:\n\tusername: %s\n", client->username);
	}
}

struct client_entry *client_test_search(char *name, struct client_entry *first){
	struct client_entry *client;

	if(!name || !first)
		return NULL;

	while(client){
		if(!strncmp(name, client->username, NAME_LEN))
			return client;
		client = client->next;
	}
	return NULL;
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

int client_init_list(struct client_entry *client_list){
/*If client_list is empty, allocate space for first client and zero it out*/
	if(client_list){
		error_msg(client_err1);
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

struct client_entry *client_list_tail(struct client_entry *client_list){
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

struct client_entry *client_add(char *name, struct sockaddr_in *clientaddr, struct client_entry *client_list){
/*
* Allocate new client_entry, fill with client info and add it to the tail of the client list.
* Inits client_list if NULL;
*/
	struct client_entry *new_client;
	struct client_entry *list_tail;

	if(!name || !clientaddr){
		error_msg(client_err2);
		return NULL;
	}
	/*Init client_list if empty, in which case list tail is just equal to client_list*/
	if(!client_list){
		if(client_init_list(client_list) < 0)
			exit(1);
		list_tail = client_list;
	}else{
		list_tail = client_list_tail(client_list);
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

//int client_remove(struct sockaddr_in *clientaddr, struct client_entry *client_list){
int client_remove(char *name, struct client_entry *client_list){
	struct client_entry *client;

	//if(!clientaddr){
	if(!name){
		error_msg(client_err3);
		return -1;
	}
	if(!client_list)
		return 0;

	//client = client_search(clientaddr, client_list);
	client = client_test_search(name, client_list);
	if(!client)
		return -1;

	/*Unlink list node*/
	if(client->next && client->prev){
		client->next->prev = client->prev;
		client->prev->next = client->next;
	}else if(client->next){
		client->next->prev = NULL;
	}else if(client->prev){
		client->prev->next = NULL;
	}

	if(client->hostp)
		free(client->hostp);
	free(client);

	return 0;
}

void client_clean(struct client_entry *client_list){
	struct client_entry *client;
	struct client_entry *next_client;

	if(!client_list)
		return;

	client = client_list;
	while(client){
		if(client->hostp)
			free(client->hostp);
		next_client = client->next;
		free(client);
		client = next_client;
	}

	return;
}











