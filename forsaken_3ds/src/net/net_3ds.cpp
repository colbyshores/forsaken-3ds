/* net_3ds.cpp -- net.h for the 3DS: no networking
 *
 * The 3DS build has no enet and no network backend, so every net.h call is a
 * no-op: network_setup reports NETWORK_OK, network_join returns 0 (failed),
 * network_state stays NETWORK_DISCONNECTED, network_players stays empty and
 * network_event is never called. networking.cpp is still compiled and runs
 * against these stubs. The tracker calls are stubbed in stubs_3ds.cpp.
 */

#ifdef __3DS__

#include "game/main.h"
#include "net/net.h"
#include "util/util.h"

network_state_t network_state = NETWORK_DISCONNECTED;
network_players_t network_players = {0, nullptr, nullptr};

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
