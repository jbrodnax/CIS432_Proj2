#include "server.h"

char common[]="Common";

rid_t handle_request(char *data){
	_sreq_union sreq_union;
	struct client_entry *client;
	struct _adjacent_server *node;
	struct channel_entry *channel;
	struct _queue_entry *entry;
	char channel_name[NAME_LEN+STR_PADD];
	char s2s_username[NAME_LEN+STR_PADD];
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
				if(!(channel = channel_search(sreq_union.sreq_name.name, &channel_manager))){
					channel = channel_create(sreq_union.sreq_name.name, &channel_manager, &server_manager);
					propogate_join(channel, node, server_info.sockfd);
				}else if(channel_manager.sub_initchannel == 0){
					if(!strncmp(channel->channel_name, common, NAME_LEN))
						node_keepalive(channel, node);
						propogate_join(channel, node, server_info.sockfd);
						channel_manager.sub_initchannel = 1;
				}else{
					node_keepalive(channel, node);
				}	
				goto RET;

			case S2S_LEAVE:
				memcpy(&sreq_union.sreq_name, data, sizeof(struct _req_leave));
				snprintf(LOG_RECV, LOGMSG_LEN, "%s:%s\trecv S2S Leave %s", node->ipaddr, node->port_str, sreq_union.sreq_name.name);
				log_recv();
				if(channel = channel_search(sreq_union.sreq_name.name, &channel_manager)){
					if(rtable_prune(channel, node, &server_manager) < 0){
						printf("%s:%s\terror: Failed to prune %s's routing table\n", server_info.ipaddr_str, server_info.portno_str, channel->channel_name);
						return REQ_INVALID;
					}
				}
				goto RET;

			case S2S_SAY:
				memcpy(&id, &data[sizeof(rid_t)], sizeof(unique_t));
				memset(s2s_username, 0, NAME_LEN+STR_PADD);
				memcpy(s2s_username, &data[(sizeof(rid_t)+sizeof(unique_t))], NAME_LEN);
				memcpy(sreq_union.sreq_say.channel, &data[(sizeof(rid_t)+sizeof(unique_t)+NAME_LEN)], NAME_LEN);
				memcpy(sreq_union.sreq_say.text, &data[(sizeof(rid_t)+sizeof(unique_t)+NAME_LEN+NAME_LEN)], TEXT_LEN);
				snprintf(LOG_RECV, LOGMSG_LEN, "%s:%s\trecv S2S Say %s %s %s", node->ipaddr, node->port_str, s2s_username, sreq_union.sreq_say.channel, sreq_union.sreq_say.text);
				log_recv();
				if(save_id(id, &server_manager) > 0){
					send_leave(sreq_union.sreq_say.channel, node, server_info.sockfd);
					goto RET;
				}
				if(channel = channel_search(sreq_union.sreq_say.channel, &channel_manager)){
					//snprintf(LOG_RECV, LOGMSG_LEN, "Channel %s has %d clients and %d table_size.", channel->channel_name, channel->num_clients, channel->table_size);
					//log_recv();
					/*Remove this server from the channel tree if the message can't be forwarded*/
					if(channel->num_clients < 1 && channel->table_size == 1){
						if(node_compare(node, channel->routing_table[0]) == 0){
							channel_remove(channel, &channel_manager);
							send_leave(sreq_union.sreq_say.channel, node, server_info.sockfd);
							goto RET;
						}
					}
					propogate_say(channel, s2s_username, id, &sreq_union.sreq_say, node, server_info.sockfd, NULL);
					if(!(req_say = malloc(sizeof(struct _req_say))))
						goto MEM_ERR;
					req_say->type_id = REQ_SAY;
					memcpy(req_say->channel, sreq_union.sreq_say.channel, NAME_LEN);
					memcpy(req_say->text, sreq_union.sreq_say.text, TEXT_LEN);
					pthread_mutex_lock(&lock1);
					if(main_queue.size < MAXQSIZE){
						if(!(entry = malloc(sizeof(struct _queue_entry))))
							goto MEM_ERR;
						memset(entry, 0, sizeof(struct _queue_entry));
						entry->req_say = req_say;
						memcpy(entry->username, s2s_username, NAME_LEN);
						memcpy(&entry->clientaddr, node->serveraddr, sizeof(struct sockaddr_in));
						main_queue.queue[main_queue.size] = entry;
						main_queue.size++;
					}
					pthread_mutex_unlock(&lock1);
				}	
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

			case REQ_JOIN:
				memcpy(&sreq_union.sreq_name, data, sizeof(struct _req_join));	
				snprintf(LOG_RECV, LOGMSG_LEN, "%s:%s\trecv Request Join %s %s", client_info.ipaddr_str, client_info.portno_str, client->username, sreq_union.sreq_name.name);
				log_recv();
				if(!(channel = channel_search(sreq_union.sreq_name.name, &channel_manager))){
					channel = channel_create(sreq_union.sreq_name.name, &channel_manager, &server_manager);
					propogate_join(channel, NULL, server_info.sockfd);
				}else if(channel_manager.sub_initchannel == 0){
					if(!strncmp(channel->channel_name, common, NAME_LEN)){
						propogate_join(channel, NULL, server_info.sockfd);
						channel_manager.sub_initchannel = 1;
					}
				}
				client_add_channel(channel, client);
				channel_add_client(client, channel);
				goto RET;

			case REQ_LEAVE:
				memcpy(&sreq_union.sreq_name, data, sizeof(struct _req_leave));	
				snprintf(LOG_RECV, LOGMSG_LEN, "%s:%s\trecv Request Leave %s %s", client_info.ipaddr_str, client_info.portno_str, client->username, sreq_union.sreq_name.name);
				log_recv();
				if(!(channel = channel_search(sreq_union.sreq_name.name, &channel_manager)))
					goto CHDNE;
				client_remove_channel(channel, client);
				channel_remove_client(client, channel);
				//if(channel->num_clients < 1)
				//	channel_remove(channel, &channel_manager);
				goto RET;

			case REQ_SAY:
				memcpy(&sreq_union.sreq_say, data, (sizeof(rid_t)+NAME_LEN));
				memcpy(&sreq_union.sreq_say.text, &data[(sizeof(rid_t)+NAME_LEN)], TEXT_LEN);	
				snprintf(LOG_RECV, LOGMSG_LEN, "%s:%s\trecv Request Say %s %s", client_info.ipaddr_str, client_info.portno_str, sreq_union.sreq_say.channel, sreq_union.sreq_say.text);
				log_recv();
				if(!(channel = channel_search(sreq_union.sreq_say.channel, &channel_manager)))
					goto CHDNE;
				propogate_say(NULL, client->username, 0, &sreq_union.sreq_say, NULL, server_info.sockfd, &server_manager);
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
				snprintf(LOG_RECV, LOGMSG_LEN, "%s:%s\trecv Request Who %s %s", client_info.ipaddr_str, client_info.portno_str, client->username, sreq_union.sreq_name.name);
				log_recv();
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










