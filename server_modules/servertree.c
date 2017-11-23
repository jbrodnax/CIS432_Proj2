#include "../server.h"

struct _adjacent_server *node_create(char *hostname, char *port, struct _server_manager *svm){
	struct _adjacent_server *new_node;	
	int rv;

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

	pthread_rwlock_wrlock(&node_lock);
	svm->tree[svm->tree_size] = node;
	node->index = svm->tree_size;
	svm->tree_size++;

	pthread_rwlock_unlock(&node_lock);
	return svm->tree_size;
}

int node_remove(struct _adjacent_server *node, struct _server_manager *svm){
	int i;

	if(!node || !svm){
		error_msg("node_remove received null argument.");
		return -1;
	}
	if(svm->tree_size < 1 || node->index > svm->tree_size){
		error_msg("node_remove received invalid node index.");
		return -1;
	}

	pthread_rwlock_wrlock(&node_lock);
	for(i=node->index; i < svm->tree_size; i++){
		svm->tree[i] = svm->tree[i+1];
		if(svm->tree[i] != NULL)
			svm->tree[i]->index--;
	}
	freeaddrinfo(node->servinfo);
	free(node);
	svm->tree[TREE_MAX-1] = NULL;
	svm->tree_size--;

	pthread_rwlock_unlock(&node_lock);
	return 0;
}

struct _adjacent_server *node_search(struct sockaddr_in *serveraddr, struct _server_manager *svm){
	struct _adjacent_server *node;
	int n;

	if(!serveraddr || !svm){
		error_msg("node_search received null argument.");
		return NULL;
	}

	pthread_rwlock_rdlock(&node_lock);
	for(n=0; n < TREE_MAX; n++){
		node = svm->tree[n];
		if(!node)
			continue;
		if(serveraddr->sin_addr.s_addr == node->serveraddr->sin_addr.s_addr){
			if(serveraddr->sin_port == node->serveraddr->sin_port){
				pthread_rwlock_unlock(&node_lock);
				return node;
			}
		}
	}

	pthread_rwlock_unlock(&node_lock);
	return NULL;
}

int node_compare(struct _adjacent_server *node1, struct _adjacent_server *node2){

	if(!node1 || !node2)	
		return -1;

	if(node1->serveraddr->sin_addr.s_addr == node2->serveraddr->sin_addr.s_addr){
		if(node1->serveraddr->sin_port == node2->serveraddr->sin_port){
			return 0;
		}
	}

	return -1;
}

void init_id_rng(){
	int n;
	unsigned int seed;
	FILE *fp;

	if(!(fp = fopen("/dev/urandom", "r"))){
		perror("Error reading from /dev/urandom");
		exit(EXIT_FAILURE);
	}
	if(!(fread(&seed, sizeof(unsigned int), 1, fp))){
		perror("Error reading from /dev/urandom");
		exit(EXIT_FAILURE);
	}
	fclose(fp);
	srand(seed);

	puts("Init IDs:");
	for(n=0;n<5;n++)
		printf("%u\n", ((unsigned int)rand()%RAND_MAX));
	return;
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
	int n;

	if(!ch || !svm){
		error_msg("rtable_init received null arguments.");
		return -1;
	}

	n = 0;
	/*rtable_init is only called by channel_create which has already locked the channel lock*/
	pthread_rwlock_rdlock(&node_lock);
	memset(ch->routing_table, 0, TREE_MAX);
	memset(ch->ss_rtable, 0, TREE_MAX);
	while(n < svm->tree_size){
		ch->routing_table[n] = svm->tree[n];
		if(!(ch->ss_rtable[n] = malloc(sizeof(struct _ss_rtable)))){
			perror("Error in malloc");
			exit(EXIT_FAILURE);
		}	
		ch->ss_rtable[n]->rtable_entry = svm->tree[n];
		ch->ss_rtable[n]->timestamp = time(NULL);
		n++;
	}
	ch->table_size = n;

	pthread_rwlock_unlock(&node_lock);
	return 0;
}

int rtable_search(struct channel_entry *ch, struct _adjacent_server *node){
	int n;

	if(!ch || !node)
		return -1;
	n=0;
	while(n < ch->table_size){
		if(node_compare(node, ch->routing_table[n]) == 0)
			return 0;
		n++;
	}
	
	return -1;
}

int rtable_add(struct channel_entry *ch, struct _adjacent_server *node){
	struct _ss_rtable *rt;	

	if(!ch || !node)
		return -1;

	if(ch->table_size >= TREE_MAX-1)
		return -1;

	if(!(rt = malloc(sizeof(struct _ss_rtable)))){
		perror("Error in malloc");
		exit(EXIT_FAILURE);
	}
	rt->rtable_entry = node;
	rt->timestamp = time(NULL);
	//snprintf(LOG_SEND, LOGMSG_LEN, "adding %s:%s to channel %s.", node->ipaddr, node->port_str, ch->channel_name);
	//log_send();
	pthread_rwlock_wrlock(&channel_lock);
	ch->routing_table[ch->table_size] = node;
	ch->ss_rtable[ch->table_size] = rt;
	ch->table_size++;

	pthread_rwlock_unlock(&channel_lock);
	return 0;
}

int rtable_prune(struct channel_entry *ch, struct _adjacent_server *node, struct _server_manager *svm){
	struct _adjacent_server *node2;
	int n;

	if(!ch || !node || !svm){
		error_msg("rtable_prune received null argument.");
		return -1;
	}

	n=0;
	pthread_rwlock_wrlock(&channel_lock);
	while(n < ch->table_size){
		node2 = ch->routing_table[n];
		if(node2->serveraddr->sin_addr.s_addr == node->serveraddr->sin_addr.s_addr){
			if(node2->serveraddr->sin_port == node->serveraddr->sin_port){
				goto RM_NODE;
			}
		}
		n++;
	}

	pthread_rwlock_unlock(&channel_lock);
	return -1;

	RM_NODE:
		free(ch->ss_rtable[n]);
		//snprintf(LOG_SEND, LOGMSG_LEN, "removing %s:%s from channel %s.", node2->ipaddr, node2->port_str, ch->channel_name);
		//log_send();
		while(n < ch->table_size){
			ch->routing_table[n] = ch->routing_table[n+1];
			ch->ss_rtable[n] = ch->ss_rtable[n+1];
			n++;
		}
		if(ch->table_size < TREE_MAX){
			memset(&ch->routing_table[n], 0, (TREE_MAX-n)*sizeof(struct _adjacent_server*));
			memset(&ch->ss_rtable[n], 0, (TREE_MAX-n)*sizeof(struct _ss_rtable *));	
		}
		if(ch->table_size > 0)
			ch->table_size--;

		pthread_rwlock_unlock(&channel_lock);	
		return 0;
}

int node_keepalive(struct channel_entry *ch, struct _adjacent_server *node){
	struct _adjacent_server *node2;
	int n;

	if(!ch || !node){
		error_msg("node_keepalive received null argument.");
		return -1;
	}

	n=0;
	pthread_rwlock_rdlock(&node_lock);
	pthread_rwlock_rdlock(&channel_lock);
	while(n < ch->table_size){
		if(!(node2 = ch->routing_table[n]))
			continue;

		if(node2->serveraddr->sin_addr.s_addr == node->serveraddr->sin_addr.s_addr){
			if(node2->serveraddr->sin_port == node->serveraddr->sin_port){
				ch->ss_rtable[n]->timestamp = time(NULL);
				pthread_rwlock_unlock(&channel_lock);
				pthread_rwlock_unlock(&node_lock);
				return 0;
			}
		}
		n++;
	}

	pthread_rwlock_unlock(&channel_lock);
	pthread_rwlock_unlock(&node_lock);
	return -1;
}

int channel_softstate(struct _channel_manager *chm){
	char log_msg[LOGMSG_LEN];
	struct channel_entry *ch;	
	time_t current_time;
	int n, i;

	if(!chm){
		error_msg("channel_softstate received null argument.");
		return -1;
	}

	current_time = time(NULL);

	pthread_rwlock_rdlock(&channel_lock);
	ch = chm->list_head;
	while(ch){
		i=0;	
		while(i < ch->table_size){
			if((current_time - ch->ss_rtable[i]->timestamp) >= SS_TIMEOUT){
				n = i;
				snprintf(log_msg, LOGMSG_LEN, "Timeout on channel %s from %s:%s", ch->channel_name, ch->routing_table[n]->ipaddr, ch->routing_table[n]->port_str);
				log_thread(log_msg);	
				free(ch->ss_rtable[n]);
				while(n < ch->table_size){
					ch->routing_table[n] = ch->routing_table[n+1];
					ch->ss_rtable[n] = ch->ss_rtable[n+1];
					n++;
				}
				if(ch->table_size < TREE_MAX){
					ch->routing_table[TREE_MAX-1] = NULL;
					ch->ss_rtable[TREE_MAX-1] = NULL;
				}
				if(ch->table_size > 0)
				        ch->table_size--;	
			}
			i++;
		}
		ch = ch->next;	
	}

	pthread_rwlock_unlock(&channel_lock);
	return 0;
}

int resubscribe(struct _channel_manager *chm, int sockfd){
	char log_msg[LOGMSG_LEN];
	struct _adjacent_server *node;
	struct channel_entry *ch;
	struct _S2S_join *join;
	int n;

	if(!chm){
		error_msg("resubscribe received null chm.");
		return -1;
	}

	if(!(join = malloc(sizeof(struct _S2S_join)))){
		perror("Error in malloc");
		exit(EXIT_FAILURE);
	}
	memset(join, 0, sizeof(struct _S2S_join));
	join->type_id = S2S_JOIN;

	pthread_rwlock_rdlock(&node_lock);
	pthread_rwlock_rdlock(&channel_lock);
	ch = chm->list_head;
	while(ch){
		memcpy(join->channel, ch->channel_name, NAME_LEN);	
		for(n=0;n<ch->table_size;n++){
			node = ch->routing_table[n];	
			snprintf(log_msg, LOGMSG_LEN, "%s:%s\tsend S2S Join %s (resubscription)", node->ipaddr, node->port_str, ch->channel_name);
			log_thread(log_msg);
			if(sendto(sockfd, join, sizeof(struct _S2S_join), 0, (struct sockaddr *)node->serveraddr, sizeof(struct sockaddr)) < 0)
				perror("Error in sendto (resubscription)");
		}	
		ch = ch->next;
	}

	pthread_rwlock_unlock(&node_lock);
	pthread_rwlock_unlock(&channel_lock);
	return 0;
}

int propogate_join(struct channel_entry *ch, struct _adjacent_server *sender, int sockfd, struct _server_manager *svm){
	char log_msg[LOGMSG_LEN];
	struct _adjacent_server *node;
	struct _S2S_join *join;
	int n, i;

	if(!ch || !svm){
		error_msg("propogate_join received null argument.");
		return -1;
	}
	if(!(join = malloc(sizeof(struct _S2S_join)))){
		perror("Error in malloc");
		exit(EXIT_FAILURE);
	}

	memset(join, 0, sizeof(struct _S2S_join));
	join->type_id = S2S_JOIN;

	pthread_rwlock_rdlock(&node_lock);
	memcpy(join->channel, ch->channel_name, NAME_LEN);
	for(n=0; n < svm->tree_size; n++){
		node = svm->tree[n];
		/*Don't send join request to source server.*/
		if(node_compare(node, sender) != 0){	
			snprintf(log_msg, LOGMSG_LEN, "%s:%s\tsend S2S Join %s", node->ipaddr, node->port_str, ch->channel_name);
			log_thread(log_msg);	
			i = sendto(sockfd, join, sizeof(struct _S2S_join), 0, (struct sockaddr *)node->serveraddr, sizeof(struct sockaddr));
			if(i < 0)
				perror("Error in sendto JOIN");
		}
	}

	pthread_rwlock_unlock(&node_lock);
	free(join);
	return n;
}
/*This storage method for ids is terrible*/
int save_id(unique_t id, struct _server_manager *svm){
	int n, retval;
	unique_t bp[UID_MAX];

	if(svm->num_ids >= UID_MAX-1){
		memset(bp, 0, (sizeof(unique_t)*UID_MAX));      
		memcpy(bp, &svm->recent_ids[(UID_MAX/2)], (sizeof(unique_t)*(UID_MAX/2)));
		memcpy(svm->recent_ids, bp, UID_MAX);
		svm->num_ids = (UID_MAX/2);
	}

	retval = 0;
	for(n=0; n < svm->num_ids; n++){
		if((svm->recent_ids[n] ^ id) == 0)
			return 1;
	}

	svm->recent_ids[n] = id;
	svm->num_ids++;

	return retval;
}

int propogate_say(struct channel_entry *ch, char *name, unique_t id, _sreq_say *req, struct _adjacent_server *sender, int sockfd, struct _server_manager *svm){
	char log_msg[LOGMSG_LEN];
	struct _adjacent_server *node;
	struct _S2S_say *rsp;
	int n;

	if(!req || !ch){
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

	if(sender){
		/*If sender is not null, treat Say as originating from adj server*/
		goto RELAY;
	}else{
		goto PROP;
	}

	PROP:
		/*Propogates a new S2S Say if message orginated from client*/
		if(save_id(generate_id(rsp), svm) < 0){
			error_msg("generated say request with non-unique id.");
			return -1;
		}
		pthread_rwlock_rdlock(&channel_lock);
		for(n=0;n<ch->table_size;n++){
			node = ch->routing_table[n];
			if(node){
				snprintf(log_msg, LOGMSG_LEN, "%s:%s\tsend S2S Say %s %s '%s'", node->ipaddr, node->port_str, name, req->channel, req->text);
				log_thread(log_msg);
				if(sendto(sockfd, rsp, sizeof(struct _S2S_say), 0, (struct sockaddr *)node->serveraddr, sizeof(struct sockaddr)) < 0)
					perror("Error in sendto");
			}
		}

		pthread_rwlock_unlock(&channel_lock);
		free(rsp);
		return n;

	RELAY:
		/*Relays an S2S Say to other servers on channel except for the source server*/
		rsp->msg_id = id;
		pthread_rwlock_rdlock(&channel_lock);	
		for(n=0;n<ch->table_size;n++){
			if(!(node = ch->routing_table[n]))
				continue;
			if(sender->serveraddr->sin_addr.s_addr == node->serveraddr->sin_addr.s_addr){
					if(sender->serveraddr->sin_port == node->serveraddr->sin_port)
						continue;
			}
			snprintf(log_msg, LOGMSG_LEN, "%s:%s\tsend S2S Say %s %s '%s'", node->ipaddr, node->port_str, name, req->channel, req->text);
			log_thread(log_msg);
			if(sendto(sockfd, rsp, sizeof(struct _S2S_say), 0, (struct sockaddr *)node->serveraddr, sizeof(struct sockaddr)) < 0)
				perror("Error in sendto");
		}

		pthread_rwlock_unlock(&channel_lock);
		free(rsp);
		return n;
}

int send_leave(char *ch, struct _adjacent_server *node, int sockfd){
	char log_msg[LOGMSG_LEN];
	struct _S2S_leave req;
	int n;

	if(!ch || !node){
		error_msg("send_leave received null argument.");
		return -1;
	}
	memset(&req, 0, sizeof(struct _S2S_leave));
	req.type_id = S2S_LEAVE;
	pthread_rwlock_rdlock(&node_lock);
	memcpy(req.channel, ch, NAME_LEN);
	snprintf(log_msg, LOGMSG_LEN, "%s:%s\tsend S2S Leave %s", node->ipaddr, node->port_str, ch);
	log_thread(log_msg);
	n = sendto(sockfd, &req, sizeof(struct _S2S_leave), 0, (struct sockaddr *)node->serveraddr, sizeof(struct sockaddr));
	if(n < 0)
		perror("Error in sendto");
	
	pthread_rwlock_unlock(&node_lock);
	return n;
}

