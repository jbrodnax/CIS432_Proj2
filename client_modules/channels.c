#include "../client.h"

void error_msg(char *err_msg){
	if(!err_msg){
		perror("[-] ERROR:\n\t ");
		return;
	}

	printf("[-] ERROR:\n\t %s\n", err_msg);
	return;
}

struct _channel_sub *channel_search(char *name, struct _client_info *cl){
	struct _channel_sub *ch;
	size_t n;

	if(!name || !cl){
		error_msg("channel search received null argument.");
		return NULL;
	}
	if((cl->list_head == NULL) || (cl->num_channels == 0)){	
		return NULL;
	}
	n = strlen(name);
	if(n < 1 || n > NAME_LEN){
		error_msg("Invalid channel name.");
		return NULL;
	}

	ch = cl->list_head;
	while(ch){
		if(!strncmp(ch->channel_name, name, NAME_LEN)){
			return ch;
		}
		ch = ch->next;
	}
	//printf("[-] Channel (%s) not found.\n", name);

	return NULL;
}

struct _channel_sub *channel_switch(char *name, struct _client_info *cl){
	struct _channel_sub *ch;

	ch = channel_search(name, cl);
	if(ch){
		cl->active_channel = ch;
		return ch;
	}

	return NULL;
}

struct _channel_sub *channel_list_tail(struct _client_info *cl){
	struct _channel_sub *ch;

	if(!cl)
		return NULL;
	else
		ch = cl->list_head;

	while(ch->next)
		ch = ch->next;
	cl->list_tail = ch;

	return ch;
}

struct _channel_sub *channel_add(char *name, struct _client_info *cl){
	struct _channel_sub *new_ch;
	size_t n;

	if(!name || !cl){
		error_msg("channel_add received null argument.");
		return NULL;
	}
	n = strlen(name);
	if(n < 1 || n > NAME_LEN){
		error_msg("Invalid channel name. Name must be between 1 and 32 byes long.");
		return NULL;
	}
	new_ch = channel_search(name, cl);
	if(new_ch){
		//printf("Already subscribed to channel (%s).", name);
		return new_ch;
	}
	/*If list_head is empty, assume client is not subscribed to any channels. list is empty*/
	if(!cl->list_head){
		new_ch = malloc(sizeof(struct _channel_sub));
		if(!new_ch){
			error_msg(NULL);
			exit(EXIT_FAILURE);
		}
		memset(new_ch, 0, sizeof(struct _channel_sub));
		cl->list_head = new_ch;
	}else{
		new_ch = malloc(sizeof(struct _channel_sub));
		if(!new_ch){
			error_msg(NULL);
			exit(EXIT_FAILURE);
		}
		memset(new_ch, 0, sizeof(struct _channel_sub));
		if(!cl->list_tail)
			channel_list_tail(cl);
		cl->list_tail->next = new_ch;
		new_ch->prev = cl->list_tail;
	}

	strncpy(new_ch->channel_name, name, NAME_LEN);
	cl->list_tail = new_ch;
	cl->num_channels++;
	//printf("[+] Now subscribbed to channel (%s).\n", new_ch->channel_name);

	return new_ch;
}

int channel_remove(struct _channel_sub *channel, struct _client_info *cl){

	if(!cl || !channel){
		error_msg("channel_remove received null argument.");
		return -1;
	}

	if(channel->next && channel->prev){
		channel->next->prev = channel->prev;
		channel->prev->next = channel->next;
	}else if(channel->next){
		cl->list_head = channel->next;
		channel->next->prev = NULL;
	}else if(channel->prev){
		cl->list_tail = channel->prev;
		channel->prev->next = NULL;
	}else{
		cl->list_tail = NULL;
		cl->list_head = NULL;
	}
	if(cl->num_channels > 0)
		cl->num_channels--;
	//printf("[+] Unlinked channel (%s).\n", channel->channel_name);
	free(channel);

	return 0;
}

void channel_clean(struct _client_info *cl){
	struct _channel_sub *channel;

	while(1){
		if(!cl->list_head || cl->num_channels < 1)
			break;
		channel = cl->list_head;
		cl->list_head = channel->next;
		//printf("[+] Freeing channel (%s).\n", channel->channel_name);
		free(channel);
		cl->num_channels--;
	}
	cl->list_head = NULL;
	cl->list_tail = NULL;
	//puts("[+] Sub channel is clean.");

	return;
}







