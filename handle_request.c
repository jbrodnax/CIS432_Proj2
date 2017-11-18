#include "server.h"

rid_t handle_request(char *data){
	_sreq_union sreq_union;
	struct client_entry *client;
	struct _adjacent_server *node;
	struct channel_entry *channel;
	struct _queue_entry *entry;
	char channel[NAME_LEN+STR_PADD];
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
		client = client_search(&client_info.clientaddr, &client_manager);
		if(client){
			client_keepalive(client);
			if(type == REQ_LOGIN){
				send_error("You are already logged in.", &client_info.clientaddr, server_info.sockfd);
				return REQ_INVALID;
			}
		}else if(type != REQ_LOGIN){
			send_error("You are not logged in yet.", &client_info.clientaddr, server_info.sockfd);
			return REQ_INVALID;
		}
		switch (type){
			case REQ_LOGIN:
				goto RET;
			case REQ_LOGOUT:
				goto RET;
			case REQ_LIST;
				goto RET;
			case REQ_ALIVE;
				goto RET;
			default:
				memset(channel_name, 0, NAME_LEN+STR_PADD);
				memcpy(channel_name, &data[sizeof(rid_t)], NAME_LEN);
				channel = channel_search(channel_name, &channel_manager);
				if(!channel){
					send_error("Channel does not exist.", &client_info.clientaddr, server_info.sockfd);
					return REQ_INVALID;
				}
		}
		switch (type){
			case REQ_JOIN:
				memcpy(&sreq_union.sreq_name, data, sizeof(struct _req_join));
				printf("Join:\t%d\t%s\n", sreq_union.sreq_name.type_id, sreq_union.sreq_name.name);
				goto RET;
			case REQ_LEAVE:
				memcpy(&sreq_union.sreq_name, data, sizeof(struct _req_leave));
				printf("Leave:\t%d\t%s\n", sreq_union.sreq_name.type_id, sreq_union.sreq_name.name);
				goto RET;
			case REQ_SAY:
				memcpy(&sreq_union.sreq_say, data, sizeof(struct _req_say));
				printf("Say:\t%d\t%s\t%s\n", sreq_union.sreq_say.type_id, sreq_union.sreq_say.channel, sreq_union.sreq_say.text);
				goto RET;
			case REQ_WHO:
				memcpy(&sreq_union.sreq_name, data, sizeof(struct _req_who));
				printf("Who:\t%d\t%s\n", sreq_union.sreq_name.type_id, sreq_union.sreq_name.name);
				goto RET;
			default:
				return REQ_INVALID;
		}
	}

	RET:
		return type;
}







