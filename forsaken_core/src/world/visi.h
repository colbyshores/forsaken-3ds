#pragma once

/* visi.h -- portal visibility: per-camera visible groups and group relations
 *
 * visi.cpp reads the Connected / Visible / IndirectVisible group tables (bitmaps
 * plus per-group lists sorted ascending) from the level .mxv at load; they are
 * per level. FindVisible walks the portal tree from the camera's group once
 * per camera pass, filling cam->visible and the GroupsVisible / IsGroupVisible
 * globals; ClipGroup then sets projection and viewport before a group is drawn
 * (full-screen citro3d cameras use a scissor instead of a sub-rect viewport).
 *
 * GroupsAreVisible and VisibleOverlap answer from the precomputed tables, not
 * from the current frame. The list merges rely on the ascending sort.
 */

#include <math.h>
#include "math/new3d.h"
#include "data/mload.h"
#include "game/ships.h"
#include "rendering/lights.h"
#include "game/skin.h"
#include "game/camera.h"

class FileReader;

struct GROUPLIST
{
	int groups;
	u_int16_t *group;
};

// indirectly visible groups seen by any active camera
extern u_int16_t Num_IndirectVisible;
extern u_int16_t IndirectVisible[ MAXGROUPS ];

/*
 * fn prototypes
 */
void FindVisible( CAMERA *cam, MLOADHEADER	* Mloadheader );
int ClipGroup( CAMERA *cam, u_int16_t group );
bool DisplayBackground( MLOADHEADER	* Mloadheader, CAMERA *cam );
void InitVisiStats( MLOADHEADER *m );
bool OutputVisiStats( MLOADHEADER *m, char *lname );

void FreeGroupConnections( void );
GROUPLIST *VisibleGroups( u_int16_t g );
int VisibleOverlap( u_int16_t g1, u_int16_t g2, u_int16_t *overlapping_group );
bool GroupsAreVisible( u_int16_t g1, u_int16_t g2 );
bool GroupsAreConnected( u_int16_t g1, u_int16_t g2 );

u_int16_t FindClipGroup( CAMERA *cam, MLOADHEADER *m, VECTOR *min, VECTOR *max );
bool PointInGroupBoundingBox( MLOADHEADER * Mloadheader, VECTOR * Pos, u_int16_t group );

/* Defined in visi.cpp; shared with the modules that use it. */
void InitIndirectVisible( u_int16_t g );
void AddIndirectVisible( u_int16_t g );
bool ReadGroupConnections( MLOADHEADER *m, FileReader & in );
extern u_int16_t IsGroupVisible[ MAXGROUPS ];
extern int NumOfVertsTouched;
extern u_int16_t NumGroupsVisible;
extern u_int16_t GroupImIn;
extern u_int16_t GroupsVisible[ MAXGROUPS ];
