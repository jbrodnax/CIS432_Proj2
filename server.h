#include "duckchat.h"

struct client_entry{
	char username[NAME_LEN+STR_PADD];
	struct sockaddr_in clientaddr;
	struct hostent *hostp;
	struct client_entry *next;
	struct client_entry *prev;
};

struct client_entry *client_search(struct sockaddr_in *clientaddr, struct client_entry *first);
int client_init_list(struct client_entry *client_list);
struct client_entry *client_list_tail(struct client_entry *client_list);
struct client_entry *client_add(char *name, struct sockaddr_in *clientaddr, struct client_entry *client_list);
//int client_remove(struct sockaddr_in *clientaddr, struct client_entry *client_list);
int client_remove(char *name, struct client_entry *client_list);
void client_clean(struct client_entry *client_list);
