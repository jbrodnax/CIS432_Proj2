#include "duckchat.h"

struct _server_info{
	int portno;
	struct sockaddr_in serveraddr;
	struct hostent *server;
	char *hostname;
};

struct _server_info server_info;
int sockfd;

void send_data(){
	char input[BUFSIZE];
	int serverlen;
	int n;

	serverlen = sizeof(struct sockaddr);
	while(1){
		memset(input, 0, BUFSIZE);
		write(1, "> ", 2);
		fgets(input, BUFSIZE, stdin);
		if(!strncmp(input, "/exit", 5))
			exit(0);
		n = sendto(sockfd, input, BUFSIZE, 0, (struct sockaddr *)&server_info.serveraddr, serverlen);
		if(n < 0){
			perror("[!] Error in sendto");
			exit(1);
		}
		puts("Message sent.");
	}

	return;
}

int main(int argc, char *argv[]){

	if(argc != 3) {
		fprintf(stderr,"usage: %s <hostname> <port>\n", argv[0]);
		exit(0);
	}

	memset(&server_info, 0, sizeof(struct _server_info));
	server_info.hostname = argv[1];
	server_info.portno = atoi(argv[2]);

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if(sockfd < 0){
		perror("[!] Error opening socket");
		exit(1);
	}

	server_info.server = gethostbyname(server_info.hostname);
	if(server_info.server == NULL){
		fprintf(stderr, "[!] Error no such host: %s\n", server_info.hostname);
		exit(1);
	}

	server_info.serveraddr.sin_family = AF_INET;
	bcopy((char *)server_info.server->h_addr, (char *)&server_info.serveraddr.sin_addr.s_addr, server_info.server->h_length);
	server_info.serveraddr.sin_port = htons(server_info.portno);
	send_data();
	return 0;
}
