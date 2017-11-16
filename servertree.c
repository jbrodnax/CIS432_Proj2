#include "server.h"

void error_msg(char *err_msg){
	if(!err_msg){
		perror("[-] ERROR:\t ");
		return;
	}

	printf("[-] ERROR:\t %s\n", err_msg);
	return;
}

void node_init_socket(){

}

struct _adjacent_server *node_create(char *hostname, int port, struct _server_manager *svm){
	struct _adjacent_server *new_node;
	struct addrinfo hints, *servinfo, *p;
	int rv, optval;

	if(!hostname || !svm || port < 1){
		error_msg("node_create received invalid argument.");
		return NULL;
	}
	if(svm->tree_size >= TREE_MAX){
		return NULL;
	}

	new_node = malloc(sizeof(struct _adjacent_server));
	if(!new_node){
		perror("Error in malloc");
		exit(EXIT_FAILURE);
	}
	memset(new_node, 0, sizeof(struct _adjacent_server));
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;
	if((rv = getaddrinfo(server_info.hostname, server_info.portno_str, &hints, &servinfo)) != 0){
		fprintf(stderr, "in getaddrinfo: %s\n", gai_strerror(rv));
		exit(1);
	}

	optval = 1;
	for(p = servinfo; p!=NULL; p = p->ai_next){
		if((new_node->sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
			perror("[?] Error in socket");
			continue;
		}
		setsockopt(new_node->sockfd, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
		break;
	}
	

	return new_node;
}