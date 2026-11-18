#pragma once

/* net_enet_2.h -- enet backend of net.h (PC); state shared with the tracker
 *
 * net_enet_2.cpp (built with NET_ENET_2) implements net.h over one enet host
 * on UDP. A session is a full mesh: every player holds a connection to every
 * other. The host gives each newcomer an 8-bit id (YOUR_ID), sends it the
 * NAT_PUNCH_THROUGH_LIST (the joiner fires empty UDP packets at the existing
 * players to open its NAT), and once the joiner acknowledges its id tells
 * everyone to connect to it (CONNECT, on the port the joiner advertised with
 * CONNECT_PORT). Only when every player has reported CONNECTED_TO the joiner
 * does the host announce it with NEW_PLAYER; a joiner that some player cannot
 * reach is dropped (FAILED_SYNCH). This handshake runs on enet channel 0;
 * game packets on the other channels reach network_event only from peers
 * that finished it. When the host leaves, the player with the lowest id
 * becomes host.
 *
 * The globals below are what net_tracker.cpp needs, since it sends from
 * enet_host's socket. The 3DS build has no networking (net_3ds.cpp stubs
 * net.h).
 */

#include "enet/enet.h"
#include "net/net.h"


extern ENetHost * enet_host;							/* the local enet host */
extern char my_player_name[ NETWORK_MAX_NAME_LENGTH ];	/* advertised to the tracker */
extern int my_local_port;								/* port enet_host listens on */
