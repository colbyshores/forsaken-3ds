#pragma once

/* botai_path.h -- bot routes over hand-placed nodes (LUA_BOT only)
 *
 * botai_path.cpp hardcodes world-space node positions for three
 * multiplayer levels (alpha, fourball, ship; any other level uses the
 * ship table) and walks them. This is not the engine's .nod AI node
 * graph. Route direction is re-rolled after each death or kill.
 */

#ifdef LUA_BOT
VECTOR BOTAI_NodeNetwork(int node);
VECTOR BOTAI_NodeNetworkAlpha(int node);
VECTOR BOTAI_NodeNetworkFourball(int node);
VECTOR BOTAI_NodeNetworkShip(int node);

int BOTAI_GetNearestNode(OBJECT * TObj);
int BOTAI_GetNodeGroup(int node);
void BOTAI_FollowNodeNetwork( bool SlideOnly, bool FollowingTarget );
void BOTAI_GetNextNode();
void BOTAI_GetNextNodeAlpha();
void BOTAI_GetNextNodeFourball();
void BOTAI_GetNextNodeShip();
#endif // LUA_BOT
