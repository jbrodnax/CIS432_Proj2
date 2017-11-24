# Duckchat v2
<h2>Compilation (64bit only)</h2>
<br>make	(compiles server and client with only necessary flags)
<br>make debug	(compiles server/client with symbols and strict compiler warnings)

<h2>Usage</h2>
<h3>Server:</h3>
<b>Single Server:</b> ./server [IP or Domain of host machine] [Port#] (e.g. ./server localhost 4000)
<br><b>Multiserver:</b> Edit start_server.sh file for desired server topology. Currently set to run capital H-shaped topology.

<h3>Client:</h3>
<b>Start:</b> ./client [IP or Domain of target server] [Port#] [Username] (e.g. ./client localhost 4000 User1)
<br><b>Commands: (*taken from proj1 description)</b>
<li><tt><b>/exit:</b></tt> Logout the user and exit the client software.</li>
<li><tt><b>/join <i>channel</i></b></tt>: Join (subscribe in) the named channel, creating the channel if it does not exist.</li>
<li><tt><b>/leave <i>channel</i></b></tt>: Leave the named channel.</li>
<li><tt><b>/list</b></tt>: List the names of all channels.</li>
<li><tt><b>/who <i>channel</i></b></tt>: List the users who are on the named channel.</li>
<li><tt><b>/switch <i>channel</i></b></tt>: Switch to an existing named channel that user has already joined.</li>

<h3>Limits (Default config):</h3>
<i>*max number of clients per channel and adjacent servers can safely be modified if needed.</i>
<br><b>Max number of clients per channel:</b> 128 (Defined in <i>duckchat.h</i> as MAX_CHANNELCLIENTS)
<br><b>Max number of adjacent servers (to any one server):</b> 16 (Defined in <i>server.h</i> as TREE_MAX)
<br><b>Max message length:</b> 64 characters
<br><b>Max username and channel name length:</b> 32 characters

<h2>File Organization</h2>
<h3>Top Level:</h3>
<b>duckchat.h:</b> Defines all structs, typedefs, macros used by both server and client (mostly).
<br><b>server.h/client.h:</b> Defines all structs, typedefs, macros, and global variables used specifically by the server or client respectively.
<br><b>server.c/client.c:</b> Main logic for server and client.

<h3>server_modules:</h3>
<b>linkedlist.c:</b> Defines functions used by the server to maintain a linked-list structure for connected clients and existing channels. 
Implements standard linked-list opperations, initialization, and clean up. All opperations are thread-safe using pthread_rwlocks to allow 
for simultaneous reading of lists.
<br><b>servertree.c:</b> Defines functions used in initializing, reading, and modifying the base server topology, as well individual channel topologies. Unique identifier generation and storage functions are also defined in this file. Functions operating on a channel's routing table are thread-safe.
<br><b>handle_request.c:</b> Defines one <i>beefy</i> function used by the main thread to handle all received data. If the data is a valid request, it is handled on the fly if an S2S, or enqueued for the sender thread to handle and dispatch to appropriate clients.

<h3>client_modules:</h3>
<b>channels.c:</b> Defines all functions related to management of subscribed channels on the client-side. Channels are stored as a doubly linked-list of _channel_sub structs (defined in client.h).
<b>raw.c/raw.h:</b> Defines functions for switching terminal between raw and cooked mode.
