#include "duckchat.h"

struct _server_info{
	int portno;
	struct sockaddr_in serveraddr;
	struct hostent *server;
	char *hostname;
};

struct req_login login_pkt;

struct _server_info server_info;
int sockfd;

void send_data();

void send_login_test(){
	char test_name[]="John";
	char data[sizeof(struct req_login)];
	int n, serverlen;

	serverlen = sizeof(struct sockaddr);
	memset(&login_pkt, 0, sizeof(struct req_login));
	login_pkt.type_id = REQ_LOGIN;
	strncpy(login_pkt.username, test_name, NAME_LEN);
	memcpy(data, &login_pkt, sizeof(struct req_login));
	n = sendto(sockfd, data, sizeof(struct req_login), 0, (struct sockaddr *)&server_info.serveraddr, serverlen);
        if(n < 0){
        	perror("[!] Error in sendto");
        	exit(1);
	}
	puts("Login sent");
	send_data();
}

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
		printf("Sending: %s\n", input);
		n = sendto(sockfd, input, BUFSIZE, 0, (struct sockaddr *)&server_info.serveraddr, serverlen);
		if(n < 0){
			perror("[!] Error in sendto");
			exit(1);
		}
		printf("Message sent (size: %d)\n", n);
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
	send_login_test();
	return 0;
}
