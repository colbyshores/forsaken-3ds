#pragma once

/*===================================================================
	net_enet_2 -- enet backend state shared with the master-server
	tracker (net_tracker.cpp). Defined in net_enet_2.cpp; PC only:
	the 3DS build has no networking (net_3ds.cpp stubs net.h).
===================================================================*/

#include "enet/enet.h"
#include "net/net.h"


extern ENetHost * enet_host;							/* the local enet host */
extern char my_player_name[ NETWORK_MAX_NAME_LENGTH ];	/* advertised to the tracker */
extern int my_local_port;								/* port enet_host listens on */

