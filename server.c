#include "server.h"

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

struct _client_manager client_manager;
struct _channel_manager channel_manager;
struct _server_info server_info;
struct _client_info client_info;
struct addrinfo hints, *servinfo, *p;

pthread_t tid[1];
pthread_mutex_t lock1 = PTHREAD_MUTEX_INITIALIZER;
char init_channelname[]="Common";

void error(char *msg){
	perror(msg);
	exit(1);
}

void test_chm(){
	if(!channel_create(init_channelname, &channel_manager)){
		fprintf(stderr, "[!] Error: Channel creation of (%s) failed.");
		exit(EXIT_FAILURE);
	}
	return;
}

void init_server(){
	int rv, optval;
	void *ptr;

	/*Zero-out all global structs*/
	memset(&client_manager, 0, sizeof(struct _client_manager));
	memset(&channel_manager, 0, sizeof(struct _channel_manager));
	memset(&main_queue, 0, sizeof(struct _req_queue));
	memset(&hints, 0, sizeof(hints));
	/*Init server's addr info for either IPv6 or IPv4, UDP socket, and default system's IP addr*/
	hints.ai_family = AF_INET;//AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;

	printf("[+] Getting address info for:\n\tHostname: %s\n\tPort: %s\n\n", server_info.hostname, server_info.portno_str);
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
	/*Create Common channel*/
	test_chm();
	switch (p->ai_family){
		case AF_INET:
			puts("ai_family IPv4");
			server_info.serveraddr = (struct sockaddr_in*)p->ai_addr;
			/*Create response thread*/
			pthread_create(&tid[0], NULL, thread_responder, NULL);
			recvdata_IPv4();
			break;
		case AF_INET6:
			puts("ai_family IPv6");
			server_info.serveraddr6 = (struct sockaddr_in6*)p->ai_addr;	
			recvdata_IPv6();
			break;
		default:
			fprintf(stderr, "Error: ai_family invalid.\n");
			exit(1);
	}
	freeaddrinfo(servinfo);

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

void send_data(struct _queue_entry *entry, int sockfd){
	int n, i;
	struct channel_entry *ch;
	struct client_entry *client;
	struct _rsp_say rsp_say;
        struct _rsp_list rsp_list;
        struct _rsp_who rsp_who;
        struct _rsp_err rsp_err;

	if(entry->req_say){
		ch = channel_search(entry->req_say->channel, &channel_manager);
		if(!ch){
			printf("[!] Error: channel (%s) does not exist.\n", entry->req_say->channel);
			//send error rsp
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
			printf("[+] Channel (%s) has (%d) clients.\n", ch->channel_name, ch->num_clients);
			client = ch->client_list[i];
			printf("[<] Sending Client (%s)\tData (%s)\ton Channel(%s)\tfrom User(%s)\n",
				client->username, rsp_say.text, rsp_say.channel, rsp_say.username);
			n = sendto(sockfd, &rsp_say, sizeof(struct _rsp_say), 0, (struct sockaddr *)&client->clientaddr, sizeof(struct sockaddr));
			if(n < 0)
				perror("Error in sendto");
		}
		free(entry->req_say);

	}else if(entry->req_list){
		printf("sending list response");
		free(entry->req_list);
	}else if(entry->req_who){
		printf("sending who response");
		free(entry->req_who);
	}else{
		printf("[!] Error: send_data received empty request type.\n");
	}

	free(entry);
}

void *thread_responder(void *vargp){
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

rid_t handle_request(char *data){
	struct client_entry *client;
	struct channel_entry *channel;
	struct _queue_entry *entry;
	rid_t type;

	/*Check if request came from authenticated client*/
	memcpy(&type, data, sizeof(rid_t));
	client = client_search(&client_info.clientaddr, &client_manager);
	if(client == NULL && type != REQ_LOGIN){
		printf("[?] Issue: received non-login request from non-authenticated client\n");
		return REQ_INVALID;
	}else if(client != NULL && type == REQ_LOGIN){
		printf("[?] Issue: received login request from already authenticated client\n");
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
			printf("[>] Login request from: (%s)\n", req_login->username);
			client_add(req_login->username, &client_info.clientaddr, &client_manager);
			free(req_login);
			return REQ_LOGIN;
		case REQ_LOGOUT:
			client_remove(client, &client_manager);
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
			printf("[>] Join request from (%s) for channel (%s)\n", client->username, req_join->channel);
			
			channel = channel_search(req_join->channel, &channel_manager);
			if(!channel){
				channel = channel_create(req_join->channel, &channel_manager);
				if(!channel){
					free(req_join);
					break;
				}
			}
			if(channel_add_client(client, channel) != 0){
				free(req_join);
				break;
			}
			if(client_add_channel(channel, client) != 0){
				free(req_join);
				break;
			}
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
			printf("[>] Leave request from (%s) for channel (%s)\n", client->username, req_leave->channel);

			channel = channel_search(req_leave->channel, &channel_manager);
			if(!channel){
				//FIX: send error msg to client
				printf("[?] Error: channel (%s) not found.\n", req_leave->channel);
				free(req_leave);
				return REQ_INVALID;
			}
			client_remove_channel(channel, client);
			channel_remove_client(client, channel);

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
			printf("[>] Say request from (%s) for channel: (%s)\n\tMessage: %s\n", client->username, req_say->channel, req_say->text);
			/*Enqueue request for sender thread*/
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
			break;
		case REQ_WHO:
			break;
		case REQ_ALIVE:
			break;
		default:
			break;		
	}

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
		printf("Server received data from:\n\tHost: %s\n\tService: %s\n\n", client_info.ipaddr_str, client_info.portno_str);
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
	printf("[+] Server Info:\n\tIPv4 Addr: %s\n\tPort: %d\n", server_info.ipaddr_str, ntohs(server_info.serveraddr->sin_port));
	printf("Server is now waiting for data...\n");
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
		printf("Server received data from:\n\tHost: %s\n\tService: %s\n\n", client_info.ipaddr_str, client_info.portno_str);
		handle_request(input);
	}

	return;
}

int main(int argc, char *argv[]){
	int optval, rv;
	int n;
	void *ptr;

	if(argc != 3){
		printf("Usage: \n");
		exit(0);
	}

	n = strlen(argv[2]);
	if(n < 1 || n > 5){
		fprintf(stderr, "Error: received invalid 2nd argument (port number).\n");
		exit(0);
	}
	memset(&server_info, 0, sizeof(struct _server_info));
	server_info.portno = atoi(argv[2]);
	memcpy(server_info.portno_str, argv[2], n);
	server_info.hostname = argv[1];
	init_server();

	return 0;
}








