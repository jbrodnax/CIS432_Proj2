#include "client.h"

const char _CMD_EXIT[]="exit";
const char _CMD_JOIN[]="join";
const char _CMD_LEAVE[]="leave";
const char _CMD_LIST[]="list";
const char _CMD_WHO[]="who";
const char _CMD_SWITCH[]="switch";
const char DST_BACKSPACE[]="\b \b";
const char PROMPT_SYM[]="> ";

struct _req_login req_login;
struct _req_logout req_logout;
struct _req_join req_join;
struct _req_leave req_leave;
struct _req_say req_say;
struct _req_list req_list;
struct _req_who req_who;
struct _req_alive req_alive;

struct _rsp_say rsp_say;
struct _rsp_list rsp_list;
struct _rsp_who rsp_who;
struct _rsp_err rsp_err;

struct _server_info server_info;
struct _client_info client_info;

char output[BUFSIZE+STR_PADD];
size_t output_size;
int sockfd;
fd_set fds;
pthread_t tid[1];
pthread_mutex_t lock1 = PTHREAD_MUTEX_INITIALIZER;

int send_data(){
	int n, serverlen;

	serverlen = sizeof(struct sockaddr);
	if(output_size < sizeof(rid_t) || output_size > BUFSIZE){
		fprintf(stderr, "Error in send_data: output_size has invalid size.\n");
		return -1;
	}

	n = sendto(sockfd, output, output_size, 0, (struct sockaddr *)&server_info.serveraddr, serverlen);
	if(n < 0){
		perror("[!] Error in sendto");
		return -1;
	}

	pthread_mutex_lock(&lock1);
	client_info.timestamp = time(NULL);
	pthread_mutex_unlock(&lock1);
	return 0;
}

void *thread_keepalive(){
	int n, serverlen;
	time_t current_time;

	serverlen = sizeof(struct sockaddr);
	while(1){
		sleep(CSS_TIMEOUT);
		pthread_mutex_lock(&lock1);
		current_time = time(NULL);
		if((current_time - client_info.timestamp) >= CSS_TIMEOUT){
			req_alive.type_id = REQ_ALIVE;
			n = sendto(sockfd, &req_alive, sizeof(struct _req_alive), 0, (struct sockaddr *)&server_info.serveraddr, serverlen);
			if(n < 0){	
				perror("Error in sendto");
				exit(EXIT_FAILURE);
			}
		}
		pthread_mutex_unlock(&lock1);
	}
}

rid_t build_request(rid_t type, int argc, char *argv[]){
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
			if(argc != 1)
				return RET_FAILURE;
			output_size = sizeof(struct _req_join);
			memset(&req_join, 0, output_size);
			req_join.type_id = REQ_JOIN;
			memcpy(req_join.channel, argv[1], NAME_LEN);
			memcpy(output, &req_join, output_size);	
			return REQ_JOIN;
		case REQ_LEAVE:
			if(argc != 1)
				return RET_FAILURE;
			output_size = sizeof(struct _req_leave);
			memset(&req_leave, 0, output_size);
			req_leave.type_id = REQ_LEAVE;
			memcpy(req_leave.channel, argv[1], NAME_LEN);
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
			output_size = sizeof(struct _req_list);
			memset(&req_list, 0, output_size);
			req_list.type_id = REQ_LIST;
			memcpy(output, &req_list, output_size);
			return REQ_LIST;
		case REQ_WHO:
			if(argc != 1)
				return RET_FAILURE;
			output_size = sizeof(struct _req_who);
			memset(&req_who, 0, output_size);
			req_who.type_id = REQ_WHO;
			memcpy(req_who.channel, argv[1], NAME_LEN);
			memcpy(output, &req_who, output_size);
			return REQ_WHO;
		case REQ_ALIVE:
			break;
		default:
			break;
	}
	return RET_FAILURE;
}

rid_t resolve_cmd(int argc, char *argv[]){
	struct _channel_sub *ch;
	char channel_name[NAME_LEN+STR_PADD];	

	if(!memcmp(argv[0], _CMD_EXIT, strlen(_CMD_EXIT))){
		if(build_request(REQ_LOGOUT, 0, NULL) == REQ_LOGOUT){
			if(send_data() > -1)
				return REQ_LOGOUT;
			else
				return RET_FAILURE;
		}
	}
	else if(!memcmp(argv[0], _CMD_JOIN, strlen(_CMD_JOIN))){	
		memset(channel_name, 0, (NAME_LEN+STR_PADD));
		memcpy(channel_name, argv[1], NAME_LEN);
		/*Attempt to add channel to sub list*/
		if(!channel_add(channel_name, &client_info)){
			return RET_FAILURE;
		}
		if(build_request(REQ_JOIN, argc, argv) == REQ_JOIN){
			if(send_data() > -1){
				return REQ_JOIN;
			}else{
				channel_remove(channel_search(channel_name, &client_info), &client_info);
				return RET_FAILURE;
			}
		}
	}
	else if(!memcmp(argv[0], _CMD_LEAVE, strlen(_CMD_LEAVE))){
		memset(channel_name, 0, (NAME_LEN+STR_PADD));
		memcpy(channel_name, argv[1], NAME_LEN);
		if((ch = channel_search(channel_name, &client_info)) == NULL)
			return RET_FAILURE;
		if(!strncmp(ch->channel_name, client_info.active_channel->channel_name, NAME_LEN)){
			printf("[-] Error: cannot leave active channel. Please switch first.\n");
			return RET_FAILURE;
		}
		if(build_request(REQ_LEAVE, argc, argv) == REQ_LEAVE){
			if(send_data() > -1){
				channel_remove(ch, &client_info);
				return REQ_LEAVE;
			}else{
				return RET_FAILURE;
			}
		}
	}
	else if(!memcmp(argv[0], _CMD_SWITCH, strlen(_CMD_SWITCH))){
		memset(channel_name, 0, (NAME_LEN+STR_PADD));
		memcpy(channel_name, argv[1], NAME_LEN);
		//FIX: return val for switch command
		if(channel_switch(channel_name, &client_info))
			return 98;
		else
			return RET_FAILURE;
	}
	else if(!memcmp(argv[0], _CMD_LIST, strlen(_CMD_LIST))){
		if(build_request(REQ_LIST, 0, NULL) == REQ_LIST){
			if(send_data() > -1)
				return REQ_LIST;
			else
				return RET_FAILURE;
		}
	}
	else if(!memcmp(argv[0], _CMD_WHO, strlen(_CMD_WHO))){
		memset(channel_name, 0, (NAME_LEN+STR_PADD));
                memcpy(channel_name, argv[1], NAME_LEN);
		if(build_request(REQ_WHO, argc, argv) == REQ_WHO){
			if(send_data() > -1)
				return REQ_WHO;
			else
				return RET_FAILURE;
		}
	}

	return RET_FAILURE;
}

void handle_sock_input(int sockfd, char *input){
	/*In order to handle possible list and who responses of ~4,136 bytes
	this function has a uniquely sized input buffer*/
	char buf[sizeof(struct _rsp_who)+STR_PADD];
	char safe_name_buf1[NAME_LEN+STR_PADD];
	char safe_name_buf2[NAME_LEN+STR_PADD];
	char safe_text_buf[TEXT_LEN+STR_PADD];
	struct sockaddr_in serveraddr;
	socklen_t serverlen;
	int n, m, l, offset;
	rid_t type, i;

	memset(buf, 0, (sizeof(struct _rsp_who)+STR_PADD));
	memset(&serveraddr, 0, sizeof(struct sockaddr_in));
	serverlen = sizeof(struct sockaddr_in);

	m = recvfrom(sockfd, buf, (sizeof(struct _rsp_who)), 0, (struct sockaddr*)&serveraddr, &serverlen);
	if(m < 0){
		perror("Error in recvfrom");
		return;
	}
	//FIX: add server addr and port validation
	memcpy(&type, buf, sizeof(rid_t));
	if(type > RSP_ERR){
		printf("[-] Error: received response with invalid type (%hu)\n", type);
		return;
	}

	/*Clean user prompt*/
	l = (strlen(input) + strlen(PROMPT_SYM));
	for(n=0; n < l; n++)
		write(1, DST_BACKSPACE, strlen(DST_BACKSPACE));

	switch (type){
		case RSP_SAY:
			if(m != sizeof(struct _rsp_say)){
				printf("[-] Error: received Say response with invalid size (%d)\n", m);
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
			break;
		case RSP_LIST:
			memset(&rsp_list, 0, sizeof(struct _rsp_list));
			memcpy(&rsp_list, buf, m);
			if(rsp_list.num_channels < 1 || rsp_list.num_channels > LIST_LEN){
				printf("[-] Error: list response had invalid number of channels\n");
				break;
			}
			printf("Existing channels:\n");
			memset(safe_name_buf1, 0, (NAME_LEN+STR_PADD));
			offset = 0;
			for(i=0; i < rsp_list.num_channels; i++){
				memcpy(safe_name_buf1, &rsp_list.channel_list[offset], NAME_LEN);
				printf("\t%s\n", safe_name_buf1);
				offset+=NAME_LEN;
			}
			break;
		case RSP_WHO:
			memset(&rsp_who, 0, sizeof(struct _rsp_who));
			memcpy(&rsp_who, buf, m);
			if(rsp_who.num_users > WHO_LEN){
				printf("[-] Error: who response had invalid number of users\n");
				break;
			}
			memset(safe_name_buf1, 0, (NAME_LEN+STR_PADD));
			memcpy(safe_name_buf1, rsp_who.channel, NAME_LEN);
			printf("Users on channel %s:\n", safe_name_buf1);
			offset = 0;
			for(i=0; i < rsp_who.num_users; i++){
				memcpy(safe_name_buf1, &rsp_who.users[offset], NAME_LEN);
				printf("\t%s\n", safe_name_buf1);
				offset+=NAME_LEN;
			}
			break;
		case RSP_ERR:
			memset(&rsp_err, 0, sizeof(struct _rsp_err));
			memcpy(&rsp_err, buf, m);
			memset(safe_text_buf, 0, (TEXT_LEN+STR_PADD));
			memcpy(safe_text_buf, rsp_err.message, TEXT_LEN);
			printf("[Server Error]: %s\n", safe_text_buf);
			break;
		default:
			printf("[-] Error: received response with invalid type (%hu)\n", type);
	}
	/*Restore user prompt*/
	l = strlen(input);
	write(1, PROMPT_SYM, strlen(PROMPT_SYM));
	for(n=0; n < l; n++)
		write(1, &input[n], 1);

	return;
}

void sanitize_input(char *input, int n){
	int i;

	for(i=0; i < n; i++){
		if(input[i] < 0x20 || input[i] > 0x7e){
			input[i] = 0x00;
		}
	}

	return;
}

void handle_user_input(char *input, int n){
/*Isn't this function beautiful?...*/
	char *argv[2];
	char cmd_name[MAXCMD_LEN+STR_PADD];
	char cmd_arg1[NAME_LEN+STR_PADD];	
	int j;
	int i = 0;

	/*Eat any leading spaces*/
	while(input[i] == 0x20){
		i++;
		if(i > BUFSIZE)
			break;	
	}
	/*if 1st non-space character is '/' then treat input as a command*/
	if(input[i] == 0x2f){	
		memset(cmd_name, 0, (MAXCMD_LEN+STR_PADD));
		i++;
		j = 0;
		/*Copy chars following '/' until non-alphabet char or length exceeds MAXCMD_LEN*/
		sanitize_input(input, n);
		while(1){
			if(input[i] < 0x41 || input[i] > 0x7a)
				break;
			cmd_name[j] = input[i];
			i++;
			j++;
			if(j > MAXCMD_LEN){
				printf("[-] Invalid command\n");
				return;
			}
		}
		/*Assume command has no arguments*/
		if(input[i] == 0x00){
			argv[0] = cmd_name;	
			if(resolve_cmd(0, argv) == REQ_LOGOUT){
				channel_clean(&client_info);
				free(input);
				cooked_mode();
				exit(EXIT_SUCCESS);
			}else{
				return;
			}
		}else if(input[i] == 0x20){
			i++;
			memset(cmd_arg1, 0, NAME_LEN+STR_PADD);
			/*Get strlen of command arg*/
			j = strlen(&input[i]);	
			if(j > NAME_LEN || j < 1){
				printf("[-] Invalid command argument\n");
				return;
			}
			strncpy(cmd_arg1, &input[i], j);
			argv[0] = cmd_name;
			argv[1] = cmd_arg1;
			if(resolve_cmd(1, argv) == RET_FAILURE)
				printf("[-] Command failure!\n");
			return;
		}else{
			printf("[-] Invalid command\n");
			return;
		}
	}

	sanitize_input(input, n);
	argv[0] = input;
	if(build_request(REQ_SAY, 1, argv) == REQ_SAY){
		if(send_data() > -1)
			return;
	}

	printf("[-] Failed to send message!\n");
	return;
}

void user_prompt(){
	char *input;
	char c;
	int n, promptlen;

	input = malloc(BUFSIZE+STR_PADD);
	if(!input){
		perror("Error in malloc");
		exit(1);
	}

	memset(input, 0, BUFSIZE+STR_PADD);
	n = 0;
	promptlen = strlen(PROMPT_SYM);
	write(1, PROMPT_SYM, promptlen);
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
				write(1, DST_BACKSPACE, strlen(DST_BACKSPACE));
			}else if(n < BUFSIZE-1){
				input[n] = c;
				write(1, &c, 1);
				n++;
				if(c == '\n' || c == '\0'){
					handle_user_input(input, n);
					n = 0;
					memset(input, 0, BUFSIZE+STR_PADD);
					write(1, PROMPT_SYM, promptlen);
				}
			}else if(n >= BUFSIZE-1){
				if(c == '\n' || c == '\0'){
					handle_user_input(input, n);
					n = 0;
					memset(input, 0, BUFSIZE+STR_PADD);
					write(1, PROMPT_SYM, promptlen);
				}
			}
			
		}
		if(FD_ISSET(sockfd, &fds)){
			handle_sock_input(sockfd, input);	
		}
	}

	return;
}

int init_login(){
	char *argv[2];
	char channel_name[]="Common";

	if(build_request(REQ_LOGIN, 0, NULL) != REQ_LOGIN){
		fprintf(stderr, "Error: failed to build login request\n");
		return -1;
	}
	if(send_data() < 0)
		return -1;

	argv[0] = NULL;
	argv[1] = channel_name;
	if(build_request(REQ_JOIN, 1, argv) != REQ_JOIN){
		fprintf(stderr, "Error: init_login failed to build join request\n.");
		return -1;
	}
	if(send_data() < 0)
		return -1;
	client_info.active_channel = channel_add(channel_name, &client_info);
	if(!client_info.active_channel)
		return -1;

	pthread_create(&tid[0], NULL, thread_keepalive, NULL);

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
