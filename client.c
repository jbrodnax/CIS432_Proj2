#include "client.h"

const char _CMD_EXIT[]="exit";
const char _CMD_JOIN[]="join";
const char _CMD_LEAVE[]="leave";
const char _CMD_LIST[]="list";
const char _CMD_WHO[]="who";
const char _CMD_SWITCH[]="switch";

struct _req_login req_login;
struct _req_logout req_logout;
struct _req_join req_join;
struct _req_leave req_leave;
struct _req_say req_say;
struct _req_list req_list;
struct _req_who req_who;
struct _req_alive req_alive;

struct _rsp_say rsp_say;

struct _server_info server_info;
struct _client_info client_info;
char output[BUFSIZE+STR_PADD];
size_t output_size;
int sockfd;
fd_set fds;

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
	char *active_channel_name;

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
			output_size = sizeof(struct _req_logout);
			memset(&req_logout, 0, output_size);
			req_logout.type_id = REQ_LOGOUT;
			memcpy(output, &req_logout, output_size);
			return REQ_LOGOUT;
		case REQ_JOIN:
			output_size = sizeof(struct _req_join);
			memset(&req_join, 0, output_size);
			req_join.type_id = REQ_JOIN;
			memcpy(req_join.channel, argv[0], NAME_LEN);
			memcpy(output, &req_join, output_size);
			return REQ_JOIN;
		case REQ_LEAVE:
			output_size = sizeof(struct _req_leave);
			memset(&req_leave, 0, output_size);
			req_leave.type_id = REQ_LEAVE;
			memcpy(req_leave.channel, argv[0], NAME_LEN);
			memcpy(output, &req_leave, output_size);
			return REQ_LEAVE;
		case REQ_SAY:
			active_channel_name = client_info.active_channel->channel_name;
			if(active_channel_name == NULL)
				break;
			output_size = sizeof(struct _req_say);
			memset(&req_say, 0, output_size);
			req_say.type_id = REQ_SAY;
			memcpy(req_say.channel, active_channel_name, NAME_LEN);
			memcpy(req_say.text, argv[0], TEXT_LEN);
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
	return RET_FAILURE;
}

rid_t resolve_cmd(char *input, int cmd_offset){
	struct _channel_sub *ch;
	char *argv[1];
	char channel_name[NAME_LEN+STR_PADD];
	int i = 0;
	int n;

	cmd_offset++;
	if(!memcmp(&input[cmd_offset], _CMD_EXIT, strlen(_CMD_EXIT))){
		if(build_request(REQ_LOGOUT, 0, NULL) == REQ_LOGOUT){
			if(send_data() > -1)
				return REQ_LOGOUT;
			else
				return RET_FAILURE;
		}
	}
	if(!memcmp(&input[cmd_offset], _CMD_JOIN, strlen(_CMD_JOIN))){
		/*Locate channel name string*/
		memset(channel_name, 0, (NAME_LEN+STR_PADD));
		n = (cmd_offset + strlen(_CMD_JOIN));
		while(input[n] < 0x21 && n < BUFSIZE-1)
			n++;
		strncpy(channel_name, &input[n], NAME_LEN);
		/*Attempt to add channel to sub list*/
		if(!channel_add(channel_name, &client_info)){
			return RET_FAILURE;
		}
		argv[0] = channel_name;
		if(build_request(REQ_JOIN, 1, argv) == REQ_JOIN){
			if(send_data() > -1){
				return REQ_JOIN;
			}else{
				channel_remove(channel_search(channel_name, &client_info), &client_info);
				return RET_FAILURE;
			}
		}
	}
	if(!memcmp(&input[cmd_offset], _CMD_LEAVE, strlen(_CMD_LEAVE))){
		memset(channel_name, 0, (NAME_LEN+STR_PADD));
		n = (cmd_offset + strlen(_CMD_LEAVE));
		while(input[n] < 0x21 && n < BUFSIZE-1)
			n++;
		strncpy(channel_name, &input[n], NAME_LEN);
		if((ch = channel_search(channel_name, &client_info)) == NULL)
			return RET_FAILURE;
		argv[0] = channel_name;
		if(build_request(REQ_LEAVE, 1, argv) == REQ_LEAVE){
			if(send_data() > -1){
				channel_remove(ch, &client_info);
				return REQ_LEAVE;
			}else{
				return RET_FAILURE;
			}
		}
	}
	if(!memcmp(&input[cmd_offset], _CMD_SWITCH, strlen(_CMD_SWITCH))){
		memset(channel_name, 0, (NAME_LEN+STR_PADD));
		n = (cmd_offset + strlen(_CMD_SWITCH));
		while(input[n] < 0x21 && n < BUFSIZE-1)
			n++;
		strncpy(channel_name, &input[n], NAME_LEN);
		//FIX: return val for switch command
		if(channel_switch(channel_name, &client_info))
			return 98;
		else
			return RET_FAILURE;
	}

	return RET_FAILURE;
}

void handle_sock_input(int sockfd){
	char buf[BUFSIZE+STR_PADD];
	char safe_name_buf1[NAME_LEN+STR_PADD];
	char safe_name_buf2[NAME_LEN+STR_PADD];
	char safe_text_buf[TEXT_LEN+STR_PADD];
	struct sockaddr_in serveraddr;
	int serverlen;
	int n;
	rid_t type;

	memset(buf, 0, BUFSIZE+STR_PADD);
	memset(&serveraddr, 0, sizeof(struct sockaddr_in));
	serverlen = sizeof(struct sockaddr_in);

	n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr*)&serveraddr, &serverlen);
	if(n < 0){
		perror("Error in recvfrom");
		return;
	}
	//FIX: add server addr and port validation
	memcpy(&type, buf, sizeof(rid_t));
	switch (type){
		case RSP_SAY:
			if(n != sizeof(struct _rsp_say)){
				printf("[-] Error: received Say response with invalid size (%d)\n", n);
				break;
			}
			memcpy(&rsp_say, buf, sizeof(struct _rsp_say));
			memset(safe_name_buf1, 0, (NAME_LEN+STR_PADD));
			memset(safe_name_buf2, 0, (NAME_LEN+STR_PADD));
			memset(safe_text_buf, 0, (TEXT_LEN+STR_PADD));
			memcpy(safe_name_buf1, rsp_say.username, NAME_LEN);
			memcpy(safe_name_buf2, rsp_say.channel, NAME_LEN);
			memcpy(safe_text_buf, rsp_say.text, TEXT_LEN);
			printf("[%s][%s]: %s\n", safe_name_buf2, safe_name_buf1, safe_text_buf);
			return;
		default:
			printf("[-] Error: received response with invalid type (%hu)\n", type);
	}

	return;
}

void handle_user_input(char *input, int n){
	char *argv[1];
	int i = 0;
	int cmd_offset = 0;
	int is_cmd = 0;

	while(i < n){
		if(input[i] == 0x2f && is_cmd == 0){
			is_cmd = 1;
			cmd_offset = i;
		}else if(input[i] < 0x20 || input[i] > 0x7e){
			input[i] = 0x00;
		}
		i++;
	}

	if(is_cmd == 1){
		if(resolve_cmd(input, cmd_offset) == REQ_LOGOUT){
			channel_clean(&client_info);
			free(input);
			cooked_mode();
			exit(EXIT_SUCCESS);
		}
	}else{
		argv[0] = input;
		//FIX: add error checking to the next two calls
		build_request(REQ_SAY, 1, argv);
		send_data();
	}

	return;
}

void user_prompt(){
	char *input;
	char c;
	char dest_bckspace[]="\b \b";
	int n;

	input = malloc(BUFSIZE+STR_PADD);
	if(!input){
		perror("Error in malloc");
		exit(1);
	}

	memset(input, 0, BUFSIZE+STR_PADD);
	n = 0;
	write(1, "> ", 2);
	while(1){
		FD_ZERO(&fds);
		FD_SET(sockfd, &fds);
		FD_SET(STDIN, &fds);
		if(select(sockfd+1, &fds, NULL, NULL, NULL) < 0){
			perror("Error in select");
			exit(EXIT_FAILURE);
		}	
		if(FD_ISSET(STDIN, &fds)){	
			c = getchar();	
			if(c == 0x7f && n > 0){
				n--;
				input[n] = 0;
				write(1, dest_bckspace, strlen(dest_bckspace));
			}else if(n < BUFSIZE-1){
				input[n] = c;
				write(1, &c, 1);
				n++;
				if(c == '\n' || c == '\0'){
					handle_user_input(input, n);
					n = 0;
					memset(input, 0, BUFSIZE+STR_PADD);
					write(1, "> ", 2);
				}
			}else if(n >= BUFSIZE-1){
				if(c == '\n' || c == '\0'){
					handle_user_input(input, n);
					n = 0;
					memset(input, 0, BUFSIZE+STR_PADD);
					write(1, "> ", 2);
				}
			}
			
		}
		if(FD_ISSET(sockfd, &fds)){
			handle_sock_input(sockfd);
			//printf("Ready to read from udp socket\n");
		}
	}

	return;
}

int init_login(){
	char *argv[1];
	char channel_name[]="Common";

	if(build_request(REQ_LOGIN, 0, NULL) != REQ_LOGIN){
		fprintf(stderr, "Error: failed to build login request\n");
		return -1;
	}
	if(send_data() < 0)
		return -1;

	/*Create and send channel sub for 'Common'
	if(!(client_info.active_channel = malloc(sizeof(struct _channel_sub)))){
		perror("Error in malloc");
		exit(EXIT_FAILURE);
	}
	memset(client_info.active_channel, 0, sizeof(struct _channel_sub));
	memcpy(client_info.active_channel->channel_name, channel_name, strlen(channel_name));
	argv[0] = client_info.active_channel->channel_name;
	*/
	argv[0] = channel_name;
	if(build_request(REQ_JOIN, 1, argv) != REQ_JOIN){
		fprintf(stderr, "Error: init_login failed to build join request\n.");
		return -1;
	}
	if(send_data() < 0)
		return -1;
	client_info.active_channel = channel_add(channel_name, &client_info);
	if(!client_info.active_channel)
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
	strncpy(client_info.username, argv[3], n);

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
	printf("Client Info:\n\tConnecting to (%s:%d)\n\tLogin as (%s)\n", server_info.hostname, ntohs(server_info.serveraddr.sin_port), client_info.username);
	if(raw_mode() == -1){
		fprintf(stderr, "[!] Error: raw_mode failed\n");
		exit(EXIT_FAILURE);
	}
	if(init_login() == 0){	
		user_prompt();
	}

	return 0;
}
