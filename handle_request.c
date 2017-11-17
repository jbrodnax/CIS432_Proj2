#include "server.h"

rid_t handle_request(char *data){
	_sreq_union sreq_union;
	struct client_entry *client;
	struct _adjacent_server *node;
	struct channel_entry *channel;
	struct _queue_entry *entry;
	unique_t id;
	rid_t type;

	memset(sreq_union, 0, sizeof(sreq_union));
	memcpy(&type, data, sizeof(rid_t));

	if(type > S2S_Say)
		return REQ_INVALID;
	if(type > REQ_ALIVE){
		goto S2S;
	}
	if(type < S2S_JOIN){
		goto USR;
	}

	S2S:do {

	}

	USR:do {

	}

	RET:
		return type;
}
