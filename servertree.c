#include "server.h"

struct _adjacent_server *node_create(char *hostname, char *port, struct _server_manager *svm){
	struct _adjacent_server *new_node;
	struct addrinfo hints, *p;
	int rv, optval;

	if(!hostname || !port || !svm){
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
	strncpy(new_node->ipaddr, hostname, 64);
	strncpy(new_node->port_str, port, 32);

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;
	if((rv = getaddrinfo(new_node->ipaddr, new_node->port_str, &hints, &new_node->servinfo)) != 0){
		fprintf(stderr, "in getaddrinfo: %s\n", gai_strerror(rv));
		exit(1);
	}

	optval = 1;
	for(p = new_node->servinfo; p!=NULL; p = p->ai_next){
		if((new_node->sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
			perror("[?] Error in socket");
			continue;
		}
		setsockopt(new_node->sockfd, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
		if(bind(new_node->sockfd, p->ai_addr, p->ai_addrlen) == -1){
			close(new_node->sockfd);
			perror("Error in bind");
			continue;
		}
		break;
	}
	if(p == NULL){
		fprintf(stderr, "[!] Failed to bind socket\n");
		exit(EXIT_FAILURE);
	}
	new_node->serveraddr = (struct sockaddr_in*)p->ai_addr;

	return new_node;
}

int node_add(struct _adjacent_server *node, struct _server_manager *svm){
	/*adds an already initialized node to the server tree and inits the node's index*/
	if(!node || !svm){
		error_msg("node_add received null argument.");
		return -1;
	}
	if(svm->tree_size >= TREE_MAX){
		error_msg("server tree is full.");
		return -1;
	}

	svm->tree[svm->tree_size] = node;
	node->index = svm->tree_size;
	svm->tree_size++;

	return svm->tree_size;
}

int node_remove(struct _adjacent_server *node, struct _server_manager *svm){
	/*The svm tree array is not rearranged when a node is removed
	since node's keep track of their init index in the array*/
	if(!node || !svm){
		error_msg("node_remove received null argument.");
		return -1;
	}
	if(svm->tree_size < 1 || node->index > svm->tree_size){
		error_msg("node_remove received invalid node index.");
		return -1;
	}

	close(node->sockfd);
	freeaddrinfo(node->servinfo);
	free(node);
	svm->tree[node->index] = NULL;
	svm->tree_size--;

	return 0;
}

struct _adjacent_server *node_search(struct sockaddr_in *serveraddr, struct _server_manager *svm){
	struct _adjacent_server *node;
	int n;

	if(!serveraddr || !svm){
		error_msg("node_search received null argument.");
		return NULL;
	}

	for(n=0; n < svm->tree_size; n++){
		node = svm->tree[n];
		if(!node)
			continue;
		if(serveraddr->sin_addr.s_addr == node->serveraddr->sin_addr.s_addr){
			if(serveraddr->sin_port == node->serveraddr->sin_port){
				return node;
			}
		}
	}

	return NULL;
}

unique_t generate_id(struct _S2S_say *req){
	unique_t id;
	FILE *fp;

	if(!req){
		error_msg("generate_id received null request pointer.");
		return 0;
	}

	if(!(fp = fopen("/dev/urandom", "r"))){
		perror("Error reading from /dev/urandom");
		exit(EXIT_FAILURE);
	}
	if(!(fread(&id, sizeof(unique_t), 1, fp))){
		perror("Error reading from /dev/urandom");
                exit(EXIT_FAILURE);
	}
	fclose(fp);

	req->msg_id = id;
	printf("generated id: %llu\n", id);
	return id;
}

int rtable_init(struct channel_entry *ch, struct _server_manager *svm){
	int n, i;

	if(!ch || !svm){
		error_msg("rtable_init received null arguments.");
		return -1;
	}

	n = 0;
	i = 0;
	memset(ch->routing_table, 0, TREE_MAX);
	while(n < svm->tree_size && i < TREE_MAX){
		if(svm->tree[i] != NULL){
			ch->routing_table[n] = svm->tree[i];
			n++;
		}
		i++;
	}
	ch->table_size = n;

	return 0;
}

int rtable_prune(struct channel_entry *ch, struct _adjacent_server *node, struct _server_manager *svm){
	struct _adjacent_server *node2;
	int n, opt, i;

	if(!ch || !node || !svm){
		error_msg("rtable_prune received null argument.");
		return -1;
	}

	n = 0;
	opt = 0;
	while(n < TREE_MAX){
		node2 = ch->routing_table[n];
		if(node2 == NULL){
			n++;
			continue;
		}
		if(node2->serveraddr->sin_addr.s_addr == node->serveraddr->sin_addr.s_addr){
			if(node2->serveraddr->sin_port == node->serveraddr->sin_port){
				opt = 1;
				break;
			}
		}
		n++;
	}
	if(opt == 0)
		return -1;

	for(i=n; i < TREE_MAX-1; i++)
		ch->routing_table[i] = ch->routing_table[i+1];
	/*Since at least one entry should be open now, zero-out the */
	ch->routing_table[i] = NULL;
	if(ch->table_size > 0)
		ch->table_size--;

	return 0;
}

int propogate_join(struct channel_entry *ch, struct _S2S_join *req){
	struct _adjacent_server *node;
	int n, i;

	if(!ch || !req){
		error_msg("propogate_join received null argument.");
		return -1;
	}

	for(n=0; n < ch->table_size; n++){
		node = ch->routing_table[n];
		if(!node)
			continue;
		i = sendto(node->sockfd, req, sizeof(struct _S2S_join), 0, (struct sockaddr *)&node->serveraddr, sizeof(struct sockaddr));
		if(i < 0)
			perror("Error in sendto JOIN");
	}
	
	return n;
}

int propogate_leave(struct channel_entry *ch, struct _S2S_leave *req){
	struct _adjacent_server *node;
	int n, i;

	if(!ch || !req){
		error_msg("propogate_leave received null argument.");
		return -1;
	}

	for(n=0; n < ch->table_size; n++){
		node = ch->routing_table[n];
		if(!node)
			continue;
		i = sendto(node->sockfd, req, sizeof(struct _S2S_leave), 0, (struct sockaddr *)&node->serveraddr, sizeof(struct sockaddr));
		if(i < 0)
			perror("Error in sendto");
	}
	
	return n;
}













