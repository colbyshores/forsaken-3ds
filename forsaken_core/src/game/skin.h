#pragma once

/* skin.h -- debug views of collision skin, portals, BSP nodes and zones
 *
 * Nothing here affects gameplay; it only draws level data that is otherwise
 * invisible. visi.cpp calls DisplayBSPNode, CreatePortalExecList and
 * CreateSkinExecList once per camera pass, and oct2.cpp draws the resulting
 * Skin_Execs / Portal_Execs vertex buffers when one of the Show* /
 * NodeCube switches is on (debug keys and debug menu toggles). Zone
 * owners (trigarea, extforce, teleport, compobjects for background-object
 * collision zones) call the Display* functions per visible group for their
 * own volumes.
 *
 * The buffers are rebuilt every pass: DisplayBSPNode releases the skin
 * buffers, the debug lines (NodeCubeLines, the line box) and the
 * sphere-zone models, so it must run before anything else adds to them that
 * pass. Each face of a hull volume (BSP node, trigger or collision zone) is
 * a huge quad in that side's plane clipped by the other sides
 * (Gimme*Vertices), drawn as a filled polygon plus white edge lines; sphere
 * zones and direction markers borrow entries from Models[].
 *
 * MakeNewSkinExec / MakeNewPortalExec copy vertices only and ignore the
 * triangle list, so the vertex array is drawn as it is laid out. ShowSkin is
 * never set above 0, so CreateSkinExecList currently returns at once.
 */

/*===================================================================
	Includes
===================================================================*/
#include <math.h>

#include "math/new3d.h"
#include "game/ships.h"
#include "data/mload.h"
#include "world/collision.h"
#include "rendering/lights.h"
#include "rendering/2dtextures.h"
#include "rendering/2dpolys.h"
#include "data/tload.h"
#include "world/bsp.h"
#include "game/trigarea.h"

/*===================================================================
	Defines
===================================================================*/
constexpr auto MAXSKINEXECS = MAXGROUPS;
constexpr auto MAXPORTALEXECS = MAXGROUPS;
constexpr int MAX_TRIS_PER_EXEC = 300;

/*===================================================================
	Prototypes
===================================================================*/
bool CreateSkinExecList( MCLOADHEADER * MCloadheader, int16_t NumVisible );
bool MakeNewSkinExec( LVERTEX *Verts, TRIANGLE *Tris, int16_t	NumVerts, int16_t NumTris );
void ReleaseSkinExecs( void );
void DisplayBSPNode( BSP_NODE * Node );
void DisplayNewBSPNode( BSP_NODE * Node );
int PlaneToCubeIntersectionShape( VECTOR * min_ptr, VECTOR * max_ptr, VECTOR * vec_ptr, float offset, VECTOR * result_ptr );
void CreateLineBox( void );
int GimmeNodeVertices( BSP_NODE * plane_ptr, BSP_NODE * space_ptr, int side, VECTOR * out_ptr );
void KillNodeCubeLines( void );
void KillBoxLines( void );
void InitSkinExecs( void );
bool MakeNewPortalExec( LVERTEX *Verts, TRIANGLE *Tris, int16_t NumVerts, int16_t NumTris );
bool CreatePortalExecList( MLOADHEADER * Mloadheader, int16_t NumVisible );
void ReleasePortalExecs( void );
void InitPortalExecs( void );
int GimmeTriggerZoneVertices( TRIGGER_ZONE * plane_ptr, TRIGGER_ZONE * space_ptr, int NumSides, int side, VECTOR * out_ptr );
int GimmeColZoneVertices( ZONESIDE * plane_ptr, ZONESIDE * space_ptr, int NumSides, int side, VECTOR * out_ptr );
void DisplayTriggerZone( TRIGGER_ZONE * Sides, int16_t NumSides, u_int16_t Group,
						 u_int8_t Red, u_int8_t Green, u_int8_t Blue, u_int8_t Trans );
void DisplayColZone( ZONESIDE * Sides, int16_t NumSides, u_int16_t Group, int16_t Sensitive );
void DisplaySphereZone( VECTOR * Pos, float Radius, u_int16_t Group,
					   u_int8_t Red, u_int8_t Green, u_int8_t Blue, u_int8_t Trans );
void KillAllSphereZones( void );
void DisplayZoneDirection( VECTOR * Pos, VECTOR * Dir, u_int16_t Group );
/* Defined in skin.cpp; shared with the modules that use it. */
extern bool ShowEFZones;
extern RENDEROBJECT Portal_Execs[ MAXPORTALEXECS ];
extern RENDEROBJECT Skin_Execs[ MAXSKINEXECS ];
extern bool ShowTeleports;
extern bool ShowTrigZones;
extern int16_t ShowPortal;
extern int16_t ShowSkin;
void DisplayDirAndUp( VECTOR * Pos, VECTOR * Dir, VECTOR *Up, u_int16_t Group );
