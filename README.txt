> Duckchat v2

	[*] Compilation (64bit only):

	make		(compiles server and client with only necessary flags) 
	make debug	(compiles server/client with symbols and strict compiler warnings)

> Usage:

	[*] Server:

	Single Server:	./server [IP or Domain of host machine] [Port#] (e.g. ./server localhost 4000) 
	Multiserver:	Edit start_server.sh file for desired server topology. Currently set to run capital H-shaped topology.

	[*] Client:

	Start:		./client [IP or Domain of target server] [Port#] [Username] (e.g. ./client localhost 4000 User1) 
	Commands:
	/exit:		Logout the user and exit the client software.
	/join channel:	Join (subscribe in) the named channel, creating the channel if it does not exist.
	/leave channel: Leave the named channel.
	/list: 		List the names of all channels on connected server (S2S List not implement).
	/who channel:	List the users who are on the named channel on the connected server (S2S Who not implemented).
	/switch channel: Switch to an existing named channel that user has already joined.

	[*] Limits (Default config):
	*max number of clients per channel and adjacent servers can safely be modified if needed.

	Max number of clients per channel:			128	(Defined in duckchat.h as MAX_CHANNELCLIENTS) 
	Max number of adjacent servers (to any one server):	16 	(Defined in server.h as TREE_MAX) 
	Max message length:					64
	Max username and channel name length:			32

> File Organization

	[*] Top Level:

	duckchat.h:		Defines all structs, typedefs, macros used by both server and client (mostly). 
	server.h/client.h:	Defines all structs, typedefs, macros, and global variables used specifically by the server or client respectively. 
	server.c/client.c:	Main logic for server and client.

	[*] server_modules:

	linkedlist.c:		Defines functions used by the server to maintain a linked-list structure for connected clients and existing channels. Implements standard 
				linked-list opperations, initialization, and clean up. All opperations are thread-safe using pthread_rwlocks to allow for simultaneous reading of lists.

	servertree.c:		Defines functions used in initializing, reading, and modifying the base server topology, as well individual channel topologies. 
				Unique identifier generation and storage functions are also defined in this file. Functions operating on a channel's routing table are thread-safe.

	handle_request.c: 	Defines one beefy function used by the main thread to handle all received data. If the data is a valid request, 
				it is handled on the fly if an S2S, or enqueued for the sender thread to handle and dispatch to appropriate clients.

	[*] client_modules:

	channels.c:		Defines all functions related to management of subscribed channels on the client-side. 
				Channels are stored as a doubly linked-list of _channel_sub structs (defined in client.h).

	raw.c/raw.h:		Defines functions for switching terminal between raw and cooked mode.


