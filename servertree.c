#include "server.h"

struct _adjacent_server *node_create(char *hostname, char *port, struct _server_manager *svm){
	struct _adjacent_server *new_node;
	struct addrinfo *p;
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
	memset(&new_node->hints, 0, sizeof(new_node->hints));
	strncpy(new_node->hostname, hostname, 256);
	strncpy(new_node->port_str, port, 32);

	new_node->hints.ai_family = AF_INET;
	new_node->hints.ai_socktype = SOCK_DGRAM;
	new_node->hints.ai_flags = AI_PASSIVE;
	if((rv = getaddrinfo(new_node->hostname, new_node->port_str, &new_node->hints, &new_node->servinfo)) != 0){
		fprintf(stderr, "in getaddrinfo: %s\n", gai_strerror(rv));
		exit(1);
	}
	new_node->serveraddr = (struct sockaddr_in*)new_node->servinfo->ai_addr;
	getnameinfo((struct sockaddr*)new_node->serveraddr, sizeof(struct sockaddr),
		new_node->ipaddr, 64, new_node->port_str, 32, NI_NUMERICHOST | NI_NUMERICSERV);
	
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

	for(n=0; n < TREE_MAX; n++){
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
	//printf("generated id: %llu\n", id);
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

int propogate_join(struct channel_entry *ch, struct _adjacent_server *sender, int sockfd){
	struct _adjacent_server *node;
	struct _S2S_join *join;
	int n, i;

	if(!ch){
		error_msg("propogate_join received null argument.");
		return -1;
	}
	if(!(join = malloc(sizeof(struct _S2S_join)))){
		perror("Error in malloc");
		exit(EXIT_FAILURE);
	}
	memset(join, 0, sizeof(struct _S2S_join));
	join->type_id = S2S_JOIN;
	memcpy(join->channel, ch->channel_name, NAME_LEN);

	for(n=0; n < ch->table_size; n++){
		if(!(node = ch->routing_table[n]))
			continue;
		/*Don't send join request to server that sent it, if any.*/
		if(sender){
			if(sender->serveraddr->sin_addr.s_addr == node->serveraddr->sin_addr.s_addr){
				if(sender->serveraddr->sin_port == node->serveraddr->sin_port)
					continue;
			}
		}
		snprintf(LOG_SEND, LOGMSG_LEN, "%s:%s\tsend S2S Join %s", node->ipaddr, node->port_str, ch->channel_name);
		log_send();
		i = sendto(sockfd, join, sizeof(struct _S2S_join), 0, (struct sockaddr *)node->serveraddr, sizeof(struct sockaddr));
		if(i < 0)
			perror("Error in sendto JOIN");
	}
	free(join);
	return n;
}
/*This storage method for ids is terrible*/
int save_id(unique_t id, struct _server_manager *svm){
	int n, retval;

	retval = 0;
	for(n=0; n < svm->num_ids; n++){
		if(n >= UID_MAX-1){
			//memcpy(svm->recent_ids, &svm->recent_ids[(UID_MAX/2)], (UID_MAX*sizeof(unique_t)));
			//memset(&svm->recent_ids[(UID_MAX/2)], 0, (UID_MAX*sizeof(unique_t)));
			memset(svm->recent_ids, 0, sizeof(unique_t)*UID_MAX);
			if(retval == 0){
				svm->recent_ids[0] = id;
				svm->num_ids = 1;
				return retval;
			}
		}
		if(svm->recent_ids[n] == id)
			retval = 1;
	}
	if(retval == 0){
		svm->recent_ids[n] = id;
		svm->num_ids++;
	}
	return retval;
}

struct _S2S_say *create_S2S_say(_sreq_say *req, char *name, struct _server_manager *svm){
	struct _S2S_say *rsp;

	if(!req || !name){
		error_msg("create_s2s_say received null argument.");
		return NULL;
	}
	if(!(rsp = malloc(sizeof(struct _S2S_say)))){
		perror("Error in malloc");
		exit(EXIT_FAILURE);
	}
	memset(rsp, 0, sizeof(struct _S2S_say));
	rsp->type_id = S2S_SAY;
	memcpy(rsp->username, name, NAME_LEN);
	memcpy(rsp->channel, req->channel, NAME_LEN);
	memcpy(rsp->text, req->text, TEXT_LEN);
	if(save_id(generate_id(rsp), svm) > 0){
		error_msg("generated say request with non-unique id.");
	}

	return rsp;
}

int propogate_say(struct channel_entry *ch, char *name, _sreq_say *req, struct _adjacent_server *sender, int sockfd, struct _server_manager *svm){
	struct _adjacent_server *node;
	struct _S2S_say *rsp;
	int n, i;

	if(!req){
		error_msg("prop_say received null argument.");
		return -1;
	}
	if(!(rsp = malloc(sizeof(struct _S2S_say)))){
		perror("Error in malloc");
		exit(EXIT_FAILURE);
	}
	memset(rsp, 0, sizeof(struct _S2S_say));
	rsp->type_id = S2S_SAY;
	memcpy(rsp->username, name, NAME_LEN);
        memcpy(rsp->channel, req->channel, NAME_LEN);
        memcpy(rsp->text, req->text, TEXT_LEN);
	//if(save_id(generate_id(rsp), svm) < 0){
	//	error_msg("generated say request with non-unique id.");
	//	return -1;
	//}
	if(ch == NULL && svm != NULL){
		goto PROPALL;
	}else{
		goto PROPCH;
	}

	PROPALL:
		if(save_id(generate_id(rsp), svm) < 0){
			error_msg("generated say request with non-unique id.");
			return -1;
		}
		for(n=0;n<TREE_MAX;n++){
			if(!(node = svm->tree[n]))
				continue;
			if(sender){
				if(sender->serveraddr->sin_addr.s_addr == node->serveraddr->sin_addr.s_addr){
					if(sender->serveraddr->sin_port == node->serveraddr->sin_port)
						continue;
				}
			}
			snprintf(LOG_SEND, LOGMSG_LEN, "%s:%s\tsend S2S Say %s %s %s", node->ipaddr, node->port_str, name, req->channel, req->text);
			log_send();
			if(sendto(sockfd, rsp, sizeof(struct _S2S_say), 0, (struct sockaddr *)node->serveraddr, sizeof(struct sockaddr)) < 0)
				perror("Error in sendto");
		}
		free(rsp);
		return n;

	PROPCH:
		for(n=0;n<ch->table_size;n++){
			if(!(node = ch->routing_table[n]))
				continue;
			if(sender){
				if(sender->serveraddr->sin_addr.s_addr == node->serveraddr->sin_addr.s_addr){
					if(sender->serveraddr->sin_port == node->serveraddr->sin_port)
						continue;
				}
			}
			snprintf(LOG_SEND, LOGMSG_LEN, "%s:%s\tsend S2S Say %s %s %s", node->ipaddr, node->port_str, name, req->channel, req->text);
			log_send();
			if(sendto(sockfd, rsp, sizeof(struct _S2S_say), 0, (struct sockaddr *)node->serveraddr, sizeof(struct sockaddr)) < 0)
				perror("Error in sendto");
		}
		free(rsp);
		return n;
}
/*
int propogate_say(struct channel_entry *ch, struct _S2S_say *req, int sockfd, struct _server_manager *svm){
	struct _adjacent_server *node;
	int n, i;

	if(!req){
		error_msg("prop_say received null argument.");
		return -1;
	}

	if(ch == NULL && svm != NULL){
		for(n=0; n < TREE_MAX; n++){
			node = svm->tree[n];
			if(!node)
				continue;
			snprintf(LOG_SEND, LOGMSG_LEN, "%s:%s\tsend S2S Say %s %s %s", node->ipaddr, node->port_str, req->username, req->channel, req->text);
			log_send();
			i = sendto(sockfd, req, sizeof(struct _S2S_say), 0, (struct sockaddr *)node->serveraddr, sizeof(struct sockaddr));
			if(i < 0)
				perror("Error in sendto");
		}
		return n;
	}

	for(n=0; n < ch->table_size; n++){
		node = ch->routing_table[n];
		if(!node)
			continue;	
		snprintf(LOG_SEND, LOGMSG_LEN, "%s:%s\tsend S2S Say %s %s %s", node->ipaddr, node->port_str, req->username, req->channel, req->text);
		log_send();
		i = sendto(sockfd, req, sizeof(struct _S2S_say), 0, (struct sockaddr *)node->serveraddr, sizeof(struct sockaddr));
		if(i < 0)
			perror("Error in sendto");
	}

	return n;
}
*/
int send_leave(char *ch, struct _adjacent_server *node, int sockfd){
	//struct _adjacent_server *node;
	struct _S2S_leave req;
	int n, i;

	if(!ch || !node){
		error_msg("send_leave received null argument.");
		return -1;
	}
	memset(&req, 0, sizeof(struct _S2S_leave));
	req.type_id = S2S_LEAVE;
	memcpy(req.channel, ch, NAME_LEN);

	n = sendto(sockfd, &req, sizeof(struct _S2S_leave), 0, (struct sockaddr *)node->serveraddr, sizeof(struct sockaddr));
	if(n < 0)
		perror("Error in sendto");
	
	return n;
}













