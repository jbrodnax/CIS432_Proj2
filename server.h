#include "duckchat.h"

struct client_entry{
	char username[NAME_LEN+STR_PADD];
	struct sockaddr_in clientaddr;
	struct hostent *hostp;
	struct client_entry *next;
	struct client_entry *prev;
};

struct _client_manager{
	struct client_entry *list_head;
	struct client_entry *list_tail;
	int num_clients;
};

void client_print(struct client_entry *client);
struct client_entry *client_search(struct sockaddr_in *clientaddr, struct client_entry *first);
struct client_entry *client_init_list();
struct client_entry *client_list_tail(struct _client_manager *clm);
struct client_entry *client_add(char *name, struct sockaddr_in *clientaddr, struct _client_manager *clm);
//int client_remove(struct sockaddr_in *clientaddr, struct client_entry *client_list);
int client_remove(char *name, struct _client_manager *clm);
void client_clean(struct _client_manager *clm);
