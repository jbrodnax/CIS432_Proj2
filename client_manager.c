#include "server.h"

char client_err1[]="Client list already initialized.";
char client_err2[]="client_add received NULL argument.";
char client_err3[]="client_remove received NULL clientaddr ptr.";
char client_err4[]="client_list is empty.";

char channel_err1[]="channel_create received null argument.";
char channel_err2[]=".";
char channel_err3[]="channel_add_client received null argument,";
char channel_err4[]="max number of clients on channel reached.";
char channel_err5[]="channel_remove_client received null argument.";
char channel_err6[]="no clients are in this channel.";

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

struct client_entry *client_search(struct sockaddr_in *clientaddr, struct _client_manager *clm){
/*For now, searches for client based on ip addr and port # by iterating through client list*/
	struct client_entry *client;

	if(!clientaddr || !clm)
		return NULL;

	client = clm->list_head;
	while(client){
		if(clientaddr->sin_addr.s_addr == client->clientaddr.sin_addr.s_addr){
			if(clientaddr->sin_port == client->clientaddr.sin_port)
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
		/*if(client_test_search(name, clm)){
			printf("[-] Client (%s) already exists.\n", name);
			return NULL;
		}*/
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
	puts("[+] New client added");
	client_print(new_client);

	return new_client;	
}

//int client_remove(struct sockaddr_in *clientaddr, struct client_entry *client_list){
int client_remove(struct client_entry *client, struct _client_manager *clm){
	//struct client_entry *client;

	//if(!clientaddr){
	if(!client){
		error_msg(client_err3);
		return -1;
	}
	if(!clm){
		error_msg(client_err4);
		return -1;
	}

	//client = client_search(clientaddr, client_list);
	//client = client_test_search(name, clm);

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

struct channel_entry *channel_search(char *name, struct _channel_manager *chm){
	struct channel_entry *channel;

	if(!name || !chm)
		return NULL;

	channel = chm->list_head;
	while(channel){
		if(!strncmp(name, channel->channel_name, NAME_LEN))
			return channel;
		channel = channel->next;
	}
	printf("[-] Channel (%s) NOT found.\n", name);
	return NULL;
}

struct channel_entry *channel_list_tail(struct _channel_manager *chm){
/*Returns ptr to last client_entry in list*/
	struct channel_entry *channel;

	if(!chm)
		return NULL;
	else
		channel = chm->list_head;

	while(channel->next)
		channel = channel->next;

	chm->list_tail = channel;

	return channel;
}

struct channel_entry *channel_create(char *name, struct _channel_manager *chm){
/*
* Allocate new client_entry, fill with client info and add it to the tail of the client list.
* Inits client_list if NULL;
*/
	struct channel_entry *new_channel;

	if(!name || !chm){
		error_msg(channel_err1);
		return NULL;
	}

	if(!chm->list_head){
		new_channel = malloc(sizeof(struct channel_entry));
		if(!new_channel){
			error_msg(NULL);
			exit(1);
		}
		memset(new_channel, 0, sizeof(struct channel_entry));
		chm->list_head = new_channel;	
	}else{
		/*if(client_test_search(name, clm)){
			printf("[-] Client (%s) already exists.\n", name);
			return NULL;
		}*/
		new_channel = malloc(sizeof(struct channel_entry));
		if(!new_channel){
			error_msg(NULL);
			exit(1);
		}
		memset(new_channel, 0, sizeof(struct channel_entry));
		if(!chm->list_tail)
			channel_list_tail(chm);

		chm->list_tail->next = new_channel;
		new_channel->prev = chm->list_tail;
	}
	//memset(new_channel, 0, NAME_LEN+STR_PADD);
	memcpy(new_channel->channel_name, name, NAME_LEN);
	//memcpy(&new_client->clientaddr, clientaddr, sizeof(struct sockaddr_in));

	/*Update list tail and number of clients*/
	chm->list_tail = new_channel;
	chm->num_channels++;
	//puts("[+] New client added");
	//client_print(new_client);

	return new_channel;
}

int channel_remove(struct channel_entry *channel, struct _channel_manager *chm){
	//struct client_entry *client;

	//if(!clientaddr){
	if(!channel){
		error_msg(client_err3);
		return -1;
	}
	if(!chm){
		error_msg(client_err4);
		return -1;
	}

	//client = client_search(clientaddr, client_list);
	//client = client_test_search(name, clm);

	/*Unlink list node and update clm head or tail if unlinking head or tail*/
	if(channel->next && channel->prev){
		channel->next->prev = channel->prev;
		channel->prev->next = channel->next;
	}else if(channel->next){
		chm->list_head = channel->next;
		channel->next->prev = NULL;
	}else if(channel->prev){
		chm->list_tail = channel->prev;
		channel->prev->next = NULL;
	}
	if(chm->num_channels > 0)
		chm->num_channels--;
	
	puts("[+] Unlinked channel:");
	//client_print(client);

	//if(client->hostp)
	//	free(client->hostp);
	free(channel);

	return 0;
}

void channel_clean(struct _channel_manager *chm){
	struct channel_entry *channel;

	while(1){
		if(!chm->list_head || chm->num_channels < 1)
			break;

		channel = chm->list_head;
		
		chm->list_head = channel->next;	
		free(channel);
		chm->num_channels--;
	}
	puts("[!] Channel list is clean.");
	return;
}

int channel_add_client(struct client_entry *client, struct channel_entry *channel){

	if(!client || !channel){
		error_msg(channel_err3);
		return -1;
	}
	/*Max number of clients per channel is actually 1 less than defined since the last array index needs to remain 0 for shifting during removal*/
	if(channel->num_clients >= MAX_CHANNELCLIENTS-1){
		error_msg(channel_err4);
		return -1;
	}

	channel->client_list[channel->num_clients] = client;
	channel->num_clients++;

	return 0;
}

int channel_remove_client(struct client_entry *client, struct channel_entry *channel){
	struct client_entry *current;
	int i;
	int n = 0;

	if(!client || !channel){
		error_msg(channel_err5);
		return -1;
	}
	if(channel->num_clients < 1){
		error_msg(channel_err6);
		return -1;
	}

	while(n < channel->num_clients){
		current = channel->client_list[n];
		if(current->clientaddr.sin_addr.s_addr == client->clientaddr.sin_addr.s_addr){
			if(current->clientaddr.sin_port == client->clientaddr.sin_port)
				break;
		}
		n++;
	}
	if(n < channel->num_clients){
		while(n < channel->num_clients){
			memcpy(channel->client_list[n], channel->client_list[n+1], sizeof(struct client_entry *));
			n++;
		}
	}

	return 0;
}











