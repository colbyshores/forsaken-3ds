
#ifndef VISI_INCLUDED
#define VISI_INCLUDED


#include <math.h>
#include "math/new3d.h"
#include "data/mload.h"
#include "game/ships.h"
#include "rendering/lights.h"
#include "game/skin.h"
#include "game/camera.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct _GROUPLIST
{
	int groups;
	u_int16_t *group;
} GROUPLIST;

// indirectly visible groups seen by any active camera
extern u_int16_t Num_IndirectVisible;
extern u_int16_t IndirectVisible[ MAXGROUPS ];

/*
 * fn prototypes
 */
bool LightGroupDispGroup( MLOADHEADER	* Mloadheader);

bool DispVisipolyModel( MLOADHEADER	* Mloadheader) ;
void FindGroupImIn( MLOADHEADER	* Mloadheader);
u_int16_t WhichGroupImIn( MLOADHEADER	* Mloadheader , VECTOR * Pos );
bool PortalTestTransformClip( MLOADHEADER * Mloadheader , u_int16_t group );
void FindVisiblePortals( MLOADHEADER * Mloadheader , u_int16_t group );
void BuildGroupList ( MLOADHEADER * Mloadheader , u_int16_t group );
void FindVisible( CAMERA *cam, MLOADHEADER	* Mloadheader );
int ClipGroup( CAMERA *cam, u_int16_t group );
bool DisplayBackground( MLOADHEADER	* Mloadheader, CAMERA *cam );
void InitVisiStats( MLOADHEADER *m );
bool OutputVisiStats( MLOADHEADER *m, char *lname );

bool FindGroupConnections( MLOADHEADER *m );
void FreeGroupConnections( void );
GROUPLIST *ConnectedGroups( u_int16_t g );
GROUPLIST *VisibleGroups( u_int16_t g );
GROUPLIST *IndirectVisibleGroups( u_int16_t g );
int VisibleOverlap( u_int16_t g1, u_int16_t g2, u_int16_t *overlapping_group );
bool GroupsAreVisible( u_int16_t g1, u_int16_t g2 );
bool GroupsAreConnected( u_int16_t g1, u_int16_t g2 );
bool GroupsAreIndirectVisible( u_int16_t g1, u_int16_t g2 );

u_int16_t FindClipGroup( CAMERA *cam, MLOADHEADER *m, VECTOR *min, VECTOR *max );
u_int16_t FindOverlappingVisibleGroups( CAMERA *cam, MLOADHEADER *m, VECTOR *min, VECTOR *max, u_int16_t * group );
bool PointInGroupBoundingBox( MLOADHEADER * Mloadheader, VECTOR * Pos, u_int16_t group );

/* Defined in visi.cpp; shared with the modules that use it. */
bool ReadGroupConnections( MLOADHEADER *m, char **pbuf );
extern u_int16_t IsGroupVisible[ MAXGROUPS ];
extern int NumOfVertsConsidered;
extern int NumOfVertsTouched;
extern u_int16_t NumGroupsVisible;
extern u_int16_t GroupImIn;
extern u_int16_t GroupsVisible[ MAXGROUPS ];

#ifdef __cplusplus
}
#endif
#endif	// VISI_INCLUDED
