#include "server.h"

pthread_t tid[3];
char ERR_MSG[TEXT_LEN];

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

void init_rwlocks(){
	if(pthread_rwlock_init(&client_lock, NULL) != 0){
		error_msg("failed to init client mutex lock.");
		exit(EXIT_FAILURE);
	}
	if(pthread_rwlock_init(&channel_lock, NULL) != 0){
		error_msg("failed to init channel mutex lock.");
		exit(EXIT_FAILURE);
	}
	if(pthread_rwlock_init(&node_lock, NULL) != 0){
		error_msg("failed to init node mutex lock.");
		exit(EXIT_FAILURE);
	}
}

void log_thread(char *message){
	printf("%s:%s\t%s\n", server_info.ipaddr_str, server_info.portno_str, message);
	return;
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

	/*Zero-out all global structs*/
	memset(&client_manager, 0, sizeof(struct _client_manager));
	memset(&channel_manager, 0, sizeof(struct _channel_manager));
	memset(&main_queue, 0, sizeof(struct _req_queue));
	memset(&hints, 0, sizeof(hints));

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;

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

void *thread_responder(){
	int t_sockfd, i;
	struct _req_queue thread_queue;	

	t_sockfd = server_info.sockfd;
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

void *thread_resubscribe(){
	while(1){
		sleep(SR_TIMEOUT);
		resubscribe(&channel_manager, server_info.sockfd);
	}
}

void *thread_softstate(){
	while(1){
		sleep(SS_TIMEOUT);
		client_softstate(&client_manager, &channel_manager);
		channel_softstate(&channel_manager);
	}
}

void recvdata_IPv4(){
	char input[BUFSIZE+STR_PADD];
	int n;
	socklen_t clientlen;

	clientlen = sizeof(client_info.clientaddr);
	inet_ntop(AF_INET, &(server_info.serveraddr->sin_addr), server_info.ipaddr_str, INET_ADDRSTRLEN);
	printf("%s:%d\tServer Setup Complete\n", server_info.ipaddr_str, ntohs(server_info.serveraddr->sin_port));
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
		handle_request(input);
	}

	return;
}

int main(int argc, char *argv[]){
	int n;	

	if((argc%2) != 1){
		printf("Usage: <hostname/ip> <port #> (followed by N adjacent server <hostname/ip> <port #> pairs)\n");
		exit(0);
	}

	n = strlen(argv[2]);
	if(n < 1 || n > 5){
		fprintf(stderr, "Error: received invalid port number.\n");
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
	
	switch (p->ai_family){
		case AF_INET:	
			pthread_create(&tid[0], NULL, thread_responder, NULL);
			pthread_create(&tid[1], NULL, thread_softstate, NULL);
			pthread_create(&tid[2], NULL, thread_resubscribe, NULL);
			recvdata_IPv4();
			break;
		default:
			fprintf(stderr, "Error: ai_family invalid.\n");
			exit(EXIT_FAILURE);
	}

	return 0;
}








