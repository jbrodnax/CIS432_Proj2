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

struct _req_login* req_login;
struct _req_logout* req_logout;
struct _req_join* req_join;
struct _req_leave* req_leave;
struct _req_say* req_say;
struct _req_list* req_list;
struct _req_who* req_who;
struct _req_alive* req_alive;

struct _client_manager client_manager;
struct _server_info server_info;
struct _client_info client_info;
struct addrinfo hints, *servinfo, *p;

void error(char *msg){
	perror(msg);
	exit(1);
}

void init_server(){
	int rv, optval;
	void *ptr;

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
	switch (p->ai_family){
		case AF_INET:
			puts("ai_family IPv4");
			server_info.serveraddr = (struct sockaddr_in*)p->ai_addr;	
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

int resolve_clientaddr(){

	client_info.hostp = gethostbyaddr((const char*)&client_info.clientaddr.sin_addr.s_addr, sizeof(client_info.clientaddr.sin_addr.s_addr), AF_INET);
	if(client_info.hostp == NULL){
		perror("[?] Issue on gethostbyaddr");
		return -1;
	}
	client_info.hostaddrp = inet_ntoa(client_info.clientaddr.sin_addr);
	if(client_info.hostaddrp == NULL){
		perror("[?] Issue in inet_ntoa");
		return -1;
	}
	client_info.portno = ntohs(client_info.clientaddr.sin_port);

	return 0;
}

rid_t handle_request(char *data){
	rid_t type;

	memcpy(&type, data, sizeof(rid_t));
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
			free(req_login);
			return REQ_LOGIN;
		case REQ_LOGOUT:
			break;
		case REQ_JOIN:
			break;
		case REQ_LEAVE:
			break;
		case REQ_SAY:
			if(!(req_say = malloc(sizeof(struct _req_say)))){
				perror("Error in malloc");
				exit(1);
			}
			memset(req_say, 0, sizeof(struct _req_say));
			req_say->type_id = REQ_SAY;
			memcpy(req_say->channel, &data[sizeof(rid_t)], NAME_LEN-1);
			memcpy(req_say->text, &data[(sizeof(rid_t)+NAME_LEN)], TEXT_LEN-1);
			printf("[>] Say request for channel: (%s)\n\tMessage: %s\n", req_say->channel, req_say->text);
			free(req_say);
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
		//printf("Data (%d):\t%s\n", n, input);
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
		//printf("Data (%d):\t%s\n", n, input);
		memcpy(&type, input, sizeof(rid_t));
		if(type == REQ_LOGIN){
			printf("Login request from: %s\n", &input[4]);
		}
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








