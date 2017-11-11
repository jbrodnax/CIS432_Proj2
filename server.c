#include "server.h"

struct _server_info{
	char *hostname;
	//char ipaddr_str[128];
	int sockfd;
	int portno;
	char portno_str[32];
	struct sockaddr_in *serveraddr;
	struct sockaddr_in6 *serveraddr6;
};

struct _client_info{
	struct sockaddr_in clientaddr;
	struct sockaddr_in6 clientaddr6;
	struct hostent *hostp;
	char *hostaddrp;
	int portno;
};

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
			printf("Using port: %d\n", ntohs(server_info.serveraddr->sin_port));
			recvdata_IPv4();
			break;
		case AF_INET6:
			puts("ai_family IPv6");
			server_info.serveraddr6 = (struct sockaddr_in6*)p->ai_addr;
			printf("Using port: %d\n", ntohs(server_info.serveraddr6->sin6_port));
			recvdata_IPv6();
			break;
		default:
			fprintf(stderr, "Error: ai_family invalid.\n");
			exit(1);
	}
	freeaddrinfo(servinfo);

	return;
}

int _resolve_clientaddr(){
	char host[1024];
	char service[32];

	memset(host, 0, 1024);
	memset(service, 0, 32);

	getnameinfo((struct sockaddr*)&client_info.clientaddr, sizeof(struct sockaddr), host, 1024, service, 32, NI_NUMERICHOST | NI_NUMERICSERV);
	printf("Resolved client info:\n\tHost: %s\n\tService: %s\n\n", host, service);
	return 1;
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

void recvdata_IPv6(){
	char input[BUFSIZE];
	int n;
	socklen_t clientlen;

	clientlen = sizeof(client_info.clientaddr6);
	printf("Server is now waiting for data...\n");
	while(1){
		memset(input, 0, BUFSIZE);
		memset(&client_info, 0, sizeof(struct _client_info));

		n = recvfrom(server_info.sockfd, input, BUFSIZE, 0, (struct sockaddr*)&client_info.clientaddr6, &clientlen);
		if(n < 0){
			perror("[?] Issue in recvfrom");
			continue;
		}
		//FIX: finish
	}
}

void recvdata_IPv4(){
	char input[BUFSIZE];
	int n;
	socklen_t clientlen;

	clientlen = sizeof(client_info.clientaddr);
	printf("Server is now waiting for data...\n");
	while(1){
		memset(input, 0, BUFSIZE);
		memset(&client_info, 0, sizeof(struct _client_info));

		n = recvfrom(server_info.sockfd, input, BUFSIZE, 0, (struct sockaddr*)&client_info.clientaddr, &clientlen);
		if(n < 0){
			perror("[?] Issue in revcfrom");
			continue;
		}
		if(_resolve_clientaddr() == 0){
			printf("Server received data from:\t%s (%s : %d)\n", client_info.hostp->h_name, client_info.hostaddrp, client_info.portno);
			printf("Data (%d):\t%s\n", n, input);
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
	/*
	if((rv = getaddrinfo(server_info.hostname, argv[2], &hints, &servinfo)) != 0){
		fprintf(stderr, "in getaddrinfo: %s\n", gai_strerror(rv));
		exit(1);
	}
	printf("Host: %s\n", server_info.hostname);
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
	//inet_ntop(AF_INET, &server_info.serveraddr.sin_addr, server_info.ipaddr_str, INET_ADDRSTRLEN);
	inet_ntop(p->ai_family, p->ai_addr->sa_data, server_info.ipaddr_str, 128);
	switch (p->ai_family){
		case AF_INET:
			puts("AI Family is IPv4");
			break;
		case AF_INET6:
			puts("AI Family is IPv6");
			break;
		default:
			puts("AI Family is invalid");
	}
	ptr = &((struct sockaddr_in6 *)p->ai_addr)->sin6_addr;
	inet_ntop(p->ai_family, ptr, server_info.ipaddr_str, 100);
	recvdata();
	
	server_info.sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if(server_info.sockfd < 0)
		error("[!] Error opening socket");

	optval = 1;
	setsockopt(server_info.sockfd, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
	server_info.serveraddr.sin_family = AF_INET;
	server_info.serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_info.serveraddr.sin_port = htons((unsigned short)server_info.portno);
	if(bind(server_info.sockfd, (struct sockaddr *)&server_info.serveraddr, sizeof(struct sockaddr_in)) < 0)
		error("[!] Error on binding");
	//inet_ntop(AF_INET, &server_info.serveraddr.sin_addr, server_info.ipaddr_str, INET_ADDRSTRLEN);
	recvdata();
	*/

	return 0;
}








