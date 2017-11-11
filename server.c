#include "server.h"

struct _server_info{
	int sockfd;
	int portno;
	struct sockaddr_in serveraddr;
	int optval;
};

struct _client_info{
	struct sockaddr_in clientaddr;
	struct hostent *hostp;
	char *hostaddrp;
	int portno;
};

struct _client_manager client_manager;
struct _server_info server_info;
struct _client_info client_info;

void error(char *msg){
	perror(msg);
	exit(1);
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

void recvdata(){
	char input[BUFSIZE];
	int n;
	socklen_t clientlen;

	clientlen = sizeof(client_info.clientaddr);
	while(1){
		memset(input, 0, BUFSIZE);
		memset(&client_info, 0, sizeof(struct _client_info));

		n = recvfrom(server_info.sockfd, input, BUFSIZE, 0, (struct sockaddr*)&client_info.clientaddr, &clientlen);
		if(n < 0){
			perror("[?] Issue in revcfrom");
			continue;
		}
		if(resolve_clientaddr() == 0){
			printf("Server received data from:\t%s (%s : %d)\n", client_info.hostp->h_name, client_info.hostaddrp, client_info.portno);
			printf("Data (%d):\t%s\n", n, input);
		}
	}

	return;
}

int main(int argc, char *argv[]){

	if(argc != 2){
		printf("Usage: \n");
		exit(0);
	}
	memset(&server_info, 0, sizeof(struct _server_info));
	server_info.portno = atoi(argv[1]);
	server_info.sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if(server_info.sockfd < 0)
		error("[!] Error opening socket");

	server_info.optval = 1;
	setsockopt(server_info.sockfd, SOL_SOCKET, SO_REUSEADDR, (const void*)&server_info.optval, sizeof(int));
	server_info.serveraddr.sin_family = AF_INET;
	server_info.serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_info.serveraddr.sin_port = htons((unsigned short)server_info.portno);
	if(bind(server_info.sockfd, (struct sockaddr *)&server_info.serveraddr, sizeof(struct sockaddr_in)) < 0)
		error("[!] Error on binding");
	return 0;
}
