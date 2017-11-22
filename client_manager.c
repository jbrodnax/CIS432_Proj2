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
		perror("[-] ERROR:\t ");
		return;
	}

	printf("[-] ERROR:\t%s\n", err_msg);
	return;
}

struct client_entry *client_search(struct sockaddr_in *clientaddr, struct _client_manager *clm){
/*Searches for client based on ip addr and port # by iterating through client list*/
	struct client_entry *client;

	if(!clientaddr || !clm)
		return NULL;

	if(clm->num_clients < 1)
		return NULL;

	pthread_rwlock_rdlock(&client_lock);
	client = clm->list_head;
	while(client){
		if(clientaddr->sin_addr.s_addr == client->clientaddr.sin_addr.s_addr){
			if(clientaddr->sin_port == client->clientaddr.sin_port){
				pthread_rwlock_unlock(&client_lock);
				return client;
			}
		}
		client = client->next;
	}

	pthread_rwlock_unlock(&client_lock);
	return NULL;
}

int client_add_channel(struct channel_entry *channel, struct client_entry *client){
	int i;

	if(!channel || !client){
		error_msg("client_add_channel received null argument.");
		return -1;
	}
	if(client->num_channels >= (MAX_CHANNELCLIENTS-1)){
		error_msg("client is subscribed to max number of channels.");
		return -1;
	}

	pthread_rwlock_rdlock(&client_lock);
	for(i=0; i < client->num_channels; i++){
		if(!strncmp(client->channel_list[i]->channel_name, channel->channel_name, NAME_LEN)){
			error_msg("client is already subscribed to channel. Ignoring request.");
			pthread_rwlock_unlock(&client_lock);
			return 1;
		}
	}
	pthread_rwlock_unlock(&client_lock);

	pthread_rwlock_wrlock(&client_lock);
	client->channel_list[client->num_channels] = channel;
	client->num_channels++;
	//printf("[+] Client (%s) added to channel (%s).\n", client->username, channel->channel_name);

	pthread_rwlock_unlock(&client_lock);
	return 0;
}

int client_remove_channel(struct channel_entry *channel, struct client_entry *client){
	struct channel_entry *current;
	int n = 0;

	if(!client || !channel){
		error_msg("client_remove_channel received null argument.");
		return -1;
	}
	if(client->num_channels < 1){
		error_msg("client is not subscribed to any channels.");
		return -1;
	}

	pthread_rwlock_wrlock(&client_lock);
	while(n < client->num_channels){
		current = client->channel_list[n];
		if(!memcmp(current->channel_name, channel->channel_name, NAME_LEN)){
			//printf("[-] Client (%s) removed from channel (%s).\n", client->username, channel->channel_name);
			break;
		}
		n++;
	}
	while(n < client->num_channels){
		memcpy(&client->channel_list[n], &client->channel_list[n+1], sizeof(struct channel_entry *));
		n++;
	}
	client->num_channels--;

	pthread_rwlock_unlock(&client_lock);
	return 0;
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

int client_softstate(struct _client_manager *clm, struct _channel_manager *chm){
	struct client_entry *client;	
	time_t current_time;
	char log_msg[LOGMSG_LEN];

	if(!clm || !chm){
		error_msg("client_softstate received null argument.");
		return -1;
	}

	pthread_rwlock_rdlock(&client_lock);
	current_time = time(NULL);
	client = clm->list_head;
	while(client){
		if((current_time - client->timestamp) >= SS_TIMEOUT){
			pthread_rwlock_unlock(&client_lock);
			snprintf(log_msg, LOGMSG_LEN, "client %s timed-out", client->username);
			log_thread(log_msg);
			client_logout(client, clm);
			pthread_rwlock_rdlock(&client_lock);
		}
		client = client->next;
	}

	pthread_rwlock_unlock(&client_lock);
	return 0;
}

time_t client_keepalive(struct client_entry *client){
	time_t current_time;

	if(!client){
		error_msg("client_keepalive received null client argument.");
		return 0;
	}

	pthread_rwlock_wrlock(&client_lock);
	current_time = time(NULL);
	if(current_time < 1 || current_time < client->timestamp){
		error_msg("[!] System error: time() returned inaccurate time.");
		exit(EXIT_FAILURE);
	}
	client->timestamp = current_time;

	pthread_rwlock_unlock(&client_lock);
	return current_time;
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

	pthread_rwlock_wrlock(&client_lock);
	if(!clm->list_head){
		new_client = malloc(sizeof(struct client_entry));
		if(!new_client){
			error_msg(NULL);
			exit(1);
		}
		memset(new_client, 0, sizeof(struct client_entry));
		clm->list_head = new_client;
		clm->list_tail = new_client;
	}else{
		new_client = malloc(sizeof(struct client_entry));
		if(!new_client){
			error_msg(NULL);
			exit(1);
		}
		memset(new_client, 0, sizeof(struct client_entry));
		if(!clm->list_tail)
			client_list_tail(clm);

		clm->list_tail->next = new_client;
		new_client->prev = clm->list_tail;
	}
	memcpy(new_client->username, name, NAME_LEN);
	memcpy(&new_client->clientaddr, clientaddr, sizeof(struct sockaddr_in));
	new_client->timestamp = time(NULL);
	/*Update list tail and number of clients*/
	clm->list_tail = new_client;
	clm->num_clients++;

	pthread_rwlock_unlock(&client_lock);
	return new_client;	
}

int client_remove(struct client_entry *client, struct _client_manager *clm){

	if(!client){
		error_msg(client_err3);
		return -1;
	}
	if(!clm){
		error_msg(client_err4);
		return -1;
	}

	/*Unlink list node and update clm head or tail if unlinking head or tail*/
	pthread_rwlock_wrlock(&client_lock);
	if(client->next && client->prev){
		client->next->prev = client->prev;
		client->prev->next = client->next;
	}else if(client->next){
		clm->list_head = client->next;
		client->next->prev = NULL;
	}else if(client->prev){
		clm->list_tail = client->prev;
		client->prev->next = NULL;
	}else{
		clm->list_head = NULL;
		clm->list_tail = NULL;
	}
	if(clm->num_clients > 0)
		clm->num_clients--;

	free(client);

	pthread_rwlock_unlock(&client_lock);
	return 0;
}

int client_logout(struct client_entry *client, struct _client_manager *clm){
	struct channel_entry *ch;
	struct channel_entry *channel_list[MAX_CHANNELCLIENTS];
	int i;

	if(!client || !clm){
		error_msg("client_logout received null argument.");
		return -1;
	}

	pthread_rwlock_rdlock(&client_lock);
	if(client->num_channels > 0){
		memcpy(channel_list, client->channel_list, (sizeof(struct channel_entry *)*MAX_CHANNELCLIENTS));
		pthread_rwlock_unlock(&client_lock);
		/*Remove client from all channels they were subscribbed to*/
		i = 0;
		ch = channel_list[0];
		while(ch){
			channel_remove_client(client, ch);
			i++;
			ch = channel_list[i];
		}
		client_remove(client, clm);
	}else{
		pthread_rwlock_unlock(&client_lock);
		client_remove(client, clm);
	}

	return 0;
}

void client_clean(struct _client_manager *clm){
	struct client_entry *client;

	pthread_rwlock_wrlock(&client_lock);
	while(1){
		if(!clm->list_head || clm->num_clients < 1)
			break;
		client = clm->list_head;
		clm->list_head = client->next;	
		free(client);
		clm->num_clients--;
	}
	puts("[!] Client list is clean.");

	pthread_rwlock_unlock(&client_lock);
	return;
}

struct channel_entry *channel_search(char *name, struct _channel_manager *chm){
	struct channel_entry *channel;

	if(!name || !chm)
		return NULL;

	pthread_rwlock_rdlock(&channel_lock);
	channel = chm->list_head;
	while(channel){
		if(!strncmp(name, channel->channel_name, NAME_LEN)){
			pthread_rwlock_unlock(&channel_lock);
			return channel;
		}
		channel = channel->next;
	}
	//printf("[-] Channel (%s) NOT found.\n", name);

	pthread_rwlock_unlock(&channel_lock);	
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

struct channel_entry *channel_create(char *name, struct _channel_manager *chm, struct _server_manager *svm){
/*
* Allocate new channel_entry, fill with channel info and add it to the tail of the list.
* Inits channel_list if empty;
*/
	struct channel_entry *new_channel;

	if(!name || !chm || !svm){
		error_msg(channel_err1);
		return NULL;
	}

	pthread_rwlock_wrlock(&channel_lock);
	if(!chm->list_head){
		new_channel = malloc(sizeof(struct channel_entry));
		if(!new_channel){
			error_msg(NULL);
			exit(EXIT_FAILURE);
		}
		memset(new_channel, 0, sizeof(struct channel_entry));
		chm->list_head = new_channel;
		chm->list_tail = new_channel;
	}else{
		new_channel = malloc(sizeof(struct channel_entry));
		if(!new_channel){
			error_msg(NULL);
			exit(EXIT_FAILURE);
		}
		memset(new_channel, 0, sizeof(struct channel_entry));
		if(!chm->list_tail)
			channel_list_tail(chm);

		chm->list_tail->next = new_channel;
		new_channel->prev = chm->list_tail;
	}
	memcpy(new_channel->channel_name, name, NAME_LEN);

	/*Update list tail and number of clients*/
	chm->list_tail = new_channel;
	chm->num_channels++;
	new_channel->num_clients = 0;
	rtable_init(new_channel, svm);

	//printf("[+] New channel (%s) created.\n", new_channel->channel_name);

	pthread_rwlock_unlock(&channel_lock);
	return new_channel;
}

int channel_remove(struct channel_entry *channel, struct _channel_manager *chm){

	if(!channel){
		error_msg(client_err3);
		return -1;
	}
	if(!chm){
		error_msg(client_err4);
		return -1;
	}

	pthread_rwlock_wrlock(&channel_lock);
	/*Unlink list node and update chm head or tail if unlinking head or tail*/
	if(channel->next && channel->prev){
		channel->next->prev = channel->prev;
		channel->prev->next = channel->next;
	}else if(channel->next){
		chm->list_head = channel->next;
		channel->next->prev = NULL;
	}else if(channel->prev){
		chm->list_tail = channel->prev;
		channel->prev->next = NULL;
	}else{
		chm->list_head = NULL;
		chm->list_tail = NULL;
	}
	if(chm->num_channels > 0)
		chm->num_channels--;
	//printf("[+] Channel (%s) has been removed.\n", channel->channel_name);
	free(channel);

	pthread_rwlock_unlock(&channel_lock);
	return 0;
}

void channel_clean(struct _channel_manager *chm){
	struct channel_entry *channel;

	pthread_rwlock_wrlock(&channel_lock);
	while(1){
		if(!chm->list_head || chm->num_channels < 1)
			break;

		channel = chm->list_head;
		
		chm->list_head = channel->next;	
		free(channel);
		chm->num_channels--;
	}
	puts("[!] Channel list is clean.");

	pthread_rwlock_unlock(&channel_lock);
	return;
}

int channel_add_client(struct client_entry *client, struct channel_entry *channel){

	if(!client || !channel){
		error_msg(channel_err3);
		return -1;
	}
	pthread_rwlock_wrlock(&channel_lock);
	/*Max number of clients per channel is actually 1 less than defined since the last array index needs to remain 0 for shifting during removal*/
	if(channel->num_clients >= MAX_CHANNELCLIENTS-1){
		error_msg(channel_err4);
		return -1;
	}

	channel->client_list[channel->num_clients] = client;
	channel->num_clients++;
	//printf("[+] Channel (%s) accepted client (%s).\n", channel->channel_name, client->username);

	pthread_rwlock_unlock(&channel_lock);
	return 0;
}

int channel_remove_client(struct client_entry *client, struct channel_entry *channel){
	struct client_entry *current;	
	int n = 0;

	if(!client || !channel){
		error_msg(channel_err5);
		return -1;
	}

	pthread_rwlock_wrlock(&channel_lock);
	if(channel->num_clients < 1){
		pthread_rwlock_unlock(&channel_lock);
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
	while(n < channel->num_clients){
		memcpy(&channel->client_list[n], &channel->client_list[n+1], sizeof(struct client_entry *));
		n++;
	}
	if(channel->num_clients > 0)
		channel->num_clients--;

	pthread_rwlock_unlock(&channel_lock);
	return 0;
}

int channel_list(struct _rsp_list *rsp_list, struct _channel_manager *chm){
	struct channel_entry *ch;
	int offset;

	if(!rsp_list || !chm){
		error_msg("channel_list received null argument.");
		return -1;
	}

	offset = 0;
	rsp_list->num_channels = 0;

	pthread_rwlock_rdlock(&channel_lock);
	ch = chm->list_head;
	while(ch){
		if(rsp_list->num_channels > LIST_LEN){
			error_msg("list response has larger number of channel names than allowed.");
			break;
		}
		memcpy(&rsp_list->channel_list[offset], ch->channel_name, NAME_LEN);
		rsp_list->num_channels++;
		offset+=NAME_LEN;
		ch = ch->next;
	}

	pthread_rwlock_unlock(&channel_lock);
	return 0;
}

int channel_who(struct _rsp_who *rsp_who, struct channel_entry *ch){
	struct client_entry *client;
	int i, offset;

	if(!rsp_who || !ch){
		error_msg("channel_who received null argument.");
		return -1;
	}

	offset = 0;
	rsp_who->num_users = 0;

	pthread_rwlock_rdlock(&channel_lock);
	if(ch->num_clients > WHO_LEN){
		error_msg("channel_who found channel with invalid number of clients.");
		pthread_rwlock_unlock(&channel_lock);
		return -1;
	}
	pthread_rwlock_rdlock(&client_lock);
	for(i=0; i < ch->num_clients; i++){
		client = ch->client_list[i];
		if(client){
			memcpy(&rsp_who->users[offset], client->username, NAME_LEN);
			rsp_who->num_users++;
			offset+=NAME_LEN;
		}
	}

	pthread_rwlock_unlock(&client_lock);
	pthread_rwlock_unlock(&channel_lock);
	return 0;
}








