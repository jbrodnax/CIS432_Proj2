#include "duckchat.h"

#define TREE_MAX 16
#define UID_MAX 128
typedef uint64_t unique_t;

struct __attribute__((packed)) _S2S_join{
	rid_t type_id;
	char channel[NAME_LEN];
};

struct __attribute__((packed)) _S2S_leave{
	rid_t type_id;
	char channel[NAME_LEN];
};

struct __attribute__((packed)) _S2S_say{
	rid_t type_id;
	unique_t msg_id;
	char username[NAME_LEN];
	char channel[NAME_LEN];
	char text[TEXT_LEN];
};

struct _adjacent_server{
	struct sockaddr_in *serveraddr;
	struct addrinfo hints, *servinfo;
	char hostname[256];
	char ipaddr[64];
	char port_str[32];
	int index;
};

struct _server_manager{
	struct _adjacent_server *tree[TREE_MAX];
	struct _adjacent_server *last_accessed;
	unique_t recent_ids[UID_MAX];
	int num_ids;
	int tree_size;
};

struct client_entry{
	char username[NAME_LEN+STR_PADD];
	struct sockaddr_in clientaddr;
	struct hostent *hostp;
	struct channel_entry *channel_list[MAX_CHANNELCLIENTS];
	int num_channels;
	time_t timestamp;
	struct client_entry *next;
	struct client_entry *prev;
};

struct _client_manager{
	struct client_entry *list_head;
	struct client_entry *list_tail;
	int num_clients;
};

struct _ss_rtable{
	time_t timestamp;
	struct _adjacent_server *rtable_entry;
};

struct channel_entry{
	char channel_name[NAME_LEN+STR_PADD];
	struct client_entry *client_list[MAX_CHANNELCLIENTS];
	struct _adjacent_server *routing_table[TREE_MAX];
	struct _ss_rtable *ss_rtable[TREE_MAX];
	int table_size;
	int num_clients;
	struct channel_entry *next;
	struct channel_entry *prev;
};

struct _channel_manager{
	struct channel_entry *list_head;
	struct channel_entry *list_tail;
	int num_channels;
	uint32_t sub_initchannel;
};

struct _server_info{
	char ipaddr_str[256];
	char portno_str[32];
	char *hostname;
	int sockfd;
	int portno;
	struct sockaddr_in *serveraddr;
	struct sockaddr_in6 *serveraddr6;
};

struct _client_info{
	char ipaddr_str[256];
	char portno_str[32];
	struct sockaddr_in clientaddr;
	struct sockaddr_in6 clientaddr6;
	struct hostent *hostp;
	char *hostaddrp;
	int portno;
};

struct _queue_entry{
	struct sockaddr_in clientaddr;
	char username[NAME_LEN+STR_PADD];
	struct _req_say *req_say;
	struct _req_list *req_list;
	struct _req_who *req_who;
};

struct _req_queue{
	struct _queue_entry *queue[MAXQSIZE];
	int size;
};

struct _req_login* req_login;
struct _req_logout* req_logout;
struct _req_join* req_join;
struct _req_leave* req_leave;
struct _req_say* req_say;
struct _req_list* req_list;
struct _req_who* req_who;
struct _req_alive* req_alive;
struct _req_queue main_queue;

struct _S2S_join *s2s_join;
struct _S2S_leave *s2s_leave;
struct _S2S_say *s2s_say;

struct _client_manager client_manager;
struct _channel_manager channel_manager;
struct _server_info server_info;
struct _client_info client_info;
struct _server_manager server_manager;
struct addrinfo hints, *servinfo, *p;

pthread_mutex_t lock1;
pthread_rwlock_t channel_lock;
pthread_rwlock_t client_lock;
pthread_rwlock_t node_lock;
char *LOG_RECV;
char *LOG_SEND;

/*client_manager.c function prototypes*/
void error_msg(char *err_msg);
//void init_rwlocks();
void client_print(struct client_entry *client);
struct client_entry *client_search(struct sockaddr_in *clientaddr, struct _client_manager *clm);
struct client_entry *client_init_list();
int client_add_channel(struct channel_entry *channel, struct client_entry *client);
int client_remove_channel(struct channel_entry *channel, struct client_entry *client);
struct client_entry *client_list_tail(struct _client_manager *clm);
time_t client_keepalive(struct client_entry *client);
int client_softstate(struct _client_manager *clm, struct _channel_manager *chm);
struct client_entry *client_add(char *name, struct sockaddr_in *clientaddr, struct _client_manager *clm);
int client_logout(struct client_entry *client, struct _client_manager *clm, struct _channel_manager *chm);
int client_remove(struct client_entry *client, struct _client_manager *clm);
void client_clean(struct _client_manager *clm);
struct channel_entry *channel_search(char *name, struct _channel_manager *chm);
struct channel_entry *channel_list_tail(struct _channel_manager *chm);
struct channel_entry *channel_create(char *name, struct _channel_manager *chm, struct _server_manager *svm);
int channel_remove(struct channel_entry *channel, struct _channel_manager *chm);
void channel_clean(struct _channel_manager *chm);
int channel_add_client(struct client_entry *client, struct channel_entry *channel);
int channel_remove_client(struct client_entry *client, struct channel_entry *channel);
int channel_list(struct _rsp_list *rsp_list, struct _channel_manager *chm);
int channel_who(struct _rsp_who *rsp_who, struct channel_entry *ch);

/*servertree.c function prototypes*/
struct _adjacent_server *node_create(char *hostname, char *port, struct _server_manager *svm);
int node_add(struct _adjacent_server *node, struct _server_manager *svm);
int node_remove(struct _adjacent_server *node, struct _server_manager *svm);
struct _adjacent_server *node_search(struct sockaddr_in *serveraddr, struct _server_manager *svm);
int node_compare(struct _adjacent_server *node1, struct _adjacent_server *node2);
unique_t generate_id(struct _S2S_say *req);
int rtable_init(struct channel_entry *ch, struct _server_manager *svm);
int rtable_prune(struct channel_entry *ch, struct _adjacent_server *node, struct _server_manager *svm);
int node_keepalive(struct channel_entry *ch, struct _adjacent_server *node);
int channel_softstate(struct _channel_manager *chm);
int resubscribe(struct _channel_manager *chm, int sockfd);
int propogate_join(struct channel_entry *ch, struct _adjacent_server *sender, int sockfd);
int save_id(unique_t id, struct _server_manager *svm);
int propogate_say(struct channel_entry *ch, char *name, unique_t id, _sreq_say *req, struct _adjacent_server *sender, int sockfd, struct _server_manager *svm);
int send_leave(char *ch, struct _adjacent_server *node, int sockfd);

/*server.c function prototypes*/
int send_error(char *errmsg, struct sockaddr_in *clientaddr, int sockfd);
void log_recv();
void log_send();
void *thread_responder(void *vargp);
void *thread_softstate(void *vargp);
rid_t handle_request(char *data);
void recvdata_IPv4();
void recvdata_IPv6();
