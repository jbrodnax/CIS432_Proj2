#include "duckchat.h"
#include "raw.h"

#define STDIN 0
#define CSS_TIMEOUT 12

struct _server_info{
	int portno;
	struct sockaddr_in serveraddr;
	struct hostent *server;
	char *hostname;
};

struct _channel_sub{
	char channel_name[NAME_LEN+STR_PADD];
	struct _channel_sub *next;
	struct _channel_sub *prev;
};

struct _client_info{
	char username[NAME_LEN+STR_PADD];
	struct _channel_sub *list_head;
	struct _channel_sub *list_tail;
	struct _channel_sub *active_channel;
	int num_channels;
	time_t timestamp;
};

struct _channel_sub *channel_search(char *name, struct _client_info *cl);
struct _channel_sub *channel_switch(char *name, struct _client_info *cl);
struct _channel_sub *channel_list_tail(struct _client_info *cl);
struct _channel_sub *channel_add(char *name, struct _client_info *cl);
int channel_remove(struct _channel_sub *channel, struct _client_info *cl);
void channel_clean(struct _client_info *cl);
