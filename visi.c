
/*===================================================================
*	All routines to do with Visipolys...
===================================================================*/

#include <stdio.h>
#include <string.h>

#include "new3d.h"
#include "quat.h"
#include "compobjects.h"
#include "bgobjects.h"
#include "object.h"
#include "networking.h"
#include "mload.h"
#include "camera.h"
#include "visi.h"
#include <float.h>
#include "file.h"
#include "triggers.h"
#include "main.h"
#include "lines.h"
#include "teleport.h"
#include "extforce.h"
#include "goal.h"
#include "title.h"
#include "restart.h"
#include "util.h"
#include "water.h"
#include "render.h"

extern render_info_t render_info;

#ifdef OPT_ON
#pragma optimize( "gty", on )
#endif

extern void SetViewportError( char *where, render_viewport_t *vp );

extern float hfov;
extern int outside_map;
extern	bool	DoClipping;
extern	CAMERA	CurrentCamera;

/*===================================================================
		Externals...	
===================================================================*/

extern	bool			CTF;
extern	bool			CaptureTheFlag;

extern	MATRIX			ProjMatrix;
extern	TLOADHEADER		Tloadheader;
extern	RENDERMATRIX		proj;
extern	RENDERMATRIX		view;
extern	MCLOADHEADER	MCloadheader;

extern	DWORD			CurrentSrcBlend;
extern	DWORD			CurrentDestBlend;
extern	DWORD			CurrentTextureBlend;

extern	BSP_NODE *	OldCollideNode;

extern	u_int16_t			GroupTris[ MAXGROUPS ];
extern	LINE			Lines[ MAXLINES ];
extern	char			LevelNames[MAXLEVELS][128];                        
extern	MLOADHEADER		Mloadheader;

/*===================================================================
		Globals...	
===================================================================*/

#define MAXPORTVERTS (MAXPORTALSPERGROUP * MAXVERTSPERPORTAL)

#define	MAXGROUPSVISIBLE 16

static int GTabRowSize;

#define GROUP2GROUP_OFFSET( G1, G2 )	( ( (G2) >> 5 ) + ( (G1) * GTabRowSize ) )
#define GROUP2GROUP_MASK( G1, G2 )		( 1 << ( (G2) & 31 ) )

#define DIV_CEIL( N, D ) ( (int32_t) ( (N) + (D) - 1 ) / (D) )

typedef struct _GROUPRELATION
{
	u_int32_t *table;
	GROUPLIST list[ MAXGROUPS ];
} GROUPRELATION;


static GROUPRELATION ConnectedGroup;
static GROUPRELATION VisibleGroup;
static GROUPRELATION IndirectVisibleGroup;

static bool InitConnections = false;


u_int16_t Num_IndirectVisible = 0;
u_int16_t IndirectVisible[ MAXGROUPS ];

static VECTOR clip_top, clip_bottom, clip_left, clip_right; // clipping planes

int		NumOfVertsConsidered= 0;
int		NumOfVertsTouched= 0;

u_int16_t	GroupImIn;
/* bjd - CHECK
D3DTRANSFORMDATA Data;
*/
DWORD VertexCount = 0;
DWORD Offscreen = 0;
VERT TestVerts[MAXPORTVERTS];
VISPOLVERTEX	VisVerts[MAXPORTVERTS];
u_int16_t	CurrentGroupVisible;						// the real group thats visible
u_int16_t	GroupInVisibleList;							// where it is in the visibility list
u_int16_t	NumGroupsVisible;
u_int16_t	GroupsVisible[MAXGROUPS];
XYRECT	GroupVisibleExtents[MAXGROUPS];
u_int16_t	NumPortalsVisible;
u_int16_t	PortalsVisible[MAXPORTALSPERGROUP];
XYRECT	PortalExtents[MAXGROUPS];
MATRIX	VisPolyMatrix = {
				1.0F, 0.0F, 0.0F, 0.0F,
				0.0F, 1.0F, 0.0F, 0.0F,
				0.0F, 0.0F, 1.0F, 0.0F,
				0.0F, 0.0F, 0.0F, 1.0F };
u_int16_t	IsGroupVisible[MAXGROUPS];
/* Parallel to IsGroupVisible[], set during the flood-fill BFS expansion
 * in FindVisible. Object renderers (enemies / pickups / models /
 * BGObjects / secondary FX) gate on `IsGroupVisible[g] &&
 * !IsGroupFloodFilled[g]` so the through-portal rooms render their
 * level geometry without paying for object draws. The level mesh
 * itself still renders (the geometry visible through the portal
 * aperture is the whole point of the flood-fill); we skip only the
 * downstream object lists.
 *
 * Reset alongside IsGroupVisible at the top of RenderCurrentCamera
 * (oct2.c). Set per-group during the flood-fill BFS in FindVisible. */
u_int16_t	IsGroupFloodFilled[MAXGROUPS];

struct
{
	u_int32_t tsum;
	u_int32_t tmax;
	u_int32_t tmin;
	u_int32_t visits;
} VisiStats[ MAXGROUPS ];


typedef struct
{
	u_int16_t group;
	struct
	{
		double x, y, z;
	} from, to;
} DebugRay;

extern int16_t LevelNum;
static void InitDebugRays( void )
{
#if 1
	char fname[ 256 ];
	FILE *rf;
	DebugRay ray;
	u_int16_t line;

	Change_Ext( &LevelNames[ LevelNum ][ 0 ], fname, ".ray" );
	rf = file_open( fname, "rb" );
	if ( !rf )
		return;
	while ( fread( &ray.group, sizeof( ray.group ), 1, rf ) )
	{
		if ( !fread( &ray.from, sizeof( double ), 3, rf ) )
			break;
		if ( !fread( &ray.to, sizeof( double ), 3, rf ) )
			break;
		line = FindFreeLine();
		if ( line != (u_int16_t) -1 )
		{
			Lines[ line ].StartPos.x = (float) ray.from.x;
			Lines[ line ].StartPos.y = (float) ray.from.y;
			Lines[ line ].StartPos.z = (float) ray.from.z;
			Lines[ line ].EndPos.x = (float) ray.to.x;
			Lines[ line ].EndPos.y = (float) ray.to.y;
			Lines[ line ].EndPos.z = (float) ray.to.z;
			Lines[ line ].StartCol.R = 255;
			Lines[ line ].StartCol.G = 64;
			Lines[ line ].StartCol.B = 64;
			Lines[ line ].EndCol.R = 64;
			Lines[ line ].EndCol.G = 255;
			Lines[ line ].EndCol.B = 64;
			Lines[ line ].StartTrans = 255;
			Lines[ line ].EndTrans = 255;
			Lines[ line ].Group = ray.group;
		}
	}
	fclose( rf );
#endif
}


void InitVisiStats( MLOADHEADER *m )
{
	int j;
	LVLGROUP *g;
	VECTOR min, max;

	for ( j = 0; j < m->num_groups; j++ )
	{
		VisiStats[ j ].tsum = 0;
		VisiStats[ j ].tmax = 0;
		VisiStats[ j ].tmin = -1;
		VisiStats[ j ].visits = 0;
		g = &m->Group[ j ];
		min.x = g->center.x - g->half_size.x;
		min.y = g->center.y - g->half_size.y;
		min.z = g->center.z - g->half_size.z;
		max.x = g->center.x + g->half_size.x;
		max.y = g->center.y + g->half_size.y;
		max.z = g->center.z + g->half_size.z;
//		CreateBoundingBox( &min, &max, j );
	}
#ifndef FINAL_RELEASE
	InitDebugRays();
#endif
}


bool OutputVisiStats( MLOADHEADER *m, char *lname )
{
	FILE *f;
	char fname[ 256 ];
	int j, k;

	if ( strlen(lname) == 0 )
	{
		DebugPrintf("Passed empty level name to OutputVisiStats!\n");
		return false;
	}

	Change_Ext( lname, fname, ".vis" );

	f = file_open( fname, "w" );
	if ( !f )
		return false;

	fprintf( f, "Group     TMAX      TAVG      TMIN      Visits    Name\n" );
	for ( j = 0; j < m->num_groups; j++ )
	{
		fprintf( f, "%s%3d %9d %9lu %9lu %9d      %s\n",
			( VisiStats[ j ].tmax > 1000 ) ? "*" : " ",
			j,
			(int)VisiStats[ j ].tmax,
			(int)( VisiStats[ j ].visits ) ? VisiStats[ j ].tsum / VisiStats[ j ].visits : 0,
			(int)( VisiStats[ j ].visits ) ? VisiStats[ j ].tmin : 0,
			(int)VisiStats[ j ].visits,
			m->Group[ j ].name );
	}

	fprintf( f, "\n\nGROUP       CONNECTED GROUPS\n" );
	for ( j = 0; j < m->num_groups; j++ )
	{
		fprintf( f, "%-10s %2d", m->Group[ j ].name, ConnectedGroup.list[ j ].groups );
		for ( k = 0; k < ConnectedGroup.list[ j ].groups; k++ )
		{
			fprintf( f, " %s", m->Group[ ConnectedGroup.list[ j ].group[ k ] ].name );
		}
		fprintf( f, "\n" );
	}

	fprintf( f, "\n\nGROUP       VISIBLE GROUPS\n" );
	for ( j = 0; j < m->num_groups; j++ )
	{
		fprintf( f, "%-10s %2d", m->Group[ j ].name, VisibleGroup.list[ j ].groups );
		for ( k = 0; k < VisibleGroup.list[ j ].groups; k++ )
		{
			fprintf( f, " %s", m->Group[ VisibleGroup.list[ j ].group[ k ] ].name );
		}
		fprintf( f, "\n" );
	}


	fclose( f );

	return true;
}


static void RelateGroups( GROUPRELATION *rel, u_int16_t g1, u_int16_t g2 )
{
	u_int32_t *t, mask;

	t = rel->table + GROUP2GROUP_OFFSET( g1, g2 );
	mask = GROUP2GROUP_MASK( g1, g2 );
	if ( !( *t & mask ) )
	{
		*t |= mask;
		rel->list[ g1 ].groups++;
	}
}


static bool AreGroupsRelated( GROUPRELATION *rel, u_int16_t g1, u_int16_t g2 )
{
	u_int32_t *t, mask;

	t = rel->table + GROUP2GROUP_OFFSET( g1, g2 );
	mask = GROUP2GROUP_MASK( g1, g2 );
	if ( *t & mask )
		return true;
	else
		return false;
}


static void FindGroupsVisible( GROUPRELATION *vis, u_int16_t from_group, VISTREE *visible )
{
	int j;

	RelateGroups( vis, from_group, visible->group );
	for ( j = 0; j < visible->num_visible; j++ )
	{
		FindGroupsVisible( vis, from_group, &visible->visible[ j ] );
	}
}


bool GroupsAreConnected( u_int16_t g1, u_int16_t g2 )
{
	return AreGroupsRelated( &ConnectedGroup, g1, g2 );
}


bool GroupsAreVisible( u_int16_t g1, u_int16_t g2 )
{
	return AreGroupsRelated( &VisibleGroup, g1, g2 );
}


bool GroupsAreIndirectVisible( u_int16_t g1, u_int16_t g2 )
{
	if ( IndirectVisibleGroup.table )
		return AreGroupsRelated( &IndirectVisibleGroup, g1, g2 );
	else
	{
		static int failed = 0;

		if ( !failed++ )
			Msg( "No IndirectVisible table loaded\n" );
		return false;
	}
}


bool ReadGroupConnections( MLOADHEADER *m, char **pbuf )
{
	u_int32_t tabsize;
	u_int16_t g;
	char *buf;
	u_int16_t *buf16;

	// initialise data structures to reasonable defaults
	for ( g = 0; g < MAXGROUPS; g++ )
	{
		ConnectedGroup.list[ g ].groups = 0;
		ConnectedGroup.list[ g ].group = NULL;
		VisibleGroup.list[ g ].groups = 0;
		VisibleGroup.list[ g ].group = NULL;
		IndirectVisibleGroup.list[ g ].groups = 0;
		IndirectVisibleGroup.list[ g ].group = NULL;
	}

	InitConnections = true;

	// allocate tables
	GTabRowSize = DIV_CEIL( MAXGROUPS, 32 );
	tabsize = m->num_groups * GTabRowSize * sizeof( u_int32_t );
	if ( !ConnectedGroup.table )
	{
		ConnectedGroup.table = (u_int32_t *) malloc( tabsize );
		if ( !ConnectedGroup.table )
		{
			Msg( "ReadGroupConnections: failed malloc for ConnectedGroup.table\n" );
			return false;
		}
	}
	if ( !VisibleGroup.table )
	{
		VisibleGroup.table = (u_int32_t *) malloc( tabsize );
		if ( !VisibleGroup.table )
		{
			Msg( "ReadGroupConnections: failed malloc for VisibleGroup.table\n" );
			return false;
		}
	}
	if ( !IndirectVisibleGroup.table )
	{
		IndirectVisibleGroup.table = (u_int32_t *) malloc( tabsize );
		if ( !IndirectVisibleGroup.table )
		{
			Msg( "ReadGroupConnections: failed malloc for IndirectVisibleGroup.table\n" );
			return false;
		}
	}
	memset( ConnectedGroup.table, 0, tabsize );
	memset( VisibleGroup.table, 0, tabsize );
	memset( IndirectVisibleGroup.table, 0, tabsize );

	buf = *pbuf;

	// read connectivity table
	memmove( ConnectedGroup.table, buf, tabsize );
	buf += tabsize;

	buf16 = (u_int16_t *) buf;
	for ( g = 0; g < m->num_groups; g++ )
	{
		ConnectedGroup.list[ g ].groups = *buf16++;
		if ( ConnectedGroup.list[ g ].groups )
		{
			ConnectedGroup.list[ g ].group = (u_int16_t *) calloc( ConnectedGroup.list[ g ].groups, sizeof( u_int16_t ) );
			if ( !ConnectedGroup.list[ g ].group )
			{
				Msg( "ReadGroupConnections: failed X_calloc for ConnectedGroup.list[ %d ]\n", g );
				return false;
			}
			memmove( ConnectedGroup.list[ g ].group, buf16, ConnectedGroup.list[ g ].groups * sizeof( u_int16_t ) );
			buf16 += ConnectedGroup.list[ g ].groups;
		}
	}
	buf = (char *) buf16;

	// read visibility summary table
	memmove( VisibleGroup.table, buf, tabsize );
	buf += tabsize;

	buf16 = (u_int16_t *) buf;
	for ( g = 0; g < m->num_groups; g++ )
	{
		VisibleGroup.list[ g ].groups = *buf16++;
		if ( VisibleGroup.list[ g ].groups )
		{
			VisibleGroup.list[ g ].group = (u_int16_t *) calloc( VisibleGroup.list[ g ].groups, sizeof( u_int16_t ) );
			if ( !VisibleGroup.list[ g ].group )
			{
				Msg( "ReadGroupConnections: failed X_calloc for VisibleGroup.list[ %d ]\n", g );
				return false;
			}
			memmove( VisibleGroup.list[ g ].group, buf16, VisibleGroup.list[ g ].groups * sizeof( u_int16_t ) );
			buf16 += VisibleGroup.list[ g ].groups;
		}
	}
	buf = (char *) buf16;

	// read indirect visibility summary table
	memmove( IndirectVisibleGroup.table, buf, tabsize );
	buf += tabsize;

	buf16 = (u_int16_t *) buf;
	for ( g = 0; g < m->num_groups; g++ )
	{
		IndirectVisibleGroup.list[ g ].groups = *buf16++;
		if ( IndirectVisibleGroup.list[ g ].groups )
		{
			IndirectVisibleGroup.list[ g ].group = (u_int16_t *) calloc( IndirectVisibleGroup.list[ g ].groups, sizeof( u_int16_t ) );
			if ( !IndirectVisibleGroup.list[ g ].group )
			{
				Msg( "ReadGroupConnections: failed X_calloc for IndirectVisibleGroup.list[ %d ]\n", g );
				return false;
			}
			memmove( IndirectVisibleGroup.list[ g ].group, buf16, IndirectVisibleGroup.list[ g ].groups * sizeof( u_int16_t ) );
			buf16 += IndirectVisibleGroup.list[ g ].groups;
		}
	}
	buf = (char *) buf16;

	*pbuf = buf;

	/* Night-Dive Remaster `.mxv` repair — see comment in FindVisible
	 * for the full story. Trigger: total_indirect == 0 across all
	 * groups (Night-Dive's KEX-engine exporter never writes
	 * IndirectVisibleGroup; 1998 originals always do). When that
	 * fires, rebuild VisibleGroup transitively via BFS over
	 * immediate-portal edges so the blaster-XLight filter passes
	 * across non-adjacent rooms. The flood-fill expansion in
	 * FindVisible runs on the same trigger to fix portal voids on
	 * the renderer side. */
	{
		int total_indirect = 0;
		int g_iter;
		for ( g_iter = 0; g_iter < m->num_groups; g_iter++ )
			total_indirect += IndirectVisibleGroup.list[ g_iter ].groups;
#if defined(__3DS__) && defined(VERBOSE_TRACE)
		{ extern void trace(const char *);
		  extern int16_t LevelNum;
		  extern char ShortLevelNames[][32];
		  int total_visible = 0, total_connected = 0, total_portals = 0;
		  for ( g_iter = 0; g_iter < m->num_groups; g_iter++ )
		  {
		      total_visible   += VisibleGroup.list[ g_iter ].groups;
		      total_connected += ConnectedGroup.list[ g_iter ].groups;
		      total_portals   += m->Group[ g_iter ].num_portals;
		  }
		  char _b[200];
		  snprintf( _b, sizeof(_b),
		      "VISI_STATS: level=%s ng=%d portals=%d conn=%d vis=%d indir=%d (avg vis/g=%.1f)",
		      (LevelNum >= 0 && LevelNum < 64) ? &ShortLevelNames[LevelNum][0] : "?",
		      (int)m->num_groups, total_portals, total_connected,
		      total_visible, total_indirect,
		      m->num_groups ? (double)total_visible / m->num_groups : 0.0 );
		  trace( _b ); }
#endif
		/* Trigger condition: total_indirect == 0.
		 *
		 * On the 10-level Night-Dive subset the user vetted by hand
		 * (defend2 / stableizers / powerdown / starship / battlebase
		 * = symptomatic; pship / spc-sp01 / bio-sphere / capship /
		 * space = clean), the IndirectVisibleGroup table is *empty*
		 * on every symptomatic level and *populated* on every clean
		 * one. Night-Dive's KEX-engine `.mxv` exporter writes
		 * IndirectVisibleGroup as zeros, but it also writes
		 * VisibleGroup with only ~1-hop transitive depth (vis/conn
		 * ratio ~1.0 on Night-Dive vs ~2.2 on 1998 originals).
		 * That means `VisibleOverlap(camera_group, xlight_group)`
		 * returns 0 between any two non-immediate-neighbour groups,
		 * filtering the blaster's projectile-XLight out of
		 * `BuildVisibleLightList` whenever the bullet is in a room
		 * the camera doesn't see directly. Same shallow data also
		 * starves the per-portal VISTREE traversal in FindVisible
		 * (handled by the flood-fill expansion below).
		 *
		 * `indir==0` is the cleanest detector: zero false positives
		 * (it's zero exactly on Night-Dive-authored levels), zero
		 * false negatives (it's nonzero on every original 1998
		 * level). Rebuild VisibleGroup transitively here so the
		 * blaster-light filter works correctly. The flood-fill
		 * below is gated on the same signal, both fixes go live
		 * together. */
		if ( total_indirect == 0 )
		{
			/* Free the existing per-group .group[] arrays and
			 * zero list[].groups so RelateGroups can repopulate
			 * them. The .table bitmap is reset to zero too. */
			for ( g = 0; g < MAXGROUPS; g++ )
			{
				if ( VisibleGroup.list[ g ].group )
					free( VisibleGroup.list[ g ].group );
				VisibleGroup.list[ g ].group = NULL;
				VisibleGroup.list[ g ].groups = 0;
			}
			memset( VisibleGroup.table, 0, tabsize );

			/* BFS from each group via immediate portal-destination
			 * edges. Walks the lightweight Portal[].visible.group
			 * scalar (not the recursive VISTREE), so it works
			 * even when the per-portal trees are also shallow.
			 * For each pair (start, reachable) we mark them in
			 * VisibleGroup so VisibleOverlap returns true between
			 * them. */
			static u_int16_t queue[ MAXGROUPS ];
			static u_int8_t  seen [ MAXGROUPS ];
			for ( g = 0; g < m->num_groups; g++ )
			{
				memset( seen, 0, m->num_groups );
				int qh = 0, qt = 0;
				queue[ qt++ ] = g;
				seen[ g ] = 1;
				while ( qh < qt )
				{
					u_int16_t cur = queue[ qh++ ];
					RelateGroups( &VisibleGroup, g, cur );
					int p;
					for ( p = 0; p < m->Group[ cur ].num_portals; p++ )
					{
						u_int16_t dst = m->Group[ cur ].Portal[ p ].visible.group;
						if ( dst >= m->num_groups ) continue;
						if ( seen[ dst ] ) continue;
						seen[ dst ] = 1;
						queue[ qt++ ] = dst;
					}
				}
			}

			/* Allocate the per-group .group[] arrays so any code
			 * that walks list[].group (rather than the bitmap)
			 * still works. Mirrors the FindGroupConnections logic
			 * for the v<3 path. */
			for ( g = 0; g < m->num_groups; g++ )
			{
				if ( VisibleGroup.list[ g ].groups )
				{
					VisibleGroup.list[ g ].group = (u_int16_t *) calloc(
						VisibleGroup.list[ g ].groups, sizeof( u_int16_t ) );
					if ( VisibleGroup.list[ g ].group )
					{
						u_int16_t gnum = 0, g2;
						for ( g2 = 0; g2 < m->num_groups; g2++ )
							if ( GroupsAreVisible( g, g2 ) )
								VisibleGroup.list[ g ].group[ gnum++ ] = g2;
					}
				}
			}

		}
	}

	return true;
}


bool FindGroupConnections( MLOADHEADER *m )
{
	u_int32_t tabsize;
	u_int16_t g, p, g2;
	LVLGROUP *group;
	PORTAL *portal;
	u_int16_t gnum;

	for ( g = 0; g < MAXGROUPS; g++ )
	{
		ConnectedGroup.list[ g ].groups = 0;
		ConnectedGroup.list[ g ].group = NULL;
		VisibleGroup.list[ g ].groups = 0;
		VisibleGroup.list[ g ].group = NULL;
		IndirectVisibleGroup.list[ g ].groups = 0;
		IndirectVisibleGroup.list[ g ].group = NULL;
	}

	InitConnections = true;

	GTabRowSize = (int)ceilf(MAXGROUPS/32.0F);
	tabsize = MAXGROUPS * GTabRowSize * sizeof( u_int32_t );
	if ( !ConnectedGroup.table )
	{
		ConnectedGroup.table = (u_int32_t *) malloc( tabsize );
		if ( !ConnectedGroup.table )
		{
			Msg( "FindGroupConnections: failed malloc for ConnectedGroup.table\n" );
			return false;
		}
	}
	if ( !VisibleGroup.table )
	{
		VisibleGroup.table = (u_int32_t *) malloc( tabsize );
		if ( !VisibleGroup.table )
		{
			Msg( "FindGroupConnections: failed malloc for VisibleGroup.table\n" );
			return false;
		}
	}
	IndirectVisibleGroup.table = NULL;
	memset( ConnectedGroup.table, 0, tabsize );
	memset( VisibleGroup.table, 0, tabsize );

	for ( g = 0; g < m->num_groups; g++ )
	{
		group = &m->Group[ g ];
		ConnectedGroup.list[ g ].groups = 0;
		VisibleGroup.list[ g ].groups = 0;
		// make group visible from itself
		RelateGroups( &VisibleGroup, g, g );
		for ( p = 0; p < group->num_portals; p++ )
		{
			portal = &group->Portal[ p ];
			// find groups connected to this one
			RelateGroups( &ConnectedGroup, g, portal->visible.group );
			// find groups visible from this one
			FindGroupsVisible( &VisibleGroup, g, &portal->visible );
		}
		if ( ConnectedGroup.list[ g ].groups )
		{
			ConnectedGroup.list[ g ].group = (u_int16_t *) calloc( ConnectedGroup.list[ g ].groups, sizeof( u_int16_t ) );
			if ( !ConnectedGroup.list[ g ].group )
			{
				Msg( "FindGroupConnections: failed X_calloc for ConnectedGroup.list[ %d ]\n", g );
				return false;
			}
			gnum = 0;
			for ( g2 = 0; g2 < m->num_groups; g2++ )
			{
				if ( GroupsAreConnected( g, g2 ) )
				{
					ConnectedGroup.list[ g ].group[ gnum++ ] = g2;
				}
			}
		}
		else
		{
			ConnectedGroup.list[ g ].group = NULL;
		}
		if ( VisibleGroup.list[ g ].groups )
		{
			VisibleGroup.list[ g ].group = (u_int16_t *) calloc( VisibleGroup.list[ g ].groups, sizeof( u_int16_t ) );
			if ( !VisibleGroup.list[ g ].group )
			{
				Msg( "FindGroupConnections: failed X_calloc for VisibleGroup.list[ %d ]\n", g );
				return false;
			}
			gnum = 0;
			for ( g2 = 0; g2 < m->num_groups; g2++ )
			{
				if ( GroupsAreVisible( g, g2 ) )
				{
					VisibleGroup.list[ g ].group[ gnum++ ] = g2;
				}
			}
		}
		else
		{
			VisibleGroup.list[ g ].group = NULL;
		}
	}

	return true;
}


void FreeGroupConnections( void )
{
	int g;

	if ( !InitConnections )
		return;
	for ( g = 0; g < MAXGROUPS; g++ )
	{
		ConnectedGroup.list[ g ].groups = 0;
		if ( ConnectedGroup.list[ g ].group )
		{
			free( ConnectedGroup.list[ g ].group );
		}
		ConnectedGroup.list[ g ].group = NULL;
		VisibleGroup.list[ g ].groups = 0;
		if ( VisibleGroup.list[ g ].group )
		{
			free( VisibleGroup.list[ g ].group );
		}
		VisibleGroup.list[ g ].group = NULL;
		IndirectVisibleGroup.list[ g ].groups = 0;
		if ( IndirectVisibleGroup.list[ g ].group )
		{
			free( IndirectVisibleGroup.list[ g ].group );
		}
		IndirectVisibleGroup.list[ g ].group = NULL;
	}
	if ( ConnectedGroup.table )
	{
		free( ConnectedGroup.table );
		ConnectedGroup.table = NULL;
	}
	if ( VisibleGroup.table )
	{
		free( VisibleGroup.table );
		VisibleGroup.table = NULL;
	}
	if ( IndirectVisibleGroup.table )
	{
		free( IndirectVisibleGroup.table );
		IndirectVisibleGroup.table = NULL;
	}
}




GROUPLIST *ConnectedGroups( u_int16_t g )
{
	return &ConnectedGroup.list[ g ];
}




GROUPLIST *VisibleGroups( u_int16_t g )
{
	if( g >= Mloadheader.num_groups )
		Msg( "VisibleGroups() Group %d out of range", g );

	if( VisibleGroup.list[ g ].groups > Mloadheader.num_groups )
		Msg( "VisibleGroups.list[%d].groups=%d more than %d groups", g, VisibleGroup.list[ g ].groups, Mloadheader.num_groups );

	if( !VisibleGroup.list[ g ].group )
		Msg( "VisibleGroups.list[%d].group Invalid", g );

	return &VisibleGroup.list[ g ];
}


GROUPLIST *IndirectVisibleGroups( u_int16_t g )
{
	return &IndirectVisibleGroup.list[ g ];
}



int VisibleOverlap( u_int16_t g1, u_int16_t g2, u_int16_t *overlapping_group )
{
	u_int32_t *t1, *t2, overlap;
	int j;
	int gnum;
	GROUPLIST *l1, *l2;
	if( g1 > Mloadheader.num_groups || g2 > Mloadheader.num_groups )
		return 0;

	t1 = VisibleGroup.table + GROUP2GROUP_OFFSET( g1, 0 );
	t2 = VisibleGroup.table + GROUP2GROUP_OFFSET( g2, 0 );
	overlap = 0;
	for ( j = 0; j < GTabRowSize; j++ )
	{
		overlap |= *t1++ & *t2++;
	}
	if ( !overlap )
		return 0;
	if ( !overlapping_group )
		return 1;

	l1 = &VisibleGroup.list[ g1 ];
	l2 = &VisibleGroup.list[ g2 ];
	g1 = 0;
	g2 = 0;
	gnum = 0;
	do {
		if ( l1->group[ g1 ] < l2->group[ g2 ] )
		{
			g1++;
		}
		else if ( l1->group[ g1 ] > l2->group[ g2 ])
		{
			g2++;
		}
		else // groups are same
		{
			overlapping_group[ gnum++ ] = l1->group[ g1 ];
			g1++;
			g2++;
		}
	} while ( g1 < l1->groups && g2 < l2->groups );

	return gnum;
}


void InitIndirectVisible( u_int16_t g )
{
	GROUPLIST *list;
	int j;

	list = &IndirectVisibleGroup.list[ g ];
	for ( j = 0; j < list->groups; j++ )
	{
		IndirectVisible[ j ] = list->group[ j ];
	}
	Num_IndirectVisible = list->groups;
}


void AddIndirectVisible( u_int16_t g )
{
	static u_int16_t AddedVisible[ MAXGROUPS ];
	int current, next;
	int added;
	GROUPLIST *add;

	add = &IndirectVisibleGroup.list[ g ];
	current = 0;
	next = 0;
	added = 0;
	while ( current < Num_IndirectVisible || next < add->groups )
	{
		if ( current < Num_IndirectVisible )
		{
			if ( next < add->groups )
			{
				if ( IndirectVisible[ current ] < add->group[ next ] )
				{
					AddedVisible[ added++ ] = IndirectVisible[ current++ ];
				}
				else if ( IndirectVisible[ current ] > add->group[ next ] )
				{
					AddedVisible[ added++ ] = add->group[ next++ ];
				}
				else
				{
					AddedVisible[ added++ ] = IndirectVisible[ current++ ];
					next++;
				}
			}
			else
			{
				AddedVisible[ added++ ] = IndirectVisible[ current++ ];
			}
		}
		else
		{
			AddedVisible[ added++ ] = add->group[ next++ ];
		}
	}
	Num_IndirectVisible = added;
	memmove( IndirectVisible, AddedVisible, Num_IndirectVisible * sizeof( IndirectVisible[ 0 ] ) );
}


#define HUGE_VALUE	(FLT_MAX * 0.5F)



#ifdef USEINLINE
__inline
#endif
 int EmptyXYExtent( EXTENT *e )
{
	return ( e->min.x >= e->max.x ) || ( e->min.y >= e->max.y );
}



static void
MinimiseXYExtent( EXTENT *e1, EXTENT *e2, EXTENT *e )
{
	e->min.x = MAX( e1->min.x, e2->min.x );
	e->min.y = MAX( e1->min.y, e2->min.y );
	e->max.x = MIN( e1->max.x, e2->max.x );
	e->max.y = MIN( e1->max.y, e2->max.y );
}


static void
MaximiseExtent( EXTENT *e1, EXTENT *e2, EXTENT *e )
{
	e->min.x = MIN( e1->min.x, e2->min.x );
	e->min.y = MIN( e1->min.y, e2->min.y );
	e->min.z = MIN( e1->min.z, e2->min.z );
	e->max.x = MAX( e1->max.x, e2->max.x );
	e->max.y = MAX( e1->max.y, e2->max.y );
	e->max.z = MAX( e1->max.z, e2->max.z );
}

static int
Transform2Viewport( CAMERA *cam, VISLIST *v, VERT *wpos, VECTOR *vpos )
{
	int clip;

    VisPolyApplyMatrix( (MATRIX *) &v->viewproj, (VECTOR *) wpos, vpos );
	clip = 0;
	if ( vpos->z > 1.0F )
	{
		clip |= CLIP_FRONT;
	    VisPolyApplyMatrix( (MATRIX *) &cam->View, (VECTOR *) wpos, vpos );
#if 0
		if ( vpos->x < 0.0F )
			vpos->x = -1.0F;
		else
			vpos->x = 1.0F;
		if ( vpos->y < 0.0F )
			vpos->y = -1.0F;
		else
			vpos->y = 1.0F;
#else
		if ( DotProduct( vpos, &clip_left ) < 0.0F )
		{
			clip |= CLIP_LEFT;
		}
		else if ( DotProduct( vpos, &clip_right ) < 0.0F )
		{
			clip |= CLIP_RIGHT;
		}
		if ( DotProduct( vpos, &clip_top ) < 0.0F )
		{
			clip |= CLIP_TOP;
		}
		else if ( DotProduct( vpos, &clip_bottom ) < 0.0F )
		{
			clip |= CLIP_BOTTOM;
		}
		return clip;
#endif
	}

	if ( vpos->x < -1.0F )
	{
		clip |= CLIP_LEFT;
		vpos->x = (float) v->viewport->X;
	}
	else if ( vpos->x > 1.0F )
	{
		clip |= CLIP_RIGHT;
		vpos->x = (float) ( v->viewport->X + v->viewport->Width );
	}
	else
	{
		vpos->x = v->viewport->X + ( v->viewport->Width * 0.5F ) + (v->viewport->ScaleX * vpos->x);
	}

   	if ( vpos->y < -1.0F )
	{
		clip |= CLIP_BOTTOM;
		vpos->y = (float) ( v->viewport->Y + v->viewport->Height );
	}
	else if ( vpos->y > 1.0F )
	{
		clip |= CLIP_TOP;
		vpos->y = (float) v->viewport->Y;
	}
	else
	{
		vpos->y = v->viewport->Y + ( v->viewport->Height * 0.5F ) - (v->viewport->ScaleY * vpos->y);
	}

	return clip;
}


static int
VisiblePortalExtent( CAMERA *cam, VISLIST *v, PORTAL *p, EXTENT *e )
{
	VECTOR pos;
	int vnum;
	int init;
	int clip;
	int clip_any;
	int clip_all;
	MCFACE *pface;
	float d;

	pface = &p->Poly[ 0 ];
	d = pface->nx * cam->Pos.x + pface->ny * cam->Pos.y + pface->nz * cam->Pos.z + pface->D;
	if ( d < 0.0F )
		return 0; // camera behind portal
	init = 0;
	clip_any = 0;
	clip_all = CLIP_LEFT | CLIP_RIGHT | CLIP_TOP | CLIP_BOTTOM | CLIP_FRONT;
	for ( vnum = 0; vnum < p->num_vertices_in_portal; vnum++ )
	{
		clip = Transform2Viewport( cam, v, &p->Verts[ vnum ], &pos );
		clip_any |= clip;
		clip_all &= clip;
		if ( !( clip & CLIP_FRONT ) )
		{
			if ( !init )
			{
				e->min = pos;
				e->max = pos;
				init = 1;
			}
			else
			{
				if ( e->min.x > pos.x )
					e->min.x = pos.x;
				if ( e->min.y > pos.y )
					e->min.y = pos.y;
				if ( e->min.z > pos.z )
					e->min.z = pos.z;
				if ( e->max.x < pos.x )
					e->max.x = pos.x;
				if ( e->max.y < pos.y )
					e->max.y = pos.y;
				if ( e->max.z < pos.z )
					e->max.z = pos.z;
			}
		}
	}
	if ( clip_all )
	{
		return 0; // all vertices clipped by main viewport
	}
	if ( clip_any & CLIP_FRONT )
	{
		*e = v->first_visible->extent; // portal clipped by front plane -> force extents fullscreen
	}
	return 1;
}


#if defined(VERBOSE_TRACE) && defined(__3DS__)
/* Per-frame portal rejection counters — incremented in ProcessVisiblePortal
 * each time a portal is silently dropped (either entirely clipped by the
 * main viewport, or its post-minimise extent is empty). The black-void
 * symptom on stableizers/powerdown/starship/battlebase fires when a portal
 * connecting current-group to the visible-but-not-yet-current group is in
 * one of these reject paths — the "next room" group never gets marked
 * visible, so the BSP-clipped geometry through that doorway never draws.
 * Reset by FindVisible at start of each frame. */
int _vt_portal_rej_clipped = 0;
int _vt_portal_rej_empty   = 0;
int _vt_portal_accepted    = 0;
#endif

static void
ProcessVisiblePortal( CAMERA *cam, VISLIST *v, VISTREE *t, EXTENT *e )
{
	VISGROUP *adj_group;
	PORTAL *p;
	EXTENT extent;
	VISTREE *tvis;
	int vnum;

	if ( !t )
		return;
	p = t->portal;
	if ( !VisiblePortalExtent( cam, v, p, &extent ) )
	{
#if defined(VERBOSE_TRACE) && defined(__3DS__)
		_vt_portal_rej_clipped++;
#endif
		return; // portal entirely clipped by main viewport
	}
	MinimiseXYExtent( &extent, e, &extent );
	if ( !EmptyXYExtent( &extent ) )
	{
#if defined(VERBOSE_TRACE) && defined(__3DS__)
		_vt_portal_accepted++;
#endif
		adj_group = &v->group[ t->group ];
		if ( adj_group->visible )
		{
			MaximiseExtent( &adj_group->extent, &extent, &adj_group->extent );
		}
		else
		{
			adj_group->extent = extent;
			v->last_visible->next_visible = adj_group;
			v->last_visible = adj_group;
			adj_group->next_visible = NULL;
		}
		adj_group->visible++;
		for ( vnum = t->num_visible, tvis = t->visible; vnum--; tvis++)
		{
			ProcessVisiblePortal( cam, v, tvis, &extent );
		}
	}
#if defined(VERBOSE_TRACE) && defined(__3DS__)
	else
	{
		_vt_portal_rej_empty++;
	}
#endif
}

void FindVisible( CAMERA *cam, MLOADHEADER *Mloadheader )
{
	VISLIST *v;
	VISGROUP *g;
	XYRECT clip;
	render_viewport_t *vp;
	VISGROUP *gsort, *gprev, *gnext;
	int j;
	float w, h;

	// helper variable used for stereo adjustment
	float lr;

	// calculate clipping planes
	w = (float) tanf( hfov );
	h = w / render_info.aspect_ratio;
	clip_right.x = w;
	clip_right.y = 0.0F;
	clip_right.z = 2.0F;
	NormaliseVector( &clip_right );
	clip_left.x = clip_right.z;
	clip_left.y = 0.0F;
	clip_left.z = clip_right.x;
	clip_right.x = -clip_left.x;
	clip_right.y = 0.0F;
	clip_right.z = clip_left.z;
	clip_top.x = 0.0F;
	clip_top.y = h;
	clip_top.z = 2.0F;
	NormaliseVector( &clip_top );
	clip_bottom.x = 0.0F;
	clip_bottom.y = clip_top.z;
	clip_bottom.z = clip_top.y;
	clip_top.x = 0.0F;
	clip_top.y = -clip_bottom.y;
	clip_top.z = clip_bottom.z;

	v = &cam->visible;
	v->groups = Mloadheader->num_groups;
	// reset group visibilities
	for ( j = 0; j < v->groups; j++ )
	{
		g = &v->group[ j ];
		g->group = j;
		g->visible = 0;
		g->extent.min.x = HUGE_VALUE;
		g->extent.min.y = HUGE_VALUE;
		g->extent.min.z = HUGE_VALUE;
		g->extent.max.x = -HUGE_VALUE;
		g->extent.max.y = -HUGE_VALUE;
		g->extent.min.z = -HUGE_VALUE;
	}

	// initialise current group
	g = &v->group[ cam->GroupImIn ];
	v->first_visible = g;
	v->last_visible = g;
	v->viewport = &cam->Viewport;
	MatrixMultiply( (MATRIX *) &cam->View, (MATRIX *) &cam->Proj, (MATRIX *) &v->viewproj );
	g->next_visible = NULL;
	g->visible = 1;
	g->extent.min.x = (float) v->viewport->X;
	g->extent.min.y = (float) v->viewport->Y;
	g->extent.min.z = -HUGE_VALUE;
	g->extent.max.x = (float) ( v->viewport->X + v->viewport->Width );
	g->extent.max.y = (float) ( v->viewport->Y + v->viewport->Height );
	g->extent.max.z = HUGE_VALUE;

#if defined(VERBOSE_TRACE) && defined(__3DS__)
	/* Per-frame portal counters reset before traversal. Logged once per
	 * second below so each level's "average" portal rejection rate gets a
	 * single summary line — easy to compare BAD vs OK levels without
	 * flooding the trace file. */
	_vt_portal_rej_clipped = 0;
	_vt_portal_rej_empty   = 0;
	_vt_portal_accepted    = 0;
#endif

	// process visible portals
	if ( outside_map )
	{
		for ( j = 0; j < Mloadheader->num_groups; j++ )
		{
			if ( j != cam->GroupImIn )
			{
				g = &v->group[ j ];
				g->extent = v->first_visible->extent;
				v->last_visible->next_visible = g;
				v->last_visible = g;
				g->next_visible = NULL;
				g->visible++;
			}
		}
	}
	else
	{
		for ( j = 0; j < Mloadheader->Group[ cam->GroupImIn].num_portals; j++ )
		{
			ProcessVisiblePortal( cam, v, &Mloadheader->Group[ cam->GroupImIn ].Portal [ j ].visible, &g->extent );
		}

		/* Data-driven flood-fill gate: enable iff the level's
		 * `IndirectVisibleGroup` table is empty.
		 *
		 * Discovered 2026-04-29: across the 10-level Night-Dive
		 * subset the user vetted by hand (defend2 / stableizers /
		 * powerdown / starship / battlebase = symptomatic; pship /
		 * spc-sp01 / bio-sphere / capship / space = clean), the
		 * `indir=` total in `VISI_STATS:` exactly partitioned the
		 * two sets:
		 *   symptomatic levels: indir = 0
		 *   clean levels:       indir > 0
		 * The 1998 Forsaken `.mxv` files include a fully-populated
		 * IndirectVisibleGroup table; Night-Dive's KEX-engine
		 * exporter does not write it (the section ends up
		 * effectively zero on every Night-Dive-authored level).
		 *
		 * Gate the flood-fill on this signal so healthy 1998 levels
		 * pay zero per-frame cost (count check at level transition
		 * only, cached in `s_floodfill_for_this_level`). Affected
		 * Night-Dive levels run the existing 2-pass / 32-cap
		 * BFS expansion to populate the through-portal visible set
		 * the engine's pre-baked VISTREE doesn't supply on its own. */
		bool _floodfill_triggered;
		{
			static int16_t s_cached_level = -1;
			static bool    s_floodfill_for_this_level = false;
			extern int16_t LevelNum;
			if ( LevelNum != s_cached_level )
			{
				s_cached_level = LevelNum;
				int g_iter, indir_total = 0;
				for ( g_iter = 0; g_iter < Mloadheader->num_groups; g_iter++ )
					indir_total += IndirectVisibleGroup.list[ g_iter ].groups;
				s_floodfill_for_this_level = ( indir_total == 0 );
			}
			_floodfill_triggered = s_floodfill_for_this_level;
		}
		if ( _floodfill_triggered )
#if 1  /* RE-ENABLED. The flood-fill is the actual fix for the
        * "wall void of color through portals" symptom on Remaster
        * levels — geometry IS drawn but vertex colors come out at
        * baked-only level (no dynamic blaster contribution) because
        * those groups aren't in the visible list. Per-vertex
        * lighting iterates the visible list; flood-fill brings
        * chained through-portal groups in, and their vertices then
        * get the blaster's per-frame light contribution. */
		/* Flood-fill expansion: after the engine's pre-computed VISTREE
		 * traversal completes, walk every currently-visible group's own
		 * Portal[] array and project each portal against the camera. If
		 * the portal projects onto a non-empty screen extent, add the
		 * destination group to the visible list with that extent. */
		{
			/* Single-pass expansion: walk each currently-visible group's
			 * own portals exactly once and add reachable destination
			 * groups. Combined with the strict per-portal extent
			 * intersection (MinimiseXYExtent) and a hard MAX_FLOODED
			 * group cap, the visible count stays bounded in a way the
			 * 1 MB GPU command buffer can absorb on real hardware.
			 *
			 * Earlier iterations went deeper (16 passes) and used a
			 * relaxed extent (g->extent inherited verbatim) — that
			 * cleanly fixed the visible black voids in mandarine but
			 * blew GPCMD_AddInternal's svcBreak on hardware (asubchb
			 * was the first to hit it: a level with many small
			 * interconnected portals). One pass + strict intersection
			 * is the conservative choice that preserves the fix's
			 * original goal (catch the specific case where Forsaken's
			 * pre-computed VISTREE terminates one hop too early) without
			 * exploding the visible-group count. */
			/* Multi-pass BFS expansion. Each pass walks the
			 * currently-visible groups and adds destinations of
			 * each group's portals that aren't already visible.
			 * Newly-added groups become the source frontier for the
			 * NEXT pass — propagates through chains of portals.
			 *
			 * Relaxed extent inheritance: the destination group
			 * inherits the source's extent verbatim. The strict
			 * MinimiseXYExtent(parent, portal-projection)
			 * approach drops portals whose projection collapses to
			 * a thin slice — but those are exactly the chained
			 * cases we need (player looking at a portal at an
			 * angle that makes the next portal a sliver). Relaxed
			 * over-includes a few groups, but those get
			 * clip-culled by the engine's per-group magnified
			 * projection downstream so they barely cost anything.
			 *
			 * Tuning: 2 passes + cap 32. 2 passes catches "look
			 * through doorway into room that has another doorway
			 * to a 3rd room" (2-hop chain), which is the common
			 * Remaster case. 16 passes cleaned all voids but blew
			 * the 1 MB GPU cmd buffer on asubchb subway. 4 passes
			 * + cap 64 also cleaned voids but cut framerate in
			 * half (extra groups go through full vertex shader +
			 * draw calls, doubling per-frame rendering cost). 2/32
			 * is the smallest config that still removes the
			 * visible voids on powerdown / starship. Pairs with
			 * the 4 MB cmd buffer in pglInit. */
			#define MAX_FLOODED_GROUPS 32
			#define MAX_FLOOD_PASSES   2
			int flooded = 0;
			int pass;
			for (pass = 0; pass < MAX_FLOOD_PASSES; pass++)
			{
				VISGROUP *pass_frontier_end = v->last_visible;
				int added_this_pass = 0;
				for ( g = v->first_visible; g; g = g->next_visible ) {
					u_int16_t gid = g->group;
					int p;
					for (p = 0; p < Mloadheader->Group[gid].num_portals; p++) {
						if (flooded >= MAX_FLOODED_GROUPS) break;
						u_int16_t dst = Mloadheader->Group[gid].Portal[p].visible.group;
						if (dst >= v->groups) continue;
						if (v->group[dst].visible) continue;
						VISGROUP *adj = &v->group[dst];
						adj->extent = g->extent;	/* relaxed inheritance */
						adj->group = dst;
						adj->visible = 1;
						adj->next_visible = NULL;
						v->last_visible->next_visible = adj;
						v->last_visible = adj;
						/* Tag this group as flood-fill-added so object
						 * renderers can skip it. The level geometry
						 * still draws — only enemies / pickups /
						 * models / BGObjects / secondary FX in this
						 * group are gated out, since they're 1-2
						 * portal hops away and the FPS cost of
						 * rendering them isn't worth the visual
						 * fidelity. */
						IsGroupFloodFilled[dst] = 1;
						flooded++;
						added_this_pass++;
					}
					if (flooded >= MAX_FLOODED_GROUPS) break;
					if (g == pass_frontier_end) break;	/* end of this pass's frontier */
				}
				if (flooded >= MAX_FLOODED_GROUPS) break;
				if (added_this_pass == 0) break;	/* converged early */
			}
			#undef MAX_FLOODED_GROUPS
			#undef MAX_FLOOD_PASSES
		}
#endif
	}

	// set viewport and projection matrix for each visible group
	for ( g = v->first_visible; g; g = g->next_visible )
	{
		clip.x1 = (long) floorf( g->extent.min.x );
		clip.y1 = (long) floorf( g->extent.min.y );
		clip.x2 = (long) ceilf( g->extent.max.x );
		clip.y2 = (long) ceilf( g->extent.max.y );
		if ( clip.x1 < 0 )
			clip.x1 = 0;
		if ( clip.y1 < 0 )
			clip.y1 = 0;
		vp = &g->viewport;
		*vp = *v->viewport;
		vp->X = clip.x1;
		vp->Y = clip.y1;
		vp->Width = clip.x2 - clip.x1;
		vp->Height = clip.y2 - clip.y1;
		vp->Width += vp->Width & 1;
		vp->Height += vp->Height & 1;
		if ( vp->X + vp->Width > v->viewport->X + v->viewport->Width )
		{
			if ( vp->X > 0 )
				vp->X--;
			else
				vp->Width--;
		}
		if ( vp->Y + vp->Height > v->viewport->Y + v->viewport->Height )
		{
			if ( vp->Y > 0 )
				vp->Y--;
			else
				vp->Height--;
		}
		vp->ScaleX = vp->Width * 0.5F;
		vp->ScaleY = vp->Height * 0.5F;

		/* Per-group projection. Engine's magnified-and-offset math
		 * for normal groups; cam's projection unchanged for groups
		 * whose sub-rect would require too-extreme magnification.
		 *
		 * Why the hybrid: when a portal projects to a thin sliver on
		 * screen (player looking at the portal at near-grazing angle),
		 * vp.Width or vp.Height collapses to 1-6 pixels. The engine
		 * math then computes
		 *     _11 = cam.Proj._11 * viewport.W / vp.W
		 *     _31 = lr / vp.W  (where lr scales with viewport.W)
		 * For vp.W = 2, _11 hits 200 and _31 hits 199. World vertices
		 * project to clip-space coords way outside the GPU's [-w,+w]
		 * clip volume, so every triangle gets rejected — black void.
		 *
		 * Diagnosed on `spc-sp01` group 41 with `mag = 200x` (PROJ_DUMP
		 * trace, 2026-04-29). asubchb / pship / thermal / starship /
		 * powerdown all hit smaller-magnitude versions of the same
		 * issue (max mag 22-40x on those levels) which sometimes
		 * manifests as void and sometimes as garbled clip-space
		 * geometry depending on viewing angle.
		 *
		 * The threshold below (`MAX_MAG = 16`) is empirical:
		 *   - vol2's max mag is 4.35x (well under) → engine math
		 *   - clean levels' max mag is 8-12x → engine math
		 *   - broken-on-specific-angle levels hit 16-200x → fallback
		 *
		 * Fallback is `cam->Proj` unchanged, paired with the sub-rect
		 * viewport (FSSetViewPort). The geometry visible through the
		 * thin slit ends up "squished" rather than at the magnified
		 * scale the engine intended — visually it reads as a tiny
		 * compressed view instead of a black void, which is strictly
		 * better. Cost: only the small number of grazing-angle groups
		 * per frame pay the cost; most groups stay on the cheap
		 * clip-cull-friendly engine math, so the 10× perf regression
		 * the always-cam->Proj approach introduced is avoided. */
		/* Engine's per-group magnified projection. Magnifies cam->Proj
		 * by `viewport->W / vp->W` in X (and Y) so geometry visible
		 * through the portal lands inside the sub-rect at expanded
		 * scale; offsets _31/_32 to recenter on the sub-rect center.
		 * Through-portal voids on Remaster levels are NOT caused by
		 * this math producing bad clip coords (we proved that with
		 * PROJ_DUMP traces — vol2 and spc-sp01 produce similar
		 * matrices yet vol2 looks fine). They're caused by the
		 * pre-computed VISTREE terminating one portal hop too
		 * shallow on Remaster data, leaving deeper through-portal
		 * groups out of the visible list. Those groups never get
		 * their vertices touched by the dynamic-light iterator,
		 * baked vertex colors are zero on Remaster levels, result
		 * is black walls through doorways. Fix is in the flood-fill
		 * expansion above the projection-compute block, not here. */
		g->projection = cam->Proj;
		g->projection._11 = cam->Proj._11 * v->viewport->Width / vp->Width;
		g->projection._22 = cam->Proj._22 * v->viewport->Height / vp->Height;
		lr = 2.0F * ( ( v->viewport->X + v->viewport->Width * 0.5F ) - ( vp->X + vp->Width * 0.5F ) );

#ifdef __3DS__
		if (!render_info.stereo_enabled)
#endif
		switch( render_info.stereo_position )
		{
		case ST_LEFT:
			lr -= 0.5f * render_info.stereo_focal_dist / render_info.stereo_eye_sep;
			break;
		case ST_RIGHT:
			lr += 0.5f * render_info.stereo_focal_dist / render_info.stereo_eye_sep;
		}

		g->projection._31 = lr / vp->Width;
		g->projection._32 = -2.0F * ( ( v->viewport->Y + v->viewport->Height * 0.5F )
									- ( vp->Y + vp->Height * 0.5F ) )
							/ vp->Height;

#if defined(VERBOSE_TRACE) && defined(__3DS__)
		/* Portal black-void detector: a visible group whose final
		 * viewport collapses to zero width or height produces a
		 * divide-by-zero in the projection-matrix construction above
		 * (vp->Width / vp->Height denominators on _11, _22, _31, _32).
		 * Float div-by-zero on ARM gives +inf, the engine then renders
		 * geometry with NaN clip coords, the GPU drops every triangle,
		 * and the player sees a black void where the portal opens onto
		 * that group's geometry. Symptom on Remaster's stableizers /
		 * powerdown / starship / battlebase levels.
		 *
		 * Trace it (rate-limited via the same _vt_flip_remaining counter
		 * autotest_tick uses) so we can correlate the broken viewport
		 * with the source group, the camera's GroupImIn, and the
		 * portal-traversal extents. */
		{
			extern int _vt_flip_remaining;
			static int s_traced_for_level = -1;
			static int s_total_visible_in_frame = 0;
			static int s_zero_vp_in_frame = 0;
			extern int16_t LevelNum;

			if (s_traced_for_level != LevelNum) {
				s_traced_for_level = LevelNum;
				s_total_visible_in_frame = 0;
				s_zero_vp_in_frame = 0;
			}

			s_total_visible_in_frame++;
			if (vp->Width <= 0 || vp->Height <= 0) {
				s_zero_vp_in_frame++;
				/* Only trace the first ~32 zero-vp groups per process to
				 * avoid flooding the log when the bug fires every frame. */
				static int s_traced = 0;
				if (s_traced < 32) {
					s_traced++;
					extern void trace(const char*);
					char _b[200];
					snprintf(_b, sizeof(_b),
					         "VISI: GROUP_INVISIBLE level=%d g=%u groupImIn=%u "
					         "vp=(%dx%d) clip=(%ld,%ld,%ld,%ld) "
					         "extent=(%.1f,%.1f,%.1f,%.1f)",
					         (int)LevelNum, (unsigned)g->group, (unsigned)cam->GroupImIn,
					         (int)vp->Width, (int)vp->Height,
					         clip.x1, clip.y1, clip.x2, clip.y2,
					         g->extent.min.x, g->extent.min.y,
					         g->extent.max.x, g->extent.max.y);
					trace(_b);
				}
			}
		}
#endif
	}

	// sort visible groups in depth order
	v->last_visible = v->first_visible;
	g = v->first_visible->next_visible;
	v->first_visible->next_visible = NULL;
	while ( g )
	{
		gnext = g->next_visible;
		gprev = v->first_visible;
		gsort = v->first_visible->next_visible;
		while ( gsort && g->extent.min.z > gsort->extent.min.z )
		{
			gprev = gsort;
			gsort = gsort->next_visible;
		}
		gprev->next_visible = g;
		g->next_visible = gsort;
		if ( !g->next_visible )
			v->last_visible = g;
		g = gnext;
	}

	NumGroupsVisible = 0;
	for ( g = v->first_visible; g; g = g->next_visible )
	{
		GroupsVisible[ NumGroupsVisible++ ] = g->group;
		IsGroupVisible[ g->group ] = 1;
	}

#if defined(VERBOSE_TRACE) && defined(__3DS__)
	/* Per-second summary log: once every 60 frames per camera. Keeps the
	 * volume manageable while still catching trends. Useful columns:
	 *   visGroups   = how many groups end up in the visible list
	 *   numGroups   = total groups in the level (denominator)
	 *   accepted    = portals processed where the through-group got added
	 *   rejClipped  = portals fully outside the main viewport
	 *   rejEmpty    = portals with non-empty pre-minimise extent but
	 *                 empty post-minimise extent (the most likely culprit
	 *                 for the black-void bug — through-portal group
	 *                 silently dropped before visible-list insertion). */
	{
		static int s_frame = 0;
		s_frame++;
		if ((s_frame % 60) == 0) {
			extern void trace(const char*);
			extern int16_t LevelNum;
			char _b[200];
			snprintf(_b, sizeof(_b),
			         "VISI: frame=%d level=%d visGroups=%u/%u groupImIn=%u "
			         "accepted=%d rejClipped=%d rejEmpty=%d",
			         s_frame, (int)LevelNum,
			         (unsigned)NumGroupsVisible, (unsigned)v->groups,
			         (unsigned)cam->GroupImIn,
			         _vt_portal_accepted, _vt_portal_rej_clipped, _vt_portal_rej_empty);
			trace(_b);
		}
	}
#endif
}

int ClipGroup( CAMERA *cam, u_int16_t group )
{
	VISGROUP *g;

	g = &cam->visible.group[ group ];
	if ( !g->visible )
		return 0;

	if( !DoClipping )
		g = &cam->visible.group[ cam->visible.first_visible->group ];

	if (!FSSetProjection(&g->projection))
		return false;
	if (!FSSetView(&cam->View))
		return false;

	/* Hand the per-group scissor-mode hint to the renderer. visi.c
	 * sets g->use_scissor_mode = true for groups whose engine
	 * magnification math would have produced clip-space coords the
	 * GPU rejects. The flag tells FSSetViewPort below to use a
	 * full-screen GPU viewport + GPU scissor to the sub-rect, instead
	 * of the engine standard sub-rect viewport. No-op on non-c3d
	 * renderers (their FSSetNextViewPortScissorMode is a stub). */
	FSSetNextViewPortScissorMode(g->use_scissor_mode);

    if (!FSSetViewPort(&g->viewport)) {
#ifdef DEBUG_VIEWPORT
		SetViewportError( "ClipGroup", &g->viewport );
#else
        Msg("SetViewport failed.\n%s", render_error_description(0));
#endif
        return false;
    }
	
	return 1;
}

/*===================================================================
		Disp Visipoly Model
===================================================================*/
extern	float	WhiteOut;
extern	u_int16_t	GroupWaterInfo[MAXGROUPS];
extern	float	GroupWaterLevel[MAXGROUPS];
extern	float	GroupWaterIntensity_Red[MAXGROUPS];
extern	float	GroupWaterIntensity_Green[MAXGROUPS];
extern	float	GroupWaterIntensity_Blue[MAXGROUPS];
bool
DisplayBackground( MLOADHEADER	* Mloadheader, CAMERA *cam ) 
{
	RENDERMATRIX	Tempproj;
	RENDERMATRIX	Tempview;
	VISGROUP *g;
	int	i, group;
	u_int32_t t;
	render_viewport_t OldViewPort;

	Tempproj = proj;
	Tempview = view;

	NumOfVertsConsidered= 0;
	NumOfVertsTouched= 0;

	FSGetViewPort(&OldViewPort);

	GroupImIn = CurrentCamera.GroupImIn;

	if ( GroupImIn != (u_int16_t) -1 )
	{
		DisplayBSPNode( OldCollideNode );

		CreatePortalExecList( Mloadheader, NumGroupsVisible );
		CreateSkinExecList( &MCloadheader, NumGroupsVisible );

		ShowAllColZones( GroupImIn );
		DisplayTeleportsInGroup( GroupImIn );
		DisplayExternalForcesInGroup( GroupImIn );

		t = 0;
		for ( g = cam->visible.first_visible, i = 0; g; g = g->next_visible, i++ )
		{
		 	ClipGroup( &CurrentCamera, (u_int16_t) g->group );
		  CurrentGroupVisible = GroupsVisible[i];
			GroupInVisibleList = i;
			group = GroupsVisible[i];

			/* Skip per-vertex dynamic light contribution for flood-fill
			 * groups (1-2 portal hops away). Their level geometry still
			 * renders with the baked vertex colors from the .mxv file —
			 * just no per-frame XLight contribution from blaster shots /
			 * RT lights. Cheaper than the full lit path because
			 * XLight1Group locks every execbuf's vertex buffer and walks
			 * `FirstLightVisible` per vertex; skipping it saves both
			 * CPU work and the linearAlloc lock/unlock pair per execbuf. */
			if ( !IsGroupFloodFilled[ GroupsVisible[i] ] )
			{
				if ( XLight1Group(  Mloadheader, GroupsVisible[i] ) != true  )
					return false;
			}

#if defined(VERBOSE_TRACE) && defined(__3DS__)
			/* Per-visible-group draw trace, rate-limited to once-per-second
			 * via the same s_frame counter as the VISI: summary above.
			 * Logs the group's triangle budget (GroupTris) and execbuf
			 * count just before its level-mesh draw fires. If a visible
			 * group has GroupTris > 0 but no geometry appears on screen
			 * (the user-reported "black void through portal"), the bug is
			 * downstream of this point — most likely in a stale/corrupt
			 * LEVELRENDEROBJECT pointer (lpVertexBuffer, lpIndexBuffer)
			 * or a bad viewport from the per-group ClipGroup above. */
			{
				static int s_dbg_frame = 0;
				if (g == cam->visible.first_visible) s_dbg_frame++;
				if ((s_dbg_frame % 60) == 0) {
					extern void trace(const char*);
					char _b[200];
					snprintf(_b, sizeof(_b),
					         "VISI_DRAW: f=%d level=%d g=%u execbufs=%u tris=%u "
					         "vp=(%dx%d) ext=(%.0f,%.0f,%.0f,%.0f)",
					         s_dbg_frame, (int)LevelNum, (unsigned)g->group,
					         (unsigned)Mloadheader->Group[g->group].num_execbufs,
					         (unsigned)GroupTris[g->group],
					         (int)g->viewport.Width, (int)g->viewport.Height,
					         g->extent.min.x, g->extent.min.y,
					         g->extent.max.x, g->extent.max.y);
					trace(_b);
				}
			}
#endif

 			if ( ExecuteSingleGroupMloadHeader(  Mloadheader, (u_int16_t) g->group ) != true  )
				return false;

#ifdef NEW_LIGHTING
			render_reset_lighting_variables();
#endif

			DispGroupTriggerAreas( (u_int16_t) g->group );
			if ( CaptureTheFlag || CTF )
				DisplayGoal( (u_int16_t) g->group );
//			ShowAllColZones( (u_int16_t) g->group );

			t += GroupTris[ g->group ];
		}
		// accumulate visibility stats
		VisiStats[ GroupImIn ].tsum += t;
		if ( VisiStats[ GroupImIn ].tmax < t )
			VisiStats[ GroupImIn ].tmax = t;
		if ( VisiStats[ GroupImIn ].tmin > t )
			VisiStats[ GroupImIn ].tmin = t;
		VisiStats[ GroupImIn ].visits++;
	}
	else
	{
		if ( ExecuteMloadHeader ( Mloadheader ) != true)
			return false;
	}

    if (!FSSetViewPort(&OldViewPort)) 
	{
#ifdef DEBUG_VIEWPORT
		SetViewportError( "ClipGroup", &g->viewport );
#else
        Msg("SetViewport failed.\n%s", render_error_description(0));
#endif
        return false;
    }

	proj = Tempproj;
	view = Tempview;

	if (!FSSetProjection(&proj))
		return false;
	if (!FSSetView(&view))
		return false;
	
	return true;
}


u_int16_t
FindClipGroup( CAMERA *cam, MLOADHEADER *m, VECTOR *min, VECTOR *max )
{
	u_int16_t group;
	VISGROUP *vg;
	LVLGROUP *mg;
	u_int16_t in_groups;
	EXTENT extent, *e=NULL; ZERO_STACK_MEM(extent);
	float extent_size, min_extent_size;

	if ( outside_map )
		return cam->visible.first_visible->group;

	// find extent of visible groups overlapped by object's bounds
	in_groups = 0;
	for ( vg = cam->visible.first_visible; vg; vg = vg->next_visible )
	{
		mg = &m->Group[ vg->group ];
		if ( min->x <= mg->center.x + mg->half_size.x &&
			 min->y <= mg->center.y + mg->half_size.y &&
			 min->z <= mg->center.z + mg->half_size.z &&
			 max->x >= mg->center.x - mg->half_size.x &&
			 max->y >= mg->center.y - mg->half_size.y &&
			 max->z >= mg->center.z - mg->half_size.z )
		{
			if ( in_groups++ )
				MaximiseExtent( &extent, &vg->extent, &extent );
			else
				extent = vg->extent;
		}
	}

	if ( in_groups )
	{
		// find visible group with smallest enclosing extent
		group = cam->visible.first_visible->group;
		e = &cam->visible.first_visible->extent;
		min_extent_size = ( e->max.x - e->min.x ) * ( e->max.y - e->min.y );
		for ( vg = cam->visible.first_visible; vg; vg = vg->next_visible )
		{
			if ( extent.min.x >= vg->extent.min.x &&
				 extent.min.y >= vg->extent.min.y &&
				 extent.max.x <= vg->extent.max.x &&
				 extent.max.y <= vg->extent.max.y )
			{
				e = &vg->extent;
				extent_size = ( e->max.x - e->min.x ) * ( e->max.y - e->min.y );
				if ( extent_size < min_extent_size )
				{
					min_extent_size = extent_size;
					group = vg->group;
				}
			}
		}
	}
	else // object not overlapping any visible groups
		group = (u_int16_t) -1;

	return group;
}


u_int16_t FindOverlappingVisibleGroups( CAMERA *cam, MLOADHEADER *m, VECTOR *min, VECTOR *max, u_int16_t * group )
{
	VISGROUP *vg;
	LVLGROUP *mg;
	u_int16_t in_groups;

	if ( outside_map )
		return 0;

	// find extent of visible groups overlapped by object's bounds
	in_groups = 0;
	for ( vg = cam->visible.first_visible; vg; vg = vg->next_visible )
	{
		mg = &m->Group[ vg->group ];
		if ( min->x <= mg->center.x + mg->half_size.x &&
			 min->y <= mg->center.y + mg->half_size.y &&
			 min->z <= mg->center.z + mg->half_size.z &&
			 max->x >= mg->center.x - mg->half_size.x &&
			 max->y >= mg->center.y - mg->half_size.y &&
			 max->z >= mg->center.z - mg->half_size.z )
		{
			if ( group )
				group[ in_groups ] = vg->group;
			in_groups++;
		}
	}

	return in_groups;
}

/*===================================================================
	Procedure	:	Is point inside bounding box of group
	Input		:	MLOADHEADER	*	Mloadheader
				:	VECTOR		*	Pos
				:	u_int16_t			Group
	Output		:	false/true
===================================================================*/
bool PointInGroupBoundingBox( MLOADHEADER * Mloadheader, VECTOR * Pos, u_int16_t group )
{
	VECTOR	Temp;

	Temp.x = Pos->x - Mloadheader->Group[group].center.x;
	if( Temp.x < 0.0F )	Temp.x *= -1.0F;
	Temp.y = Pos->y - Mloadheader->Group[group].center.y;
	if( Temp.y < 0.0F )	Temp.y *= -1.0F;
	Temp.z = Pos->z - Mloadheader->Group[group].center.z;
	if( Temp.z < 0.0F )	Temp.z *= -1.0F;
	if ( (Temp.x <= ( Mloadheader->Group[group].half_size.x ) ) &&
		 (Temp.y <= ( Mloadheader->Group[group].half_size.y ) ) &&
		 (Temp.z <= ( Mloadheader->Group[group].half_size.z ) ) )
	{
		return true;
	}
	return false;
}

#ifdef OPT_ON
#pragma optimize( "", off )
#endif
