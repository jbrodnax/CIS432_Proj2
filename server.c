#include "server.h"

pthread_t tid[2];
//pthread_mutex_t lock1 = PTHREAD_MUTEX_INITIALIZER;
char init_channelname[]="Common";
char ERR_MSG[TEXT_LEN];

time_t timestamp;

void error(char *msg){
	perror(msg);
	exit(1);
}

void sig_handler(int signo){
	if(signo == SIGINT){
		printf("SIGINT Caught.\n");
		//FIX: implement clean up
		freeaddrinfo(servinfo);
		exit(EXIT_SUCCESS);
	}
}

void log_recv(){
	printf("%s:%s\t%s\n", server_info.ipaddr_str, server_info.portno_str, LOG_RECV);
	memset(LOG_RECV, 0, LOGMSG_LEN);
	return;
}

void log_send(){
	printf("%s:%s\t%s\n", server_info.ipaddr_str, server_info.portno_str, LOG_SEND);
	memset(LOG_SEND, 0, LOGMSG_LEN);
	return;
}

void test_chm(){
	init_rwlocks();
	if(!channel_create(init_channelname, &channel_manager, &server_manager)){
		fprintf(stderr, "[!] Error: Channel creation of (%s) failed.");
		exit(EXIT_FAILURE);
	}
	return;
}

void init_servertree(int argc, char *argv[]){
	char *hostname;
	char *port;
	struct _adjacent_server *node;
	int n;

	memset(&server_manager, 0, sizeof(struct _server_manager));
	for(n=3; n < argc; n+=2){
		if(strlen(argv[n]) > 0 && strlen(argv[n+1]) > 0){
			hostname = argv[n];
			port = argv[n+1];
			node = node_create(hostname, port, &server_manager);
			if(node){
				printf("Adjacent server node on bound socket \t%s:%d\n", node->ipaddr, ntohs(node->serveraddr->sin_port));
				node_add(node, &server_manager);
			}else{
				fprintf(stderr, "[!] Failed to create node for '%s:%s'\n", hostname, port);
				exit(EXIT_FAILURE);
			}
		}
	}
}

void init_server(){
	int rv, optval;
	void *ptr;

	printf("Initializing DuckChat Server...\n");
	/*Zero-out all global structs*/
	memset(&client_manager, 0, sizeof(struct _client_manager));
	memset(&channel_manager, 0, sizeof(struct _channel_manager));
	memset(&main_queue, 0, sizeof(struct _req_queue));
	memset(&hints, 0, sizeof(hints));
	/*Init server's addr info for either IPv6 or IPv4, UDP socket, and default system's IP addr*/
	hints.ai_family = AF_INET;//AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;

	//printf("Server's hostname:port => %s:%s\n", server_info.hostname, server_info.portno_str);
	if((rv = getaddrinfo(server_info.hostname, server_info.portno_str, &hints, &servinfo)) != 0){
		fprintf(stderr, "in getaddrinfo: %s\n", gai_strerror(rv));
		exit(1);
	}

	optval = 1;
	for(p = servinfo; p != NULL; p = p->ai_next){
		if((server_info.sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
			perror("[?] Error in socket");
			continue;
		}
		setsockopt(server_info.sockfd, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
		if(bind(server_info.sockfd, p->ai_addr, p->ai_addrlen) == -1){
			close(server_info.sockfd);
			perror("[?] Error in bind");
			continue;
		}
		break;
	}
	if(p == NULL){
		fprintf(stderr, "[!] Failed to bind socket\n");
		exit(1);
	}
	server_info.serveraddr = (struct sockaddr_in*)p->ai_addr;

	return;
}

void request_dequeue(struct _req_queue *thread_queue){
	int n;

	pthread_mutex_lock(&lock1);
	if(main_queue.size > 0){
		n = main_queue.size;
		memcpy(thread_queue->queue, main_queue.queue, (sizeof(struct _queue_entry *)*n));
		memset(main_queue.queue, 0, (sizeof(struct _queue_entry *)*n));
		thread_queue->size = n;
		main_queue.size = 0;
	}
	pthread_mutex_unlock(&lock1);
	return;
}

int send_error(char *errmsg, struct sockaddr_in *clientaddr, int sockfd){
	struct _rsp_err rsp_err;
	int n;

	memset(&rsp_err, 0, sizeof(struct _rsp_err));
	rsp_err.type_id = RSP_ERR;
	strncpy(rsp_err.message, errmsg, TEXT_LEN-1);
	n = sendto(sockfd, &rsp_err, sizeof(struct _rsp_err), 0, (struct sockaddr*)clientaddr, sizeof(struct sockaddr));
	if(n < 0)
		perror("Error in sendto");

	return 0;
}

void send_data(struct _queue_entry *entry, int sockfd){
	char ipaddr[128];
	char portno[32];
	int n, i;
	struct channel_entry *ch;
	struct client_entry *client;
	struct _rsp_say rsp_say;
        struct _rsp_list rsp_list;
        struct _rsp_who rsp_who;
        struct _rsp_err rsp_err;

	if(&entry->clientaddr){
		getnameinfo((struct sockaddr*)&entry->clientaddr, sizeof(struct sockaddr), ipaddr, 128, portno, 32, NI_NUMERICHOST | NI_NUMERICSERV);
	}else{
		fprintf(stderr, "[!] Error: sender thread received null entry clientaddr.\n");
		free(entry);
		return;
	}

	if(entry->req_say){
		ch = channel_search(entry->req_say->channel, &channel_manager);
		if(!ch){
			memset(ERR_MSG, 0, TEXT_LEN);
			snprintf(ERR_MSG, TEXT_LEN-1, "channel does not exist.");
			send_error(ERR_MSG, &entry->clientaddr, sockfd);
			free(entry->req_say);
			free(entry);
			return;
		}
		memset(&rsp_say, 0, sizeof(struct _rsp_say));
		rsp_say.type_id = RSP_SAY;
		memcpy(rsp_say.channel, entry->req_say->channel, NAME_LEN);
		memcpy(rsp_say.username, entry->username, NAME_LEN);
		memcpy(rsp_say.text, entry->req_say->text, TEXT_LEN);
		for(i=0; i < ch->num_clients; i++){	
			client = ch->client_list[i];	
			//printf("[<] Sending Client (%s)\tData (%s)\ton Channel(%s)\tfrom User(%s)\n",
				//client->username, rsp_say.text, rsp_say.channel, rsp_say.username);
			n = sendto(sockfd, &rsp_say, sizeof(struct _rsp_say), 0, (struct sockaddr *)&client->clientaddr, sizeof(struct sockaddr));
			if(n < 0)
				perror("Error in sendto");
		}
		free(entry->req_say);

	}else if(entry->req_list){
		memset(&rsp_list, 0, sizeof(struct _rsp_list));
		rsp_list.type_id = RSP_LIST;
		if(channel_list(&rsp_list, &channel_manager) > -1){
			i = (sizeof(struct _rsp_list) - ((LIST_LEN*NAME_LEN)-(rsp_list.num_channels*NAME_LEN)));	
			//printf("[+] List request got (%hu) number of channels and response is of size (%d).\n", rsp_list.num_channels, i);
			n = sendto(sockfd, &rsp_list, i, 0, (struct sockaddr *)&entry->clientaddr, sizeof(struct sockaddr));
			if(n < 0)
				perror("Error in sendto");
		}
		free(entry->req_list);
	}else if(entry->req_who){
		ch = channel_search(entry->req_who->channel, &channel_manager);
		if(!ch){
			memset(ERR_MSG, 0, TEXT_LEN);
			snprintf(ERR_MSG, TEXT_LEN-1, "Channel (%s) does not exist.\n", entry->req_who->channel);
			send_error(ERR_MSG, &entry->clientaddr, sockfd);
			free(entry->req_who);
			free(entry);
			return;
		}
		memset(&rsp_who, 0, sizeof(struct _rsp_who));
		rsp_who.type_id = RSP_WHO;
		memcpy(rsp_who.channel, entry->req_who->channel, NAME_LEN);
		if(channel_who(&rsp_who, ch) > -1){
			i = (sizeof(struct _rsp_who) - ((WHO_LEN*NAME_LEN)-(rsp_who.num_users*NAME_LEN)));	
			//printf("[+] Who request has (%hu) number of channels and response is of size (%d).\n", rsp_who.num_users, i);
			n = sendto(sockfd, &rsp_who, i, 0, (struct sockaddr *)&entry->clientaddr, sizeof(struct sockaddr));
			if(n < 0)
				perror("Error in sendto");
		}
		free(entry->req_who);
	}else{
		printf("[!] Error: send_data received empty request type.\n");
	}

	free(entry);
}

void *thread_responder(void *vargp){
	int t_sockfd, i;
	time_t current_time;
	time_t timestamp;
	struct _req_queue thread_queue;	

	t_sockfd = server_info.sockfd;
	timestamp = time(NULL);
	while(1){
		memset(&thread_queue, 0, sizeof(struct _req_queue));
		request_dequeue(&thread_queue);
		if(thread_queue.size > 0){
			for(i=0; i < thread_queue.size; i++){
				send_data(thread_queue.queue[i], t_sockfd);
			}
		}
	}
}

void *thread_softstate(void *vargp){

	while(1){
		sleep(SS_TIMEOUT);
		client_softstate(&client_manager, &channel_manager);
	}
}

rid_t handle_request(char *data){
	struct client_entry *client;
	struct _adjacent_server *node;
	struct channel_entry *channel;
	struct _queue_entry *entry;
	unique_t id;
	rid_t type;
	int n;

	//Check if request came from adjacent server
	memcpy(&type, data, sizeof(rid_t));
	node = node_search(&client_info.clientaddr, &server_manager);
	if(node){	
		switch(type){
			case S2S_JOIN:
				if(!(s2s_join = malloc(sizeof(struct _S2S_join)))){
					perror("Error in malloc");
					exit(EXIT_FAILURE);
				}
				memset(s2s_join, 0, sizeof(struct _S2S_join));
				s2s_join->type_id = S2S_JOIN;
				memcpy(s2s_join->channel, &data[sizeof(rid_t)], NAME_LEN-1);
				snprintf(LOG_RECV, LOGMSG_LEN, "%s:%s\trecv S2S Join %s", node->ipaddr, node->port_str, s2s_join->channel);
				log_recv();
				channel = channel_search(s2s_join->channel, &channel_manager);
				if(!channel){
					channel = channel_create(s2s_join->channel, &channel_manager, &server_manager);
					if(!channel){
						free(s2s_join);
						return REQ_INVALID;
					}
					//propogate_join(channel, s2s_join, server_info.sockfd);
				}
				free(s2s_join);
				return S2S_JOIN;
			case S2S_LEAVE:
				if(!(s2s_leave = malloc(sizeof(struct _S2S_leave)))){
					perror("Error in malloc");
					exit(EXIT_FAILURE);
				}
				memset(s2s_leave, 0, sizeof(struct _S2S_leave));
				s2s_leave->type_id = S2S_LEAVE;
				memcpy(s2s_leave->channel, &data[sizeof(rid_t)], NAME_LEN-1);
				snprintf(LOG_RECV, LOGMSG_LEN, "%s:%s\trecv S2S Leave %s", node->ipaddr, node->port_str, s2s_leave->channel);
				log_recv();
				channel = channel_search(s2s_leave->channel, &channel_manager);
				if(channel){
					if(rtable_prune(channel, node, &server_manager) < 0){
						printf("%s:%s\terror: Failed to prune %s's routing table\n", server_info.ipaddr_str, server_info.portno_str, channel->channel_name);
						return REQ_INVALID;
					}
				}
				free(s2s_leave);
				return S2S_LEAVE;
			case S2S_SAY:
				memcpy(&id, &data[sizeof(rid_t)], sizeof(unique_t));
				//printf("received id: %llu\n", id);
				if(save_id(id, &server_manager) > 0){
					s2s_say = malloc(sizeof(struct _S2S_say));
					memset(s2s_say, 0, sizeof(struct _S2S_say));
					memcpy(s2s_say->channel, &data[(sizeof(rid_t)+sizeof(unique_t)+NAME_LEN)], NAME_LEN);
					send_leave(s2s_say->channel, node, server_info.sockfd);
					return S2S_SAY;
				}
				if(!(s2s_say = malloc(sizeof(struct _S2S_say)))){
					perror("Error in malloc");
					exit(EXIT_FAILURE);
				}
				memset(s2s_say, 0, sizeof(struct _S2S_say));
				s2s_say->type_id = S2S_SAY;
				s2s_say->msg_id = id;
				memcpy(s2s_say->username, &data[(sizeof(rid_t)+sizeof(unique_t))], (NAME_LEN+NAME_LEN)+TEXT_LEN);	
				channel = channel_search(s2s_say->channel, &channel_manager);	
				if(channel){
					//propogate_say(channel, s2s_say, server_info.sockfd, NULL);
					req_say = malloc(sizeof(struct _req_say));
					req_say->type_id = REQ_SAY;
					memcpy(req_say->channel, s2s_say->channel, (NAME_LEN+TEXT_LEN));
					pthread_mutex_lock(&lock1);
					if(main_queue.size < MAXQSIZE){
						//FIX: add check
						entry = malloc(sizeof(struct _queue_entry));
						memset(entry, 0, sizeof(struct _queue_entry));
						entry->req_say = req_say;
						memcpy(entry->username, client->username, NAME_LEN);
						memcpy(&entry->clientaddr, node->serveraddr, sizeof(struct sockaddr_in));
						main_queue.queue[main_queue.size] = entry;
						main_queue.size++;
					}
					pthread_mutex_unlock(&lock1);
				}
				free(s2s_say);
				return S2S_SAY;
			default:
				snprintf(LOG_RECV, LOGMSG_LEN, "%s:%s\trecv S2S Invalid", node->ipaddr, node->port_str);
        			log_recv();
				return REQ_INVALID;
		}
	}

	client = client_search(&client_info.clientaddr, &client_manager);
	if(client){
		//update client timestamp
		client_keepalive(client);
	}
	if(client == NULL && type != REQ_LOGIN){
		send_error("You are not logged in yet.", &client_info.clientaddr, server_info.sockfd);
		return REQ_INVALID;
	}else if(client != NULL && type == REQ_LOGIN){
		send_error("You are already logged in.", &client_info.clientaddr, server_info.sockfd);
		return REQ_INVALID;
	}

	switch(type){
		case REQ_LOGIN:	
			if(!(req_login = malloc(sizeof(struct _req_login)))){
				perror("Error in malloc");
				exit(1);
			}
			memset(req_login, 0, sizeof(struct _req_login));
			req_login->type_id = REQ_LOGIN;
			//FIX: remove the -1 from NAME_LEN once client login is implemented
			memcpy(req_login->username, &data[sizeof(rid_t)], NAME_LEN-1);
			snprintf(LOG_RECV, LOGMSG_LEN, "%s:%s\trecv Request Login %s", client_info.ipaddr_str, client_info.portno_str, req_login->username);
			log_recv();
			//printf("[>] Login request from: (%s)\n", req_login->username);
			client_add(req_login->username, &client_info.clientaddr, &client_manager);

			free(req_login);
			return REQ_LOGIN;
		case REQ_LOGOUT:
			snprintf(LOG_RECV, LOGMSG_LEN, "%s:%s\trecv Request Logout %s", client_info.ipaddr_str, client_info.portno_str, client->username);
			log_recv();
			client_logout(client, &client_manager, &channel_manager);
			return REQ_LOGOUT;
		case REQ_JOIN:
			if(!(req_join = malloc(sizeof(struct _req_join)))){
				perror("Error in malloc");
				exit(EXIT_FAILURE);
			}
			memset(req_join, 0, sizeof(struct _req_join));
			req_join->type_id = REQ_JOIN;
			//FIX: remove -1 on NAME_LEN
			memcpy(req_join->channel, &data[sizeof(rid_t)], NAME_LEN-1);
			snprintf(LOG_RECV, LOGMSG_LEN, "%s:%s\trecv Request Join %s %s", client_info.ipaddr_str, client_info.portno_str, client->username, req_join->channel);
			log_recv();
			//printf("[>] Join request from (%s) for channel (%s)\n", client->username, req_join->channel);
			
			channel = channel_search(req_join->channel, &channel_manager);
			if(!channel){
				channel = channel_create(req_join->channel, &channel_manager, &server_manager);
				if(!channel){
					send_error("join request failed.", &client_info.clientaddr, server_info.sockfd);
					free(req_join);
					break;
				}
				if(!(s2s_join = malloc(sizeof(struct _S2S_join)))){
					perror("Error in malloc");
					exit(EXIT_FAILURE);
				}
				s2s_join->type_id = S2S_JOIN;
				memcpy(s2s_join->channel, channel->channel_name, NAME_LEN);
				//propogate_join(channel, s2s_join, server_info.sockfd);
			}
			n = client_add_channel(channel, client);
			//Only send error message if client_add_channel return with an error e.g. -1
			if(n < 0)
				send_error("join request failed.", &client_info.clientaddr, server_info.sockfd);	
			if(n != 0){
				free(req_join);
				break;
			}
			if(channel_add_client(client, channel) != 0){
				send_error("join request failed.", &client_info.clientaddr, server_info.sockfd);
				free(req_join);
				break;
			}

			free(s2s_join);
			free(req_join);
			return REQ_JOIN;
		case REQ_LEAVE:
			if(!(req_leave = malloc(sizeof(struct _req_leave)))){
				perror("Error in malloc");
				exit(EXIT_FAILURE);
			}
			memset(req_leave, 0, sizeof(struct _req_leave));
			req_leave->type_id = REQ_LEAVE;
			memcpy(req_leave->channel, &data[sizeof(rid_t)], NAME_LEN-1);
			snprintf(LOG_RECV, LOGMSG_LEN, "%s:%s\trecv Request Leave %s %s", client_info.ipaddr_str, client_info.portno_str, client->username, req_leave->channel);
			log_recv();
			//printf("[>] Leave request from (%s) for channel (%s)\n", client->username, req_leave->channel);

			channel = channel_search(req_leave->channel, &channel_manager);
			if(!channel){
				send_error("channel not found for leave request.", &client_info.clientaddr, server_info.sockfd);
				free(req_leave);
				return REQ_INVALID;
			}
			client_remove_channel(channel, client);
			channel_remove_client(client, channel);
			//Check if channel is empty
			if(channel->num_clients < 1){
				if(channel_remove(channel, &channel_manager) != 0){
					printf("[?] Error: failed to remove empty channel (%s).\n", channel->channel_name);
				}
			}

			free(req_leave);
			return REQ_LEAVE;
		case REQ_SAY:
			if(!(req_say = malloc(sizeof(struct _req_say)))){
				perror("Error in malloc");
				exit(1);
			}
			memset(req_say, 0, sizeof(struct _req_say));
			req_say->type_id = REQ_SAY;
			//FIX: remove -1 on NAME_LEN
			memcpy(req_say->channel, &data[sizeof(rid_t)], NAME_LEN-1);
			memcpy(req_say->text, &data[(sizeof(rid_t)+NAME_LEN)], TEXT_LEN-1);
			snprintf(LOG_RECV, LOGMSG_LEN, "%s:%s\trecv Request Say %s %s", client_info.ipaddr_str, client_info.portno_str, req_say->channel, req_say->text);
			log_recv();
			//printf("[>] Say request from (%s) for channel: (%s)\n\tMessage: %s\n", client->username, req_say->channel, req_say->text);
			//Enqueue request for sender thread
			channel = channel_search(req_say->channel, &channel_manager);
			if(channel){
				//s2s_say = create_S2S_say(client->username, req_say->channel, req_say->text, &server_manager);
				//if(s2s_say){
				//	propogate_say(NULL, s2s_say, server_info.sockfd, &server_manager);
				//}
				//free(s2s_say);
			}
			pthread_mutex_lock(&lock1);
			if(main_queue.size < MAXQSIZE){
				//FIX: add check
				entry = malloc(sizeof(struct _queue_entry));
				memset(entry, 0, sizeof(struct _queue_entry));
				entry->req_say = req_say;
				memcpy(entry->username, client->username, NAME_LEN);
				memcpy(&entry->clientaddr, &client->clientaddr, sizeof(struct sockaddr_in));
				main_queue.queue[main_queue.size] = entry;
				main_queue.size++;
			}
			pthread_mutex_unlock(&lock1);

			return REQ_SAY;
		case REQ_LIST:
			if(!(req_list = malloc(sizeof(struct _req_list)))){
				perror("Error in malloc");
				exit(EXIT_FAILURE);
			}
			memset(req_list, 0, sizeof(struct _req_list));
			req_list->type_id = REQ_LIST;
			snprintf(LOG_RECV, LOGMSG_LEN, "%s:%s\trecv Request List %s", client_info.ipaddr_str, client_info.portno_str, client->username);
			log_recv();
			//Enqueue request for sender thread
			pthread_mutex_lock(&lock1);
			if(main_queue.size < MAXQSIZE){
				entry = malloc(sizeof(struct _queue_entry));
				memset(entry, 0, sizeof(struct _queue_entry));
				entry->req_list = req_list;
				memcpy(&entry->clientaddr, &client->clientaddr, sizeof(struct sockaddr_in));
				main_queue.queue[main_queue.size] = entry;
				main_queue.size++;
			}
			pthread_mutex_unlock(&lock1);
			return REQ_LIST;
		case REQ_WHO:
			if(!(req_who = malloc(sizeof(struct _req_who)))){
				perror("Error in malloc");
				exit(EXIT_FAILURE);
			}
			memset(req_who, 0, sizeof(struct _req_who));
			req_who->type_id = REQ_WHO;
			memcpy(req_who->channel, &data[sizeof(rid_t)], NAME_LEN-1);
			snprintf(LOG_RECV, LOGMSG_LEN, "%s:%s\trecv Request Who %s %s", client_info.ipaddr_str, client_info.portno_str, client->username, req_who->channel);
			log_recv();
			//Enqueue request for sender thread
			pthread_mutex_lock(&lock1);
			if(main_queue.size < MAXQSIZE){
				entry = malloc(sizeof(struct _queue_entry));
				memset(entry, 0, sizeof(struct _queue_entry));
				entry->req_who = req_who;
				memcpy(&entry->clientaddr, &client->clientaddr, sizeof(struct sockaddr_in));
				main_queue.queue[main_queue.size] = entry;
				main_queue.size++;
			}
			pthread_mutex_unlock(&lock1);
			return REQ_WHO;
		case REQ_ALIVE:
			//client timestamp is updated at begining of function
			//client_keepalive(client);
			snprintf(LOG_RECV, LOGMSG_LEN, "%s:%s\trecv Request KeepAlive %s", client_info.ipaddr_str, client_info.portno_str, client->username);
			log_recv();
			return REQ_ALIVE;
		default:
			break;		
	}
	snprintf(LOG_RECV, LOGMSG_LEN, "Invalid request");
	log_recv();
	return REQ_INVALID;
}

void recvdata_IPv6(){
	char input[BUFSIZE+STR_PADD];
	int n;
	socklen_t clientlen;

	clientlen = sizeof(client_info.clientaddr6);
	inet_ntop(AF_INET6, &(server_info.serveraddr6->sin6_addr), server_info.ipaddr_str, INET6_ADDRSTRLEN);
	printf("[+] Server Info:\n\tIPv6 Addr: %s\n\tPort: %d\n", server_info.ipaddr_str, ntohs(server_info.serveraddr6->sin6_port));
	printf("Server is now waiting for data...\n");
	while(1){
		memset(input, 0, BUFSIZE+STR_PADD);
		memset(&client_info, 0, sizeof(struct _client_info));

		n = recvfrom(server_info.sockfd, input, BUFSIZE, 0, (struct sockaddr*)&client_info.clientaddr6, &clientlen);
		if(n < 0){
			perror("[?] Issue in recvfrom");
			continue;
		}
		getnameinfo((struct sockaddr*)&client_info.clientaddr6, sizeof(struct sockaddr), 
			client_info.ipaddr_str, 256, client_info.portno_str, 32, NI_NUMERICHOST | NI_NUMERICSERV);
		//printf("Server received data from:\n\tHost: %s\n\tService: %s\n\n", client_info.ipaddr_str, client_info.portno_str);
		handle_request(input);	
	}

	return;
}

void recvdata_IPv4(){
	char input[BUFSIZE+STR_PADD];
	int n;
	rid_t type;
	socklen_t clientlen;

	clientlen = sizeof(client_info.clientaddr);
	inet_ntop(AF_INET, &(server_info.serveraddr->sin_addr), server_info.ipaddr_str, INET_ADDRSTRLEN);
	printf("Server Setup Complete.\n");
	printf("Accepting data on bound socket \t%s:%d\n", server_info.ipaddr_str, ntohs(server_info.serveraddr->sin_port));
	while(1){
		memset(input, 0, BUFSIZE+STR_PADD);
		memset(&client_info, 0, sizeof(struct _client_info));

		n = recvfrom(server_info.sockfd, input, BUFSIZE, 0, (struct sockaddr*)&client_info.clientaddr, &clientlen);
		if(n < 0){
			perror("[?] Issue in revcfrom");
			continue;
		}
		getnameinfo((struct sockaddr*)&client_info.clientaddr, sizeof(struct sockaddr), 
			client_info.ipaddr_str, 256, client_info.portno_str, 32, NI_NUMERICHOST | NI_NUMERICSERV);
		//printf("Server received data from:\n\tHost: %s\n\tService: %s\n\n", client_info.ipaddr_str, client_info.portno_str);
		handle_request2(input);
	}

	return;
}

int main(int argc, char *argv[]){
	int optval, rv;
	int n;
	void *ptr;

	if((argc%2) != 1){
		printf("Usage: \n");
		exit(0);
	}

	n = strlen(argv[2]);
	if(n < 1 || n > 5){
		fprintf(stderr, "Error: received invalid 2nd argument (port number).\n");
		exit(0);
	}
	if(signal(SIGINT, sig_handler) == SIG_ERR){
		perror("Error in signal");
		exit(EXIT_FAILURE);
	}
	memset(&server_info, 0, sizeof(struct _server_info));
	server_info.portno = atoi(argv[2]);
	memcpy(server_info.portno_str, argv[2], n);
	server_info.hostname = argv[1];
	LOG_RECV = malloc(LOGMSG_LEN);
	LOG_SEND = malloc(LOGMSG_LEN);
	if(!LOG_RECV || !LOG_SEND){
		perror("Error in malloc");
		exit(EXIT_FAILURE);
	}
	if(pthread_mutex_init(&lock1, NULL) != 0){
		perror("Error in pthread_mutex_init");
		exit(EXIT_FAILURE);
	}
	init_server();
	init_servertree(argc, argv);
	
	test_chm();
	switch (p->ai_family){
		case AF_INET:
			//Create response thread
			pthread_create(&tid[0], NULL, thread_responder, NULL);
			pthread_create(&tid[1], NULL, thread_softstate, NULL);
			recvdata_IPv4();
			break;
		case AF_INET6:
			puts("ai_family IPv6");	
			recvdata_IPv6();
			break;
		default:
			fprintf(stderr, "Error: ai_family invalid.\n");
			exit(1);
	}

	return 0;
}








