#include "duckchat.h"

struct client_entry{
	char username[NAME_LEN+STR_PADD];
	struct sockaddr_in clientaddr;
	struct hostent *hostp;
	struct channel_entry *channel_list[MAX_CHANNELCLIENTS];
	int num_channels;
	struct client_entry *next;
	struct client_entry *prev;
};

struct _client_manager{
	struct client_entry *list_head;
	struct client_entry *list_tail;
	int num_clients;
};

struct channel_entry{
	char channel_name[NAME_LEN+STR_PADD];
	struct client_entry *client_list[MAX_CHANNELCLIENTS];
	int num_clients;
	struct channel_entry *next;
	struct channel_entry *prev;
};

struct _channel_manager{
	struct channel_entry *list_head;
	struct channel_entry *list_tail;
	int num_channels;
};

/*client_manager.c function prototypes*/
void client_print(struct client_entry *client);
struct client_entry *client_search(struct sockaddr_in *clientaddr, struct _client_manager *clm);
struct client_entry *client_init_list();
int client_add_channel(struct channel_entry *channel, struct client_entry *client);
int client_remove_channel(struct channel_entry *channel, struct client_entry *client);
struct client_entry *client_list_tail(struct _client_manager *clm);
struct client_entry *client_add(char *name, struct sockaddr_in *clientaddr, struct _client_manager *clm);
int client_remove(struct client_entry *client, struct _client_manager *clm);
void client_clean(struct _client_manager *clm);
struct channel_entry *channel_search(char *name, struct _channel_manager *chm);
struct channel_entry *channel_list_tail(struct _channel_manager *chm);
struct channel_entry *channel_create(char *name, struct _channel_manager *chm);
int channel_remove(struct channel_entry *channel, struct _channel_manager *chm);
void channel_clean(struct _channel_manager *chm);
int channel_add_client(struct client_entry *client, struct channel_entry *channel);
int channel_remove_client(struct client_entry *client, struct channel_entry *channel);

/*server.c function prototypes*/
void *thread_responder(void *vargp);
void recvdata_IPv4();
void recvdata_IPv6();
