#include "server.h"

rid_t handle_request2(char *data){
	_sreq_union sreq_union;
	struct client_entry *client;
	struct _adjacent_server *node;
	struct channel_entry *channel;
	struct _queue_entry *entry;
	char channel_name[NAME_LEN+STR_PADD];
	unique_t id;
	rid_t type;

	memset(&sreq_union, 0, sizeof(sreq_union));
	memcpy(&type, data, sizeof(rid_t));

	if(type > S2S_SAY)
		return REQ_INVALID;
	if(type > REQ_ALIVE){
		goto S2S;
	}
	if(type < S2S_JOIN){
		goto USR;
	}

	S2S:
		if(!(node = node_search(&client_info.clientaddr, &server_manager))){
			if(client_search(&client_info.clientaddr, &client_manager))
				send_error("Invalid request type.", &client_info.clientaddr, server_info.sockfd);
			return REQ_INVALID;
		}
		switch(type){
			case S2S_JOIN:
				memcpy(&sreq_union.sreq_name, data, sizeof(struct _req_join));
				snprintf(LOG_RECV, LOGMSG_LEN, "%s:%s\trecv S2S Join %s", node->ipaddr, node->port_str, sreq_union.sreq_name.name);
				log_recv();
				if(!(channel = channel_search(sreq_union.sreq_name.name, &channel_manager)))
					channel = channel_create(sreq_union.sreq_name.name, &channel_manager, &server_manager);
				propogate_join(channel, &sreq_union.sreq_name, server_info.sockfd);
				goto RET;

			case S2S_LEAVE:
				goto RET;
			case S2S_SAY:
				goto RET;
			default:
				snprintf(LOG_RECV, LOGMSG_LEN, "%s:%s\trecv S2S Invalid", node->ipaddr, node->port_str);
        			log_recv();
				return REQ_INVALID;
		}

	USR:
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
				memcpy(&sreq_union.sreq_name, data, sizeof(struct _req_login));
				snprintf(LOG_RECV, LOGMSG_LEN, "%s:%s\trecv Request Login %s",
					client_info.ipaddr_str, client_info.portno_str, sreq_union.sreq_name.name);
				log_recv();
				client_add(sreq_union.sreq_name.name, &client_info.clientaddr, &client_manager);
				goto RET;

			case REQ_LOGOUT:
				snprintf(LOG_RECV, LOGMSG_LEN, "%s:%s\trecv Request Logout %s", 
					client_info.ipaddr_str, client_info.portno_str, client->username);
				log_recv();
				client_logout(client, &client_manager, &channel_manager);
				goto RET;

			case REQ_LIST:
				if(!(req_list = malloc(sizeof(struct _req_list))))
					goto MEM_ERR;
				memset(req_list, 0, sizeof(struct _req_list));
				req_list->type_id = REQ_LIST;
				snprintf(LOG_RECV, LOGMSG_LEN, "%s:%s\trecv Request List %s", 
					client_info.ipaddr_str, client_info.portno_str, client->username);
				log_recv();
				pthread_mutex_lock(&lock1);
				if(main_queue.size < MAXQSIZE){
					if(!(entry = malloc(sizeof(struct _queue_entry))))
						goto MEM_ERR;
					memset(entry, 0, sizeof(struct _queue_entry));
					entry->req_list = req_list;
					memcpy(&entry->clientaddr, &client->clientaddr, sizeof(struct sockaddr_in));
					main_queue.queue[main_queue.size] = entry;
					main_queue.size++;
				}
				pthread_mutex_unlock(&lock1);
				goto RET;

			case REQ_ALIVE:
				snprintf(LOG_RECV, LOGMSG_LEN, "%s:%s\trecv Request KeepAlive %s", 
					client_info.ipaddr_str, client_info.portno_str, client->username);
				log_recv();
				goto RET;

		}
		switch (type){
			case REQ_JOIN:
				memcpy(&sreq_union.sreq_name, data, sizeof(struct _req_join));
				printf("Join:\t%d\t%s\n", sreq_union.sreq_name.type_id, sreq_union.sreq_name.name);
				if(!(channel = channel_search(sreq_union.sreq_name.name, &channel_manager))){
					channel = channel_create(sreq_union.sreq_name.name, &channel_manager, &server_manager);
					//if(!(s2s_join = malloc(sizeof(struct _S2S_join))))
					//	goto MEM_ERR;
					//s2s_join->type_id = S2S_JOIN;
					//memcpy(s2s_join->channel, channel->channel_name, NAME_LEN);
					propogate_join(channel, &sreq_union.sreq_name, server_info.sockfd);
					//free(s2s_join);
				}
				client_add_channel(channel, client);
				channel_add_client(client, channel);
				goto RET;

			case REQ_LEAVE:
				memcpy(&sreq_union.sreq_name, data, sizeof(struct _req_leave));
				printf("Leave:\t%d\t%s\n", sreq_union.sreq_name.type_id, sreq_union.sreq_name.name);
				if(!(channel = channel_search(sreq_union.sreq_name.name, &channel_manager)))
					goto CHDNE;
				client_remove_channel(channel, client);
				channel_remove_client(client, channel);
				if(channel->num_clients < 1)
					channel_remove(channel, &channel_manager);
				goto RET;
			case REQ_SAY:
				memcpy(&sreq_union.sreq_say, data, (sizeof(rid_t)+NAME_LEN));
				memcpy(&sreq_union.sreq_say.text, &data[(sizeof(rid_t)+NAME_LEN)], TEXT_LEN);
				printf("Say:\t%d\t%s\t%s\n", sreq_union.sreq_say.type_id, sreq_union.sreq_say.channel, sreq_union.sreq_say.text);
				if(!(channel = channel_search(sreq_union.sreq_say.channel, &channel_manager)))
					goto CHDNE;
				propogate_say(NULL, client->username, &sreq_union.sreq_say, server_info.sockfd, &server_manager);
				if(!(req_say = malloc(sizeof(struct _req_say))))
					goto MEM_ERR;
				memset(req_say, 0, sizeof(struct _req_say));
				memcpy(req_say, data, (sizeof(rid_t)+NAME_LEN));
				memcpy(req_say->text, sreq_union.sreq_say.text, TEXT_LEN);
				pthread_mutex_lock(&lock1);
				if(main_queue.size < MAXQSIZE){
					if(!(entry = malloc(sizeof(struct _queue_entry))))
						goto MEM_ERR;
					memset(entry, 0, sizeof(struct _queue_entry));
					entry->req_say = req_say;
					memcpy(entry->username, client->username, NAME_LEN);
					memcpy(&entry->clientaddr, &client->clientaddr, sizeof(struct sockaddr_in));
					main_queue.queue[main_queue.size] = entry;
					main_queue.size++;
				}
				pthread_mutex_unlock(&lock1);
				goto RET;

			case REQ_WHO:
				memcpy(&sreq_union.sreq_name, data, sizeof(struct _req_who));
				printf("Who:\t%d\t%s\n", sreq_union.sreq_name.type_id, sreq_union.sreq_name.name);
				if(!(channel = channel_search(sreq_union.sreq_name.name, &channel_manager)))
					goto CHDNE;
				if(!(req_who = malloc(sizeof(struct _req_who))))
					goto MEM_ERR;
				memset(req_who, 0, sizeof(struct _req_who));
				memcpy(req_who, &sreq_union.sreq_name, sizeof(struct _req_who));
				if(main_queue.size < MAXQSIZE){
					if(!(entry = malloc(sizeof(struct _queue_entry))))
						goto MEM_ERR;
					memset(entry, 0, sizeof(struct _queue_entry));
					entry->req_who = req_who;
					memcpy(&entry->clientaddr, &client->clientaddr, sizeof(struct sockaddr_in));
					main_queue.queue[main_queue.size] = entry;
					main_queue.size++;
				}
				pthread_mutex_unlock(&lock1);
				goto RET;

			default:
				return REQ_INVALID;
		}

	CHDNE:
		send_error("Channel does not exist.", &client_info.clientaddr, server_info.sockfd);
		return REQ_INVALID;
	MEM_ERR:
		perror("Error in malloc");
		exit(EXIT_FAILURE);
	RET:
		return type;
}










