#include "server.h"

char channel_err1[]="channel_create received null argument.";

void error_msg(char *err_msg){
	if(!err_msg){
		perror("[-] ERROR:\n\t ");
		return;
	}

	printf("[-] ERROR:\n\t %s\n", err_msg);
	return;
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
		if(!chm->list_tail)
			channel_list_tail(chm);

		chm->list_tail->next = new_channel;
		new_channel->prev = chm->list_tail;
	}
	memset(new_channel, 0, NAME_LEN+STR_PADD);
	memcpy(new_channel->channel_name, name, NAME_LEN);
	//memcpy(&new_client->clientaddr, clientaddr, sizeof(struct sockaddr_in));

	/*Update list tail and number of clients*/
	chm->list_tail = new_channel;
	chm->num_channels++;
	//puts("[+] New client added");
	//client_print(new_client);

	return new_channel;
}
