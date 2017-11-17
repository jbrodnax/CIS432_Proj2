#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <netdb.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>

#ifndef DUCKCHAT_H
#define DUCKCHAT_H
#endif

/*Defines max length for both user and channel names*/
#define NAME_LEN	32
#define TEXT_LEN	64
#define LIST_LEN	128
#define WHO_LEN		128
#define BUFSIZE		1024
#define MAX_CHANNELCLIENTS	128
#define MAXQSIZE	128
#define MAXCMD_LEN	8
#define LOGMSG_LEN	256
#define SS_TIMEOUT	30
/*Null-byte padding size for strings originating from user data*/
#define STR_PADD	4

/*Define request types sent from client to server*/
#define REQ_LOGIN	0
#define REQ_LOGOUT	1
#define REQ_JOIN	2
#define REQ_LEAVE	3
#define REQ_SAY		4
#define REQ_LIST	5
#define REQ_WHO		6
#define REQ_ALIVE	7
#define S2S_JOIN	8
#define S2S_LEAVE	9
#define S2S_SAY		10
#define REQ_INVALID	11	//This is not used as a request type, just as a return value for invalid client requests
/*Define response types sent from server to client*/
#define RSP_SAY		0
#define RSP_LIST	1
#define RSP_WHO		2
#define RSP_ERR		3

#define RET_FAILURE	99
/*Request/Response identifier types length*/
typedef uint32_t rid_t;

struct __attribute__((packed)) _req_login {
	rid_t type_id;
	char username[NAME_LEN];
};

struct __attribute__((packed)) _req_logout {
	rid_t type_id;
};

struct __attribute__((packed)) _req_join {
	rid_t type_id;
	char channel[NAME_LEN];
};

struct __attribute__((packed)) _req_leave {
	rid_t type_id;
	char channel[NAME_LEN];
};

struct __attribute__((packed)) _req_say {
	rid_t type_id;
	char channel[NAME_LEN];
	char text[TEXT_LEN];
};

struct __attribute__((packed)) _req_list {
	rid_t type_id;
};

struct __attribute__((packed)) _req_who {
	rid_t type_id;
	char channel[NAME_LEN];
};

struct __attribute__((packed)) _req_alive {
	rid_t type_id;
};

/*Struct defs for server responses*/
struct __attribute__((packed)) _rsp_say {
	rid_t type_id;
	char channel[NAME_LEN];
	char username[NAME_LEN];
	char text[TEXT_LEN];
};

struct __attribute__((packed)) _rsp_list {
	rid_t type_id;
	rid_t num_channels;
	char channel_list[(NAME_LEN*LIST_LEN)];
};

struct __attribute__((packed)) _rsp_who {
	rid_t type_id;
	rid_t num_users;
	char channel[NAME_LEN];
	char users[(NAME_LEN*WHO_LEN)];
};

struct __attribute__((packed)) _rsp_err {
	rid_t type_id;
	char message[TEXT_LEN];
};

typedef struct __attribute__((packed)) __sreq_name {
	rid_t type_id;
	char name[NAME_LEN+STR_PADD];
} _sreq_name;

typedef struct __attribute__((packed)) __sreq_say {
	rid_t type_id;
	char channel[NAME_LEN+STR_PADD];
	char text[TEXT_LEN+STR_PADD];
} _sreq_say;

typedef union __sreq_union {
	_sreq_name sreq_name;
	_sreq_say sreq_say;
} _sreq_union;









