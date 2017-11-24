# Duckchat v2
<h2>Compilation (64bit only)</h2>
<br>make	(compiles server and client with only necessary flags)
<br>make debug	(compiles server/client with symbols and strict compiler warnings)

<h2>File Organization</h2>
<h3>Top Level:</h3>
<br>duckchat.h: Defines all structs, typedefs, macros used by both server and client (mostly).
<br>server.h/client.h: Defines all structs, typedefs, macros, and global variables used specifically by the server or client respectively.
<br>server.c/client.c: Main logic for server and client.
<h3>server_modules:</h3>
<br>linkedlist.c: Defines functions used by the server to maintain a linked-list structure for connected clients and existing channels. 
Implements standard linked-list opperations, initialization, and clean up. All opperations are thread-safe using pthread_rwlocks to allow 
for simultaneous reading of lists.
<br>servertree.c: Defines functions used in initializing, reading, and modifying the base server topology, as well individual 
channel topologies. Unique identifier generation and storage functions are also defined in this file. Functions operating on a channel's 
routing table are thread-safe.
<br>handle_request.c: Defines one <i>beefy</i> function used by the main thread to handle all received data. If the data is a valid request, it 
is handled on the fly if an S2S, or enqueued for the sender thread to handle and dispatch to appropriate clients.
<h3>client_modules:</h3>
<br>channels.c: Defines all functions related to management of subscribed channels on the client-side. Channels are stored as a doubly linked-list of _channel_sub structs (defined in client.h).
<br>raw.c/raw.h: Defines functions for switching terminal between raw and cooked mode.
