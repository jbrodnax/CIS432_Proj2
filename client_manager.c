#include "server.h"

char client_err1[]="Client list already initialized.";
char client_err2[]="client_add received NULL argument.";
char client_err3[]="client_remove received NULL clientaddr ptr.";
char client_err4[]="client_list is empty.";

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

struct client_entry *client_test_search(char *name, struct _client_manager *clm){
	struct client_entry *client;

	if(!name || !clm)
		return NULL;

	client = clm->list_head;

	while(client){
		if(!strncmp(name, client->username, NAME_LEN))
			return client;
		client = client->next;
	}
	//printf("[-] Client (%s) NOT found.\n", name);
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

struct client_entry *client_list_tail(struct _client_manager *clm){
/*Returns ptr to last client_entry in list*/
	struct client_entry *client;

	if(!clm)
		return NULL;
	else
		client = clm->list_head;

	while(client->next)
		client = client->next;

	clm->list_tail = client;

	return client;
}

struct client_entry *client_add(char *name, struct sockaddr_in *clientaddr, struct _client_manager *clm){
/*
* Allocate new client_entry, fill with client info and add it to the tail of the client list.
* Inits client_list if NULL;
*/
	struct client_entry *new_client;

	if(!name || !clientaddr){
		error_msg(client_err2);
		return NULL;
	}

	if(!clm->list_head){
		new_client = malloc(sizeof(struct client_entry));
		if(!new_client){
			error_msg(NULL);
			exit(1);
		}
		clm->list_head = new_client;	
	}else{
		if(client_test_search(name, clm)){
			printf("[-] Client (%s) already exists.\n", name);
			return NULL;
		}
		new_client = malloc(sizeof(struct client_entry));
		if(!new_client){
			error_msg(NULL);
			exit(1);
		}
		if(!clm->list_tail)
			client_list_tail(clm);

		clm->list_tail->next = new_client;
		new_client->prev = clm->list_tail;
	}
	memset(new_client, 0, NAME_LEN);
	memcpy(new_client->username, name, NAME_LEN);
	memcpy(&new_client->clientaddr, clientaddr, sizeof(struct sockaddr_in));

	/*Update list tail and number of clients*/
	clm->list_tail = new_client;
	clm->num_clients++;
	//puts("[+] New client added");
	//client_print(new_client);

	return new_client;	
}

//int client_remove(struct sockaddr_in *clientaddr, struct client_entry *client_list){
int client_remove(char *name, struct _client_manager *clm){
	struct client_entry *client;

	//if(!clientaddr){
	if(!name){
		error_msg(client_err3);
		return -1;
	}
	if(!clm){
		error_msg(client_err4);
		return 0;
	}

	//client = client_search(clientaddr, client_list);
	client = client_test_search(name, clm);
	if(!client)	
		return -1;

	/*Unlink list node and update clm head or tail if unlinking head or tail*/
	if(client->next && client->prev){
		client->next->prev = client->prev;
		client->prev->next = client->next;
	}else if(client->next){
		clm->list_head = client->next;
		client->next->prev = NULL;
	}else if(client->prev){
		clm->list_tail = client->prev;
		client->prev->next = NULL;
	}
	clm->num_clients--;
	
	puts("[+] Unlinked client:");
	client_print(client);

	if(client->hostp)
		free(client->hostp);
	free(client);

	return 0;
}

void client_clean(struct _client_manager *clm){
	struct client_entry *client;

	while(1){
		if(!clm->list_head || clm->num_clients < 1)
			break;

		client = clm->list_head;
		if(client->hostp)
			free(client->hostp);

		clm->list_head = client->next;	
		free(client);
		clm->num_clients--;
	}
	puts("[!] Client list is clean.");
	return;
}











