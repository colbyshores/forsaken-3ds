/*
 * net_3ds.c - Network stubs for 3DS (single-player only)
 *
 * Provides no-op implementations of the network API
 * so the game compiles without enet.
 */

#ifdef __3DS__

#include "main.h"
#include "net.h"
#include "util.h"

network_state_t network_state = NETWORK_DISCONNECTED;
network_players_t network_players = {0, NULL, NULL};

network_return_t network_setup(char* player_name, int local_port)
{
	(void)player_name; (void)local_port;
	DebugPrintf("net_3ds: network_setup stub (no networking on 3DS)\n");
	return NETWORK_OK;
}

int network_join(char* address, int port)
{
	(void)address; (void)port;
	return 0;
}

void network_host(void) {}
void network_pump(void) {}
void network_cleanup(void) {}
void network_set_player_name(char* name) { (void)name; }

void network_send(network_player_t* player, void* data, int size, network_flags_t flags, int channel)
{
	(void)player; (void)data; (void)size; (void)flags; (void)channel;
}

void network_broadcast(void* data, int size, network_flags_t flags, int channel)
{
	(void)data; (void)size; (void)flags; (void)channel;
}

#endif /* __3DS__ */
