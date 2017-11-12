#include "duckchat.h"

struct _server_info{
	int portno;
	struct sockaddr_in serveraddr;
	struct hostent *server;
	char *hostname;
};

struct _client_info{
	char username[NAME_LEN+STR_PADD];
};

struct _req_login req_login;
struct _req_logout req_logout;
struct _req_join req_join;
struct _req_leave req_leave;
struct _req_say req_say;
struct _req_list req_list;
struct _req_who req_who;
struct _req_alive req_alive;

struct _server_info server_info;
struct _client_info client_info;
char output[BUFSIZE+STR_PADD];
size_t output_size;
int sockfd;
/*Just for testing*/
char channel_name[]="Common";

int send_data();

void send_login_test(){
	char test_name[]="John";
	char data[sizeof(struct _req_login)];
	int n, serverlen;

	serverlen = sizeof(struct sockaddr);
	memset(&req_login, 0, sizeof(struct _req_login));
	req_login.type_id = REQ_LOGIN;
	strncpy(req_login.username, test_name, NAME_LEN);
	memcpy(data, &req_login, sizeof(struct _req_login));
	n = sendto(sockfd, data, sizeof(struct _req_login), 0, (struct sockaddr *)&server_info.serveraddr, serverlen);
        if(n < 0){
        	perror("[!] Error in sendto");
        	exit(1);
	}
	puts("Login sent");
	send_data();
}
/*
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
*/
int send_data(){
	int n, serverlen;

	serverlen = sizeof(struct sockaddr);
	if(output_size < sizeof(rid_t) || output_size > BUFSIZE){
		fprintf(stderr, "Error in send_data: output_size has invalid size of (%u)\n", output_size);
		return -1;
	}

	n = sendto(sockfd, output, output_size, 0, (struct sockaddr *)&server_info.serveraddr, serverlen);
	if(n < 0){
		perror("[!] Error in sendto");
		return -1;
	}
	printf("Message sent (size: %d)\n", n);
	return 0;
}

rid_t build_request(rid_t type, int argc, char **argv){

	memset(output, 0, (BUFSIZE+STR_PADD));
	switch(type){
		case REQ_LOGIN:
			output_size = sizeof(struct _req_login);
			memset(&req_login, 0, output_size);
			req_login.type_id = REQ_LOGIN;
			memcpy(req_login.username, client_info.username, NAME_LEN);
			memcpy(output, &req_login, output_size);
			return REQ_LOGIN;
		case REQ_LOGOUT:
			break;
		case REQ_JOIN:
			break;
		case REQ_LEAVE:
			break;
		case REQ_SAY:
			output_size = sizeof(struct _req_say);
			memset(&req_say, 0, output_size);
			req_say.type_id = REQ_SAY;
			memcpy(req_say.channel, channel_name, NAME_LEN);
			memcpy(req_say.text, argv[1], TEXT_LEN);
			memcpy(output, &req_say, output_size);
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

rid_t resolve_cmd(char *input){
	if(!strncmp(input, "/exit", 5))
		return REQ_LOGOUT;
	else
		return RSP_ERR;
}

void user_prompt(){
	char *input;
	char **argv;
	int n;

	input = malloc(BUFSIZE+STR_PADD);
	if(!input){
		perror("Error in malloc");
		exit(1);
	}

	argv = malloc((sizeof(char *))*2);
	if(!argv){
		perror("Error in malloc");
		exit(1);
	}
	/*FIX: Hardcoded channel name*/
	argv[0] = channel_name;
	argv[1] = input;

	while(1){	
		memset(input, 0, BUFSIZE+STR_PADD);
		write(1, "> ", 2);

		n=0;
		//argv[0] = session->_active_channel->name;
		while(n < BUFSIZE){	
			if((read(0, &input[n], 1)) < 1)
				break;
			if(input[n] == 0x0a || input[n] == 0x00)	
				break;
			n++;	
		}
		//FIX: Hardcoded channel name
		argv[0] = channel_name;
		if(input[0] == 0x2f){
			if(resolve_cmd(input) == REQ_LOGOUT)
				break;
		}else{	
			build_request(REQ_SAY, 2, argv);
			send_data();
			//send_channel_request(_OUT_SAY);
		}
	}

	free(argv);
	free(input);

	return;
}

int send_login(){
	if(build_request(REQ_LOGIN, 0, NULL) != REQ_LOGIN){
		fprintf(stderr, "Error: failed to build login request\n");
		return -1;
	}
	if(send_data() < 0)
		return -1;

	return 0;
}

int main(int argc, char *argv[]){
	int n;

	if(argc != 4) {
		fprintf(stderr,"usage: %s <hostname> <port> <username>\n", argv[0]);
		exit(0);
	}
	n = atoi(argv[2]);
	if(n < 0 || n > 65535){
		fprintf(stderr, "Error: invalid port number!\nPort number must be between 0 and 65535.");
		exit(0);
	}
	n = strlen(argv[3]);
	if(n < 1 || n > NAME_LEN){
		fprintf(stderr, "Error: username has invalid length!\nUsername must be between 1 and 32 characters in length.");
		exit(0);
	}

	memset(&server_info, 0, sizeof(struct _server_info));
	memset(&client_info, 0, sizeof(struct _client_info));
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
	printf("Client Info:\n\tConnecting to (%s:%d)\n\tLogin as (%s)\n", server_info.hostname, ntohs(server_info.serveraddr.sin_port));
	if(send_login() == 0){
		user_prompt();
	}

	return 0;
}
