#include "duckchat.h"

struct client_entry{
	char username[NAME_LEN];
	struct sockaddr_in clientaddr;
	char *hostent;
	struct client_info *next;
	struct client_info *prev;
};

struct client_list_entry{
	struct client_info *client;
	struct client_info *next;
	struct client_info *prev;
};

struct client_entry *client_list;

struct client_entry *search_clients(char *name, struct client_entry *first){
//FIX: remove refs to client_list_entry structs
	char * username;
	struct client_list_entry *clnt_entry;
	struct client_info *clnt;

	if(!name || !list)
		return NULL;

	username = name;
	clnt_entry = list;
	if(clnt_entry->client)
		clnt = clnt_entry->client;

	while(clnt){
		if(!strncmp(clnt->username, username, NAME_LEN))
			return clnt;

		clnt_entry = clnt_entry->next;
		if(!clnt_entry)
			break;
		clnt = clnt_entry->client;
	}

	return NULL;
}
