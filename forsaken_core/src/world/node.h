#pragma once

/* node.h -- AI navigation node network (.NOD) and route queries
 *
 * node.cpp loads one NODE array per level (Nodeload / NodeRelease). A missing
 * file is not an error: NodeNetworkHeader.State stays false and every query
 * returns early. NetMask holds the networks a node belongs to; an object only
 * uses nodes that share a bit with its NodeNetwork.
 *
 * Pos is the authored position; SolidPos is snapped at load to the floor
 * below plus 75 world units. Routing uses an N x N shortest-path distance
 * table built at load over all links regardless of network; the mask is only
 * applied when choosing the next hop. Links in the file beyond
 * MAXLINKSPERNODE are skipped.
 */

#include "game/main.h"
#include "game/object.h"

/* Bumped 16 -> 32 to fit Remaster nodes that exceed the 1998 cap.
 * starship has nodes with 17-18 links per the runtime trace; 32
 * gives headroom for other Remaster levels with wider connectivity.
 * Memory cost: per-NODE struct grows by (32-16) × sizeof(struct NODE*)
 * = 64 bytes × number of nodes per level (typically 100-300) =
 * ~6-20 KB extra heap per level. Negligible. */
constexpr int MAXLINKSPERNODE = 32;

constexpr auto NODE_DECISION = (1<<0);
constexpr auto NODE_DROPMINES = (1<<1);
constexpr auto NODE_SNIPE_POSITION = (1<<2);
constexpr auto NODE_HIDE_POSITION = (1<<3);
constexpr auto NODE_TERMINATE = (1<<4);

typedef struct NODE
{
	bool	LegalGroup;
	u_int16_t	Flags;
	int16_t	Group;
	VECTOR	Pos;
	VECTOR	SolidPos;
	float	Radius;
	u_int16_t	NodeNum;		// what number in the array I am..
	u_int32_t	NetMask;		// which networks am I in..
	int16_t	NumOfLinks;
struct	NODE *	NodeLink[MAXLINKSPERNODE];
struct	NODE *	NextNodeInGroup;
} NODE,*LPNODE;

typedef struct _NODENETWORKHEADER
{
	bool	State;
	int32_t	NumOfNodes;
	NODE *	FirstNode;
} NODENETWORKHEADER,*LPNODENETWORKHEADER;


void NodeRelease(void);
bool Nodeload( char * Filename );

void FindNearestNode( OBJECT * Object );
void UpdateNearestNode( OBJECT * Object );
void UpdateNearestNodeSpecial( OBJECT * Object );
NODE * WhichNode( u_int32_t Network , NODE * NodeFrom , NODE * NodeTo );
NODE * WhichRetreatNode( u_int32_t Network , NODE * NodeFrom , NODE * NodeTo );

NODE * ChooseAlternateNode( u_int32_t Network , NODE * NodeFrom , NODE * NodeTo );

NODE * FindSuitableSplineNode( u_int32_t Network, NODE * NodeFrom , NODE *Node1 , NODE *Node2 , NODE *Node3 , NODE *Node4 );
NODE * FindSuitableSplineNodeRandom( u_int32_t Network, NODE * NodeFrom , NODE *Node1 , NODE *Node2 , NODE *Node3 , NODE *Node4 );
float ReturnClosestNode( VECTOR * Pos, VECTOR * NearestNodePos, u_int16_t * Group, VECTOR * TopLeft, VECTOR * BottomRight );

/* Defined in node.cpp; shared with the modules that use it. */
extern NODENETWORKHEADER NodeNetworkHeader;
void ShowNodeNetwork( u_int32_t NetMask );
