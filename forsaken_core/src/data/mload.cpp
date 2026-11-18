/* mload.cpp -- loads the level mesh and its tables from the .mxv file
 *
 * PreMload reads the file and the texture names (and queues the textures);
 * Mload then builds, per visibility group, the render objects (vertex, index
 * and normal buffers plus texture groups), and reads the portals and their
 * visibility tree, the group relation tables (visi.cpp), the group-to-group
 * sound attenuation table, the lighting cells, the start positions, the group
 * up vectors, the background texture animations and the per-group clear
 * colours. Everything is copied out of the file, which is freed at the end of
 * Mload. ExecuteMloadHeader / ExecuteSingleGroupMloadHeader draw the groups;
 * BackGroundTextureAnimation steps the texture animations each frame.
 *
 * .mxv layout (little-endian, packed: fields have no alignment, so every read
 * goes through FileReader). Counts are u16 unless noted.
 *
 *   magic, version (u32 each; version 3)
 *   num_texture_files, texture names (NUL-terminated)       -- PreMload
 *   num_groups
 *   per group: num_execbufs
 *     per execbuf:
 *       execute-buffer size (i16, unused), exec_type (bit 0: has
 *       transparencies), num_vertices
 *       OLDLVERTEX[num_vertices]       x y z, reserved, colour, specular, tu tv
 *       num_texture_groups
 *       per texture group:
 *         texture type, start vertex, vertex count (all unused), tpage,
 *         num_triangles, MFACE[num_triangles]  v1 v2 v3, flags (bit 0:
 *         colour-keyed), face normal
 *   visibility flag: 0 = no visibility section (this 0 is then read again
 *       as num_animations below); 2 = present; anything else is rejected
 *   if present:
 *     per group: name, centre (VERT), half size (VERT), num_portals
 *       per portal: num_vertices, VERT[num_vertices], num_polys,
 *                   MCFACE[num_polys] (the portal's collision polygons)
 *     per group, per portal: the portal's visibility tree: target group,
 *       num_visible, then num_visible child nodes, each { portal index,
 *       group, num_visible, children ... } (read_visible)
 *     group relation tables, three times (connected, visible, indirectly
 *       visible): a bitmap of num_groups rows of DIV_CEIL(MAXGROUPS, 32)
 *       u32s, then per group a count and that many group numbers
 *     sound attenuation: float[num_groups][num_groups]
 *     cell flag: 0 = no lighting cells (the 0 is read again as
 *       num_start_positions); otherwise
 *       cell size (float; stored as its reciprocal)
 *       per group: num_execbufs (ignored), then per execbuf: cell origin
 *         (VECTOR), x / y / z cell counts, num_vertex_indices,
 *         u16[num_vertex_indices] (vertex numbers), VERTEXCELL[cells]
 *         (each a start and count in the vertex numbers)
 *       and per group: COLOR[cells of execbuf 0] (ambient per cell)
 *     num_start_positions, STARTPOS[...] (40 bytes: pos, dir, up, group, pad)
 *     up flag: 0 = every group's up is (0, 1, 0) (the 0 is read again as
 *       num_animations); otherwise VECTOR[num_groups]
 *   num_animations
 *   per animation: frame count and max loops (unused), length,
 *       u16[length] (frame numbers and TEXANIM_* commands)
 *   if num_animations: per group, per execbuf: num_animating_polys
 *     per poly: animation, frames, vertices, u16[vertices] (vertex numbers),
 *               { tu, tv }[vertices * frames]
 *   num_colours: if non-zero, per group a u32 background clear colour
 *       (top byte: clear flag)
 *   animation index flag: if non-zero, count, then per entry group,
 *       execbuf, poly, initial state and the trigger mod fired when it stops
 *       (i16); triggers address the animations through this index
 */

/*===================================================================
		Include File...	
===================================================================*/

#include "game/main.h"
#include <stdio.h>

#include "math/new3d.h"
#include "math/quat.h"
#include "ui/local.h"
#include "game/compobjects.h"
#include "game/bgobjects.h"
#include "game/object.h"
#include "rendering/stereomemo.h"
#include "net/networking.h"
#include "data/mload.h"
#include "rendering/lights.h"
#include "game/triggers.h"
#include "util/util.h"
#include "game/oct2.h"
#include "rendering/render.h"
#include "data/tload.h"
#include "util/hunk_alloc.h"
#include "world/visi.h"
#include "util/file_reader.h"

static void ReadSoundInfo( MLOADHEADER *m, FileReader & in );

/*===================================================================
		Defines
===================================================================*/
#define	MXV_VERSION_NUMBER	3

constexpr float UV_FUDGE = (0.5F);
constexpr float SAME_UV = (2.0F);

/*===================================================================
		Globals...	
===================================================================*/
float	UV_Fix = UV_FUDGE;
u_int16_t	num_start_positions = 0;
u_int16_t	last_start_position = 0;
GAMESTARTPOS		StartPositions[MAXSTARTPOSITIONS];			// pos and group info...

u_int16_t	FirstStartPositionInGroup[MAXGROUPS];


float	SoundInfo[MAXGROUPS][MAXGROUPS];
u_int16_t	GroupTris[ MAXGROUPS ];

/*===================================================================
	Procedure	:		Dont Know....
	Input		:		Who can tell...
	Output		:		int93???
===================================================================*/
static bool read_visible( MLOADHEADER * Mloadheader, VISTREE *v, u_int16_t group, FileReader & in )
{
	bool ok;
	u_int16_t vnum;

	ok = true;
	u_int16_t portal = in.u16();
	if ( portal < MAXPORTALSPERGROUP ) // nb cannot check against Mloadheader->Group[group].num_portals yet...
	{
		v->portal = &Mloadheader->Group[group].Portal[portal];
		v->group = in.u16();
		if ( v->group < Mloadheader->num_groups )
		{
			v->num_visible = in.u16();
			if ( v->num_visible )
			{
				v->visible = (VISTREE *) calloc( v->num_visible, sizeof( VISTREE ) );
				if ( v->visible )
				{
					for ( vnum = 0; vnum < v->num_visible; vnum++ )
					{
						if ( !read_visible( Mloadheader, &v->visible[ vnum ], v->group, in ) )
						{
							ok = false;
							break;
						}
					}
				}
				else
					ok = false;
			}
			else
				v->visible = NULL;
		}
		else
			ok = false;
	}
	else
		ok = false;
	return ok;
}

/* Pull a triangle's texture coordinates in by UV_Fix (half a texel) where an
   edge runs along a line of constant u or v (within SAME_UV texels), so
   filtering doesn't sample the neighbouring area of the texture page across
   that edge. Tpage is the texture's index in Tloadheader (a loader's local
   tpage must go through its TloadIndex first); the step is measured in that
   texture's texels. A vertex moves at most one step from its original
   position (Orig_Vert), however many triangles share it. */
void FixUV( LPTRIANGLE Tri, LPLVERTEX Vert, u_int16_t Tpage, LPLVERTEX Orig_Vert )
{
	static LPLVERTEX TriVert[ 3 ], Orig_TriVert[ 3 ];
	static float u[ 3 ], v[ 3 ];
	float du = 0.0f, dv = 0.0f;
	int j = 0, k = 0;
	int zu = 0, zv = 0, uz = 0, vz = 0;
	float su = 0.0f, sv = 0.0f;
	int Xsize = 0, Ysize = 0;

	Xsize = Tloadheader.Xsize[Tpage];
	Ysize = Tloadheader.Ysize[Tpage];

	if( bSquareOnly )
	{
		if( Xsize != Ysize )
		{
			if( Xsize > Ysize )
			{
				Xsize = Ysize;
			}
			if( Ysize > Xsize )
			{
				Ysize = Xsize;
			}
		}
	}

	u[ 0 ] = Vert[ Tri->v1 ].tu;
	u[ 1 ] = Vert[ Tri->v2 ].tu;
	u[ 2 ] = Vert[ Tri->v3 ].tu;
	v[ 0 ] = Vert[ Tri->v1 ].tv;
	v[ 1 ] = Vert[ Tri->v2 ].tv;
	v[ 2 ] = Vert[ Tri->v3 ].tv;
	zu = zv = 0;
	for ( j = 0; j < 3; j++ )
	{
		k = ( j + 1 ) % 3;
		du = u[ k ] - u[ j ];
		dv = v[ k ] - v[ j ];
		if ( fabsf( du * Xsize ) < SAME_UV )
		{
			uz = j;
			zu++;
		}
		if ( fabsf( dv * Ysize ) < SAME_UV )
		{
			vz = j;
			zv++;
		}
	}
	TriVert[ 0 ] = &Vert[ Tri->v1 ];
	TriVert[ 1 ] = &Vert[ Tri->v2 ];
	TriVert[ 2 ] = &Vert[ Tri->v3 ];

	Orig_TriVert[ 0 ] = &Orig_Vert[ Tri->v1 ];
	Orig_TriVert[ 1 ] = &Orig_Vert[ Tri->v2 ];
	Orig_TriVert[ 2 ] = &Orig_Vert[ Tri->v3 ];

	du = ( UV_Fix / Xsize );
	dv = ( UV_Fix / Ysize );
	su = du * ( ( TriVert[ ( uz + 2 ) % 3 ]->tu > TriVert[ uz ]->tu ) ? 1 : -1 );
	sv = dv * ( ( TriVert[ ( vz + 2 ) % 3 ]->tv > TriVert[ vz ]->tv ) ? 1 : -1 );
	if ( fabsf( TriVert[ uz ]->tu + su - Orig_TriVert[ uz ]->tu ) <= du )
		TriVert[ uz ]->tu += su;
	k = ( uz + 1 ) % 3;
	if ( fabsf( TriVert[ k ]->tu + su - Orig_TriVert[ k ]->tu ) <= du )
		TriVert[ k ]->tu += su;
	if ( fabsf( TriVert[ vz ]->tv + sv - Orig_TriVert[ vz ]->tv ) <= dv )
		TriVert[ vz ]->tv += sv;
	k = ( vz + 1 ) % 3;
	if ( fabsf( TriVert[ k ]->tv + sv - Orig_TriVert[ k ]->tv ) <= dv )
		TriVert[ k ]->tv += sv;
}


void
FixUV_Anim( POLYANIM *PolyAnim, LPLVERTEX Vert, LPLVERTEX Orig_Vert )
{
	int vnum, frame, vi;
	TANIMUV *UV;

	UV = PolyAnim->UVs;
	for ( frame = 0; frame < PolyAnim->frames; frame++ )
	{
		for ( vnum = 0; vnum < PolyAnim->vertices; vnum++ )
		{
			vi = PolyAnim->vert[ vnum ];
			UV->u += Vert[ vi ].tu - Orig_Vert[ vi ].tu;
			UV->v += Vert[ vi ].tv - Orig_Vert[ vi ].tv;
			UV++;
		}
	}
}


/*===================================================================
	Procedure	:		Load .Mxv File
	Input		:		char	*	Filename , MLOADHEADER *
	Output		:		Nothing
===================================================================*/
bool Mload( char * Filename, MLOADHEADER * Mloadheader  )
{
	TRIANGLE		FacePnt; // was a pointer
	u_int16_t			exec_type;			// the type of execute buffer
	u_int16_t			num_vertices;		// overall number of verts
	u_int16_t			num_triangle_groups;// number of triangle groups
	u_int16_t			num_triangles;		// number of triangles in group
	u_int16_t			verts;
	u_int16_t			portal;
	LPLVERTEX	lpLVERTEX;
	LPLVERTEX	lpBufStart = NULL;
	WORD			*lpIndices = NULL;
	NORMAL * lpNormals = NULL;
	int				indexOffset = 0;
	int				ibIndex = 0;
	int				triangleCount = 0;
	int			i,e,o;
	u_int16_t tpage;
	int execbuf;
	int group;
	int r,g,b,a;
	COLOR color;
	u_int16_t	animation,frames,vertices;
	POLYANIM * PolyAnim;
	TANIMUV * TanimUV;
	int * intPnt;
	u_int16_t	num_animations;
	u_int16_t	animsize;
	u_int16_t	NumColours;
	u_int32_t	TempColour;
	TEXTUREANIMINFOINDEX * TAnimInfoIndexPnt;
	u_int16_t	polyanim;
	u_int16_t	initstate;
	int16_t	whenstoppedtriggermod;
	int colourkey = 0;

	DebugPrintf("Mload - %s\n",Filename);

	// Mloadheader is not valid until everything has been done..
	Mloadheader->state = false;

	if( Mloadheader->Buffer == NULL)
	{
		Msg( "Mload : Error Allocating Buffer\n" );
		return false;
	}
	FileReader in( Mloadheader->Buffer );

	/*	get the number of groups	*/
	Mloadheader->num_groups = in.u16();

	if ( Mloadheader->num_groups > MAXGROUPS )
	{
		Msg( "Mload : NumGroups > MAXGROUPS\n" );
		return false;
	}

	for( group = 0; group < Mloadheader->num_groups; group++)
	{
		GroupTris[ group ] = 0;
		/*	get the number of execbufs in this group	*/
		Mloadheader->Group[group].num_execbufs = in.u16();

		if ( Mloadheader->Group[group].num_execbufs > MAXEXECBUFSPERGROUP )
		{
			Msg( "Mload : NumExecBufs > MAXEXECBUFSPERGROUP\n" );
			return false;
		}
		
		for( execbuf = 0; execbuf < Mloadheader->Group[group].num_execbufs; execbuf++)
		{
			Mloadheader->Group[group].num_animating_polys[execbuf] = 0;
			
			in.skip( sizeof( int16_t ) );	// D3D execute-buffer size (unused)

			/*	get the type of execution buffer	*/
			exec_type = in.u16();
			/*	get the number of verts in execution buffer	*/
			num_vertices = in.u16();
	
			/*	record how what type of exec buffer	*/
			Mloadheader->Group[group].exec_type[execbuf] = exec_type;

			/*	record how many verts there are in the exec buffer	*/
			Mloadheader->Group[group].num_verts_per_execbuf[execbuf] = num_vertices;

			if (!FSCreateVertexBuffer((RENDEROBJECT*)&Mloadheader->Group[group].renderObject[execbuf], num_vertices))
			{
				Msg( "Mload : MakeExecBuffer Failed\n" );
				return false;
			}
			Mloadheader->Group[group].renderObject[execbuf].vertOwner = VERT_OWNER_LEVEL;

			DebugPrintf("created buffer to hold :%d verts\n", num_vertices);

			if (!(FSLockVertexBuffer((RENDEROBJECT*)&Mloadheader->Group[group].renderObject[execbuf], &lpLVERTEX)))
			{
				Msg( "Mload() lock failed in %s\n", Filename );
				return false;
			}

			lpBufStart = lpLVERTEX;

			/*	copy the vertex data into the execute buffer	*/
			for ( i=0 ; i<num_vertices; i++)
			{
				OLDLVERTEX old;
				in.bytes( &old, sizeof( old ) );

				lpLVERTEX[i].x = old.x;
				lpLVERTEX[i].y = old.y;
				lpLVERTEX[i].z = old.z;
				lpLVERTEX[i].tu = old.tu;
				lpLVERTEX[i].tv = old.tv;

				color = old.color;
				a = (color>>24)&255;
				r = (color>>16)&255;
				g = (color>>8)&255;
				b = color&255;

				a = 255;

				// right here you could override texture coloring if you wanted
				color = RGBA_MAKE( r , g , b , a  );
				lpLVERTEX[i].color = color;

				// the file's specular colour is unused (LVERTEX has no specular)
			}

			/* bjd - allows us to retrieve copies of the original vertices in the new format! */
			Mloadheader->Group[group].originalVerts[execbuf] = (LPLVERTEX)malloc(sizeof(LVERTEX) * num_vertices);
			memmove(Mloadheader->Group[group].originalVerts[execbuf], &lpLVERTEX[0], sizeof(LVERTEX) * num_vertices);//memcpy
	
			num_triangle_groups = in.u16();

			triangleCount = 0;

			/* find out how many vertexes we'll need to load into our vertex buffer */
			{
				FileReader scan = in;
				for ( i=0 ; i<num_triangle_groups; i++)
				{
					scan.skip( 4 * sizeof( u_int16_t ) );
					u_int16_t numTriangles = scan.u16();
					scan.skip( numTriangles * sizeof( MFACE ) );
					triangleCount += numTriangles;
				}
			}

			/*	create an index buffer	*/
			if (!FSCreateIndexBuffer((RENDEROBJECT*)&Mloadheader->Group[group].renderObject[execbuf], triangleCount * 3))
			{
				return false;
			}

			/*	lock the index buffer	*/
			if (!(FSLockIndexBuffer((RENDEROBJECT*)&Mloadheader->Group[group].renderObject[execbuf], &lpIndices)))
			{
				Msg( "Mload() lock failed in %s\n", Filename );
				return false;
			}

			if (!FSCreateNormalBuffer((RENDEROBJECT*)&Mloadheader->Group[group].renderObject[execbuf], triangleCount))
			{
				Msg( "Mload() failed to create normal buffer %s\n", Filename );
				return false;
			}

			if (!(FSLockNormalBuffer((RENDEROBJECT*)&Mloadheader->Group[group].renderObject[execbuf], &lpNormals)))
			{
				Msg( "Mload() failed to lock normal buffer %s\n", Filename );
				return false;
			}

			/* Allocate this execbuf's textureGroups[] from the TAG_LEVEL
			 * hunk, sized to the file's `num_triangle_groups` count.
			 * Bulk-freed in ReleaseView via Hunk_FreeAll(TAG_LEVEL). */
			if ( num_triangle_groups > 0 )
			{
				Mloadheader->Group[group].renderObject[execbuf].textureGroups =
				    (TEXTUREGROUP *) Hunk_Alloc(TAG_LEVEL,
				                                num_triangle_groups * sizeof(TEXTUREGROUP));
				if ( !Mloadheader->Group[group].renderObject[execbuf].textureGroups )
				{
					Msg( "Mload() hunk-alloc failed for textureGroups[%u] %s\n",
					     (unsigned)num_triangle_groups, Filename );
					return false;
				}
			}

			ibIndex = 0;
			indexOffset = 0;

			for ( i=0 ; i<num_triangle_groups; i++)
			{
				in.skip( sizeof( u_int16_t ) );	// texture type (unused)
				in.skip( sizeof( u_int16_t ) );	// group vertex start (unused)
				in.skip( sizeof( u_int16_t ) );	// group vertex count (unused)
				tpage = in.u16();
				num_triangles = in.u16();
				
				GroupTris[ group ] += num_triangles;
	
				/*	copy the faces data into the execute buffer	*/
				for( e=0; e<num_triangles; e++)
				{
					MFACE face;
					in.bytes( &face, sizeof( face ) );
					FacePnt.v1 = face.v1;
					FacePnt.v2 = face.v2;
					FacePnt.v3 = face.v3;

					lpIndices[ibIndex] = FacePnt.v1;
					ibIndex++;
					lpIndices[ibIndex] = FacePnt.v2;
					ibIndex++;
					lpIndices[ibIndex] = FacePnt.v3;
					ibIndex++;

					lpNormals->nx = face.nx;
					lpNormals->ny = face.ny;
					lpNormals->nz = face.nz;

					if ( face.pad & 1 )
					{
						// colourkey triangle found
						colourkey++;
					}

					FixUV( &FacePnt, lpBufStart, Mloadheader->TloadIndex[tpage], Mloadheader->Group[group].originalVerts[execbuf] );
					lpNormals++;
				}

				Mloadheader->Group[group].renderObject[execbuf].textureGroups[i].numVerts = num_vertices;
				Mloadheader->Group[group].renderObject[execbuf].textureGroups[i].numTriangles = num_triangles;

				/* keep track of our offset into our index buffer */
				Mloadheader->Group[ group ].renderObject[execbuf].textureGroups[i].startIndex = indexOffset;
				indexOffset += num_triangles * 3;

				Mloadheader->Group[group].renderObject[execbuf].textureGroups[i].texture = Tloadheader.lpTexture[Mloadheader->TloadIndex[tpage]];
				Mloadheader->Group[group].renderObject[execbuf].textureGroups[i].colourkey = Tloadheader.ColourKey[Mloadheader->TloadIndex[tpage]];


				{
					LEVELRENDEROBJECT * obj = &(Mloadheader->Group[group].renderObject[execbuf]);
					INCREASE_TEXTURE_GROUPS( obj );
				}
			}


			Mloadheader->Group[group].polyanim[execbuf] = NULL;
			Mloadheader->Group[group].num_animating_polys[execbuf] = 0;

			// this go here?
			/*	unlock the vertex buffer	*/
			if (!(FSUnlockVertexBuffer((RENDEROBJECT*)&Mloadheader->Group[group].renderObject[execbuf])))
			{
				Msg( "Mxload() vb unlock failed in %s\n", Filename );
				return false ;
			}

			/*	unlock the index buffer	*/
			if (!(FSUnlockIndexBuffer((RENDEROBJECT*)&Mloadheader->Group[group].renderObject[execbuf])))
			{
				Msg( "Mxload() ib unlock failed in %s\n", Filename );
				return false ;
			}

			if (!(FSUnlockNormalBuffer((RENDEROBJECT*)&Mloadheader->Group[group].renderObject[execbuf])))
			{
				Msg( "Mxload() normal unlock failed in %s\n", Filename );
				return false ;
			}
		}
	}
				
	
	// is there any vispoly info..... (a zero here is not consumed: it is
	// read again below as the animation count)
	if ( in.peek_u16() != 0)
	{
		u_int16_t pnum, vnum;

		if ( in.u16() != 2 )
		{
			Msg( "Mload : Not in new recursive format\n" );
			return false; // error if not in new recursive group/portal visibility tree format
		}

/*===================================================================
 *		Particular Stuff Related To Visiploys	
===================================================================*/
		for( group=0 ; group<Mloadheader->num_groups; group++)
		{
			/*	copy over the groups Name	*/
			strcpy( Mloadheader->Group[group].name, in.cstring() );

			// Make sure there are no start points initially in this group..
			Mloadheader->Group[group].StartPosInThisGroup = (u_int16_t) -1;

			in.bytes( &Mloadheader->Group[group].center, sizeof( VERT ) );
			in.bytes( &Mloadheader->Group[group].half_size, sizeof( VERT ) );

			/*	get the number of portals in the group	*/
			Mloadheader->Group[group].num_portals = in.u16();
			if(Mloadheader->Group[group].num_portals > MAXPORTALSPERGROUP)
			{
				Msg( "Mload : NumPortals > MAXPORTALSPERGROUP\n" );
				return false;    
			}
			
			Mloadheader->Group[group].Portal = (PORTAL*) malloc( Mloadheader->Group[group].num_portals * sizeof(PORTAL) );
			
			for( portal = 0 ; portal < Mloadheader->Group[group].num_portals ; portal ++ )
			{
				/*	get the number of Verts in the Portal	*/
				Mloadheader->Group[group].Portal[portal].num_vertices_in_portal = in.u16();

				if(Mloadheader->Group[group].Portal[portal].num_vertices_in_portal > MAXVERTSPERPORTAL)
				{
					Msg( "Mload : More than MAXVERTSPERPORTAL\n" );
					return false;  
				}
	
				{
					VERT sum = {0,0,0};
					VERT centroid = {0,0,0};
					int length = Mloadheader->Group[group].Portal[portal].num_vertices_in_portal;

					for( verts = 0 ; verts < length ; verts++ )
					{
						VERT vert;
						in.bytes( &vert, sizeof( vert ) );
						sum.x += vert.x;
						sum.y += vert.y;
						sum.z += vert.z;
						Mloadheader->Group[group].Portal[portal].Verts[verts] = vert;
					}

					centroid.x = sum.x / length;
					centroid.y = sum.y / length;
					centroid.z = sum.z / length;

					Mloadheader->Group[group].Portal[portal].centroid = centroid;

				}

				// get the polys in the portal
				Mloadheader->Group[group].Portal[portal].num_polys_in_portal = in.u16();

				if ( Mloadheader->Group[group].Portal[portal].num_polys_in_portal > MAXPOLYSPERPORTAL )
				{
					Msg( "Mload : More than MAXPOLYSPERPORTAL\n" );
					return false;
				}

				for ( pnum = 0; pnum < Mloadheader->Group[group].Portal[portal].num_polys_in_portal; pnum++ )
				{
					in.bytes( &Mloadheader->Group[group].Portal[portal].Poly[pnum], sizeof( MCFACE ) );
				}
			}
		
		}		
		// get the recursive group/portal visibility tree
		for( group=0 ; group<Mloadheader->num_groups; group++)
		{
			for( portal = 0 ; portal < Mloadheader->Group[group].num_portals ; portal ++ )
			{
				Mloadheader->Group[group].Portal[portal].visible.portal = &Mloadheader->Group[group].Portal[portal];
				Mloadheader->Group[group].Portal[portal].visible.group = in.u16();
				if ( Mloadheader->Group[group].Portal[portal].visible.group >= Mloadheader->num_groups )
				{
					Msg( "Mload : Group > Num groups\n" );
					return false;
				}
				Mloadheader->Group[group].Portal[portal].visible.num_visible = in.u16();
				if ( Mloadheader->Group[group].Portal[portal].visible.num_visible > 0 )
				{
					Mloadheader->Group[group].Portal[portal].visible.visible =
						(VISTREE *) calloc( Mloadheader->Group[group].Portal[portal].visible.num_visible, sizeof( VISTREE ) );
					if ( !Mloadheader->Group[group].Portal[portal].visible.visible )
					{
						Msg( "Mload : no visible portal\n" );
						return false;
					}

					for ( vnum = 0; vnum < Mloadheader->Group[group].Portal[portal].visible.num_visible; vnum++ )
					{
						if ( !read_visible( Mloadheader, &Mloadheader->Group[group].Portal[portal].visible.visible[vnum], 
								Mloadheader->Group[group].Portal[portal].visible.group, in ) )
						{
							Msg( "Mload : Read visible failed\n" );
							return false;
						}
					}
				}
				else
					Mloadheader->Group[group].Portal[portal].visible.visible = NULL;
			}
		}
/*===================================================================
 *		End Of Visiploy Stuff	
===================================================================*/

		if ( !ReadGroupConnections( Mloadheader, in ) )
			return false;
		ReadSoundInfo( Mloadheader, in );

/*===================================================================
 *		Start of Cell Index Stuff	
===================================================================*/
		// is there any cell index info..... (a zero here is not consumed: it
		// is read again below as the start position count)
		if ( in.peek_u16() != 0)
		{
			in.skip( sizeof( u_int16_t ) );

			Mloadheader->CellSize = 1.0F / in.f32();

			for( group=0 ; group<Mloadheader->num_groups; group++)
			{
				in.skip( sizeof( u_int16_t ) ); //dont need the number of exec buffs......
				for( execbuf=0 ; execbuf<Mloadheader->Group[group].num_execbufs; execbuf++)
				{
					Mloadheader->Group[group].cell_origin[execbuf] = in.vec();
					Mloadheader->Group[group].xcells[execbuf] = in.u16();
					Mloadheader->Group[group].ycells[execbuf] = in.u16();
					Mloadheader->Group[group].zcells[execbuf] = in.u16();
					Mloadheader->Group[group].numofcells[execbuf] =	Mloadheader->Group[group].xcells[execbuf] *
																	Mloadheader->Group[group].ycells[execbuf] *
																	Mloadheader->Group[group].zcells[execbuf];


					if ( !execbuf )
						Mloadheader->Group[group].colour_cell_pnt[0] = (COLOR*) malloc( Mloadheader->Group[group].numofcells[execbuf] * sizeof(COLOR) );
					else
						Mloadheader->Group[group].colour_cell_pnt[execbuf] = NULL;
					if( !Mloadheader->Group[group].colour_cell_pnt[0] )
					{
						Msg( "Mload : Couldnt allocate enough memory for the cell colour info\n" );
						return false;
					}
					Mloadheader->Group[group].num_vertex_indices[execbuf] = in.u16();

					Mloadheader->Group[group].vertex_index_pnt[execbuf] =
						(u_int16_t *) malloc( Mloadheader->Group[group].num_vertex_indices[execbuf] * sizeof( u_int16_t ) );
					Mloadheader->Group[group].vertex_cell_pnt[execbuf] =
						(VERTEXCELL *) malloc( Mloadheader->Group[group].numofcells[execbuf] * sizeof( VERTEXCELL ) );
					if( !Mloadheader->Group[group].vertex_index_pnt[execbuf] || !Mloadheader->Group[group].vertex_cell_pnt[execbuf] )
					{
						Msg( "Mload : Couldnt allocate enough memory for the cell index info\n" );
						return false;
					}
					in.bytes( Mloadheader->Group[group].vertex_index_pnt[execbuf], Mloadheader->Group[group].num_vertex_indices[execbuf] * sizeof( u_int16_t ) );
					in.bytes( Mloadheader->Group[group].vertex_cell_pnt[execbuf], Mloadheader->Group[group].numofcells[execbuf] * sizeof( VERTEXCELL ) );
				}
				in.bytes( Mloadheader->Group[group].colour_cell_pnt[0], Mloadheader->Group[group].numofcells[0] * sizeof( COLOR ) );
			}
		
		}
/*===================================================================
 *		Start of Start Pos Stuff
===================================================================*/
		num_start_positions = in.u16();

		for( i = 0 ; i < MAXGROUPS ; i++ )
		{
			FirstStartPositionInGroup[i] = (u_int16_t) -1;
		}
		for( i = 0 ; i < num_start_positions ; i++ )
		{
			STARTPOS start;
			in.bytes( &start, sizeof( start ) );
			StartPositions[i].Pos = start.Pos;
			StartPositions[i].Dir = start.Dir;
			StartPositions[i].Up = start.Up;
			StartPositions[i].Group = start.Group;
			//Make a note in each group of which Start point is in there...
			Mloadheader->Group[StartPositions[i].Group].StartPosInThisGroup = i;
			if( FirstStartPositionInGroup[StartPositions[i].Group] == (u_int16_t) -1 )
			{
				StartPositions[i].NextInGroup = (u_int16_t) -1;
				FirstStartPositionInGroup[StartPositions[i].Group] = i;
			}else{
				StartPositions[i].NextInGroup = FirstStartPositionInGroup[StartPositions[i].Group];
				FirstStartPositionInGroup[StartPositions[i].Group] = i;
			}
		}

/*===================================================================
 *		Start of group up vector Stuff
===================================================================*/
		// is there any up vector info..... (a zero here is not consumed: it is
		// read again below as the animation count)
		if ( in.peek_u16() != 0)
		{
			in.skip( sizeof( u_int16_t ) );
			for ( i = 0; i < Mloadheader->num_groups; i++ )
			{
				Mloadheader->Group[ i ].up = in.vec();
			}
		}
		else
		{
			// no up vector info -> use default (0,1,0)
			for ( i = 0; i < Mloadheader->num_groups; i++ )
			{
				Mloadheader->Group[ i ].up.x = 0.0F;
				Mloadheader->Group[ i ].up.y = 1.0F;
				Mloadheader->Group[ i ].up.z = 0.0F;
			}
		}

	}

	// Background Animation Stuff......
	num_animations = in.u16();
	Mloadheader->AnimData.num_animations = num_animations;
	
	if( num_animations )
	{
		for( i = 0 ; i < num_animations ; i++ )
		{
			in.skip( sizeof( u_int16_t ) );	// frames (unused)
			in.skip( sizeof( u_int16_t ) );	// max loops (unused)
			animsize = in.u16();
			Mloadheader->AnimData.AnimSeq[i] = (u_int16_t *) malloc( animsize * sizeof( u_int16_t ) );
			in.bytes( Mloadheader->AnimData.AnimSeq[i], animsize * sizeof( u_int16_t ) );
		}
		
		for( group=0 ; group<Mloadheader->num_groups; group++)
		{
			for( execbuf=0 ; execbuf<Mloadheader->Group[group].num_execbufs; execbuf++)
			{
				// Animating background polys......		
				Mloadheader->Group[group].num_animating_polys[execbuf] = in.u16();
				if( Mloadheader->Group[group].num_animating_polys[execbuf] != 0 )
				{
					Mloadheader->Group[group].polyanim[execbuf] = (POLYANIM*)malloc( Mloadheader->Group[group].num_animating_polys[execbuf] * sizeof( POLYANIM ) );
		
		
					PolyAnim = Mloadheader->Group[group].polyanim[execbuf];
					if( !PolyAnim )
					{
						Msg( "Mload : No PolyAnim\n" );
						return false;
					}
					if (!(FSLockVertexBuffer((RENDEROBJECT*)&Mloadheader->Group[ group ].renderObject[execbuf], &lpLVERTEX)))
					{
						Msg( "Mload : Lock VertexBuffer failed\n" );
						return false;
					}

					for( i = 0 ; i < Mloadheader->Group[group].num_animating_polys[execbuf] ; i++ )
					{
						animation = in.u16();
						frames = in.u16();
						vertices = in.u16();
		
		
						PolyAnim->State = TEXTUREANIM_Go;
						PolyAnim->CurrentTime = 0.0F;
						PolyAnim->MasterTime = 0.0F;
						PolyAnim->CurrentOffset = 0;
						PolyAnim->currentframe = (u_int16_t) -1;
						PolyAnim->newframe = 0;
		
						PolyAnim->animation =animation;
						PolyAnim->frames	=frames;
						PolyAnim->vertices	=vertices;
		
						PolyAnim->vert = NULL;
						PolyAnim->UVs = NULL;
						
						PolyAnim->vert = (int *) malloc( vertices * sizeof(int) );
						if( !PolyAnim->vert)
						{
							Msg( "Mload : PolyAnim vert is null\n" );
							return false;
						}
						PolyAnim->UVs  = (TANIMUV *) malloc( vertices * frames * sizeof(TANIMUV) );
						if( !PolyAnim->UVs)
						{
							Msg( "Mload : PolyAnim uv is null\n" );
							return false;
						}
		
						intPnt = PolyAnim->vert;
						TanimUV = PolyAnim->UVs;
		
						for( e = 0 ; e < vertices  ; e++ )
						{
							*intPnt++ = in.u16();
						}
		
						for( o = 0 ; o < vertices*frames ; o++ )
						{
							TanimUV->u = in.f32();
							TanimUV->v = in.f32();
							TanimUV++;
						}
						FixUV_Anim( PolyAnim, lpLVERTEX, Mloadheader->Group[group].originalVerts[execbuf] );
						PolyAnim++;
					}

					if (!(FSUnlockVertexBuffer((RENDEROBJECT*)&Mloadheader->Group[group].renderObject[execbuf])))
					{
						Msg( "Mload : Unlock VertexBuffer failed\n" );
						return false;
					}
				}
			}
		}
	}

	NumColours = in.u16();

	if( NumColours )
	{
		for( group=0; group < Mloadheader->num_groups; group++ )
		{
			TempColour = in.u32();
			Mloadheader->Group[group].BGClear_Flag	= (int16_t) ( ( TempColour >> 24 ) & 255 );
			Mloadheader->Group[group].BGClear_Red	= ( ( ( (float) ( ( TempColour >> 16 ) & 255 ) ) ) / 255.0F );
			Mloadheader->Group[group].BGClear_Green = ( ( ( (float) ( ( TempColour >> 8 ) & 255 ) ) ) / 255.0F );
			Mloadheader->Group[group].BGClear_Blue	= ( ( ( (float) ( ( TempColour >> 0 ) & 255 ) ) ) / 255.0F );
		}
	}
	else
	{
		for( group=0; group < Mloadheader->num_groups; group++ )
		{
			Mloadheader->Group[group].BGClear_Flag = 0;
			Mloadheader->Group[group].BGClear_Red	= 0.0F;
			Mloadheader->Group[group].BGClear_Green = 0.0F;
			Mloadheader->Group[group].BGClear_Blue	= 0.0F;
		}
	}

	
	// Background Poly Info Index Animation Stuff......
	Mloadheader->AnimData.TAnimInfoIndex = NULL;
	Mloadheader->AnimData.num_animating_polys = 0;
	if( in.u16() )
	{
		Mloadheader->AnimData.num_animating_polys = in.u16();
		Mloadheader->AnimData.TAnimInfoIndex = ( TEXTUREANIMINFOINDEX * ) malloc( Mloadheader->AnimData.num_animating_polys * sizeof(TEXTUREANIMINFOINDEX) );
	   
		TAnimInfoIndexPnt = Mloadheader->AnimData.TAnimInfoIndex;
		if( !TAnimInfoIndexPnt )
		{
			Msg( "Mload : Texture Animation Index failed mallco\n" );
			return false;
		}
		for( i = 0 ; i < Mloadheader->AnimData.num_animating_polys ; i++ )
		{
			group = in.u16();
			execbuf = in.u16();
			polyanim = in.u16();
			initstate = in.u16();
			whenstoppedtriggermod = in.i16();

			PolyAnim = Mloadheader->Group[group].polyanim[execbuf];
			PolyAnim += polyanim;
			PolyAnim->State = initstate;
			PolyAnim->WhenStoppedTriggerMod = whenstoppedtriggermod;


			TAnimInfoIndexPnt->Group = group;
			TAnimInfoIndexPnt->Execbuf = execbuf;
			TAnimInfoIndexPnt->PolyAnim = polyanim;
			TAnimInfoIndexPnt->InitState = initstate;
			TAnimInfoIndexPnt++;
		}
	}

	/* Everything the level keeps has been copied out of the file. */
	free( Mloadheader->OrgAddr );
	Mloadheader->OrgAddr = NULL;
	Mloadheader->Buffer = NULL;

	DebugPrintf( "Mload: %d colourkey triangles found\n", colourkey );
	// Mloadheader is valid and can be executed...
	Mloadheader->state = true;


	return( true );
}

/*===================================================================
	Procedure	:		Execute all group buffers for a Mloadheader
	Input		;		MLOADHEADER *
	Output		:		FLASE/true
===================================================================*/

bool ExecuteMloadHeader( MLOADHEADER * Mloadheader  )
{
	int group;
	if (Mloadheader->state == true )
	{
		for ( group=0 ; group<Mloadheader->num_groups ; group++)
		{
			ExecuteSingleGroupMloadHeader( Mloadheader, group );
		}
	}
	return true;
}
/*===================================================================
	Procedure	:		Execute one group for an Mloadheader
	Input		;		MLOADHEADER *
	Output		:		FLASE/true
===================================================================*/

// bjd - this function seems to draw static geometry?

bool ExecuteSingleGroupMloadHeader( MLOADHEADER * Mloadheader, u_int16_t group  )
{
	u_int16_t i;
	RENDERMATRIX Matrix;
	
	if (Mloadheader->state == true )
	{
		for ( i=0 ; i<Mloadheader->Group[group].num_execbufs; i++)
		{
			if( Mloadheader->Group[group].exec_type[i]&HASTRANSPARENCIES )
			{
				if (!FSGetWorld(&Matrix))
				{
					return false;
				}
				AddTransExe(
					&Matrix,
					(RENDEROBJECT*)&Mloadheader->Group[group].renderObject[i],
					0,
					(u_int16_t) -1,
					(u_int16_t) group,
					Mloadheader->Group[ group ].num_verts_per_execbuf[i] 
				);
			}
			else
			{
				if (!draw_object((RENDEROBJECT*)&Mloadheader->Group[group].renderObject[i]))
				{
					return false;
				}
			}
		}
	}
	return true;
}


void ReleaseVisible( VISTREE *v )
{
	int i;

	if ( v->visible )
	{
		for ( i = 0; i < v->num_visible; i++ )
		{
			ReleaseVisible( &v->visible[ i ] );
		}
		free( v->visible );
		v->visible = NULL;
	}
}


/*
 * ReleaseMloadheader
 * Release Execute buffers
 */
void
ReleaseMloadheader( MLOADHEADER * Mloadheader )
{
    int i,e;
    int group;
	POLYANIM * PolyAnim;

	if ( !Mloadheader->state )
	{
		DebugPrintf("mload release: not freeing header because !state\n");
		return;
	}

	FreeGroupConnections();

#ifdef __3DS__
	/* The backend's persistent cache holds converted copies of these
	 * execbufs; free it with them, not after the next level has loaded. */
	c3d_release_level_cache();
#endif

    	for (group = 0; group < Mloadheader->num_groups; group++)
	{
		for ( i = 0; i < Mloadheader->Group[ group ].num_portals; i++ )
		{
			ReleaseVisible( &Mloadheader->Group[ group ].Portal[ i ].visible );
		}

		free( Mloadheader->Group[group].Portal );

		for (i = 0; i < Mloadheader->Group[group].num_execbufs; i++)
		{
			FSReleaseRenderObject((RENDEROBJECT*)&(Mloadheader->Group[group].renderObject[i]));

			if ( Mloadheader->Group[group].colour_cell_pnt[i] )
				free( Mloadheader->Group[group].colour_cell_pnt[i] );
			Mloadheader->Group[group].colour_cell_pnt[i] = NULL;
			free( Mloadheader->Group[group].vertex_index_pnt[i] );
			Mloadheader->Group[group].vertex_index_pnt[i] = NULL;
			free( Mloadheader->Group[group].vertex_cell_pnt[i] );
			Mloadheader->Group[group].vertex_cell_pnt[i] = NULL;

			/* Per-execbuf vertex copy allocated at mload.c:507. Without
			 * this free the buffer leaks every level transition — about
			 * 1–2 MB per level depending on geometry density, which
			 * accumulates and exhausts the OG 3DS 24 MB malloc heap by
			 * the fifth level load. */
			if ( Mloadheader->Group[group].originalVerts[i] )
			{
				free( Mloadheader->Group[group].originalVerts[i] );
				Mloadheader->Group[group].originalVerts[i] = NULL;
			}

			PolyAnim = Mloadheader->Group[group].polyanim[i];
			
			for( e = 0 ; e < Mloadheader->Group[group].num_animating_polys[i] ; e++ )
			{	
			   	if( PolyAnim )
			   	{
			   		if( PolyAnim->vert )
			   		{
			   			free( PolyAnim->vert );
			   			PolyAnim->vert = NULL;
			   		}
			   		if( PolyAnim->UVs )
			   		{
			   			free( PolyAnim->UVs );
			   			PolyAnim->UVs = NULL;
			   		}
			   	}
				PolyAnim++;
			}

			if (Mloadheader->Group[group].polyanim[i])
		   		free(Mloadheader->Group[group].polyanim[i]);
			
			Mloadheader->Group[group].polyanim[i] = NULL;
		}
	}

	for( i = 0 ; i < Mloadheader->AnimData.num_animations ; i++ )
	{
		free(Mloadheader->AnimData.AnimSeq[i]);
		Mloadheader->AnimData.AnimSeq[i] = NULL;
	}

	if( Mloadheader->AnimData.TAnimInfoIndex )
	{
		free(Mloadheader->AnimData.TAnimInfoIndex);
		Mloadheader->AnimData.TAnimInfoIndex = NULL;
	}
	
	if ( Mloadheader->OrgAddr )
	{
		free( Mloadheader->OrgAddr );
		Mloadheader->OrgAddr = NULL;
	}

	memset( Mloadheader, 0, sizeof(MLOADHEADER) );

	Mloadheader->state = false;
}

/*===================================================================
	Procedure	:		Pre - Load .Mx File
	Input		:		char	*	Filename , MLOADHEADER *
	Output		:		Nothing
===================================================================*/
bool PreMload( char * Filename, MLOADHEADER * Mloadheader  )
{
	long			File_Size;
	long			Read_Size;
	char		*	Buffer;
	int				i;
	u_int32_t			MagicNumber;
	u_int32_t			VersionNumber;
	int8_t			TempFilename[ 256 ];

	DebugPrintf("Loading Level: %s\n",Filename);

	// Mloadheader is not valid until everything has been done..
	Mloadheader->state = false;
	Mloadheader->Buffer = NULL;

	File_Size = Get_File_Size( Filename );

	if ( !File_Size )
	{
	 	Msg( "PreMLoad : File_Size is zero\n" );
		return false;
	}

	Buffer = (char*)calloc( 1, File_Size + sizeof( int ) );

	if( Buffer == NULL )
	{
	 	Msg( "PreMLoad : Unable to allocate buffer\n" );
		return( false );
	}

	Read_Size = Read_File( Filename, Buffer, File_Size );

	if( Read_Size != File_Size )
	{
	 	Msg( "PreMLoad : Didn't read all of file\n" );
		return( false );
	}

	Mloadheader->OrgAddr = Buffer;

	FileReader in( Buffer );
	MagicNumber = in.u32();
	VersionNumber = in.u32();

	if( ( MagicNumber != MAGIC_NUMBER ) || ( VersionNumber != MXV_VERSION_NUMBER  ) )
	{
		Msg( "PreMload() Incompatible level( .MXV ) file %s", Filename );
		return( false );
	}

	Mloadheader->num_texture_files = in.u16();

	if ( Mloadheader->num_texture_files > MAXTPAGESPERMLOAD )
	{
	 	Msg( "PreMLoad : Too many texture pages\n" );
		return false;
	}

	if ( Mloadheader->num_texture_files !=0)
	{
		/*	put the filenames in the tex struct	*/
		for( i = 0;	 i < Mloadheader->num_texture_files; i++)
		{
			strcpy( Mloadheader->ImageFile[i], in.cstring() );

			GetLevelTexturePath( (char*)&TempFilename[ 0 ], &Mloadheader->ImageFile[i][0], &ShortLevelNames[ LevelNum ][ 0 ] );

			Mloadheader->TloadIndex[i] = AddTexture( &Tloadheader , (char*)&TempFilename[ 0 ], true , false , true, 0, 0 );

			if( Mloadheader->TloadIndex[i] == -1 )
			{
			 	Msg( "PreMLoad : Too many TPages\n" );
				return( false );
			}
		}

	}

	Mloadheader->Buffer = (const char *) in.pos();
	return( true );
}

/*===================================================================
	Procedure	:		Handle All Animation For an Mloadheader
	Input		:		MLOADHEADER *
	Output		:		Nothing
===================================================================*/
void BackGroundTextureAnimation( MLOADHEADER * Mloadheader , u_int16_t group )
{
	u_int16_t		*	Uint16Pnt;
	int	i;
	int	execbuf;
	POLYANIM * PolyAnim;
	u_int16_t * AnimData;
	float	ExtraTime;
	
	if( AnimOncePerFrame == Mloadheader->Group[group].AnimOncePerFrame )
		return;
	Mloadheader->Group[group].AnimOncePerFrame = AnimOncePerFrame;
	
	for( execbuf=0 ; execbuf<Mloadheader->Group[group].num_execbufs; execbuf++)
	{
		PolyAnim = Mloadheader->Group[group].polyanim[execbuf];
		
		for( i = 0 ; i < Mloadheader->Group[group].num_animating_polys[execbuf] ; i++ )
		{
			
			if( ( PolyAnim->State == TEXTUREANIM_Go ) &&
				( PolyAnim->CurrentOffset != (u_int16_t) -1 ) &&
				( ( PolyAnim->CurrentTime -= framelag ) <= 0.0F ) )
			{
				ExtraTime = PolyAnim->CurrentTime;
				// time to tickover...
				while( (PolyAnim->State==TEXTUREANIM_Go) && (PolyAnim->CurrentTime<0.0F) )
				{
					// Take account of the overflow just incase that means another frame need to be displayed....
					PolyAnim->CurrentTime = PolyAnim->MasterTime + ExtraTime;
					ExtraTime += PolyAnim->MasterTime;
					if( ExtraTime > 0.0F )
						ExtraTime = 0.0F;
					AnimData = ( Mloadheader->AnimData.AnimSeq[PolyAnim->animation] + PolyAnim->CurrentOffset );
					Uint16Pnt = HandleAnimCommands( PolyAnim , AnimData , Mloadheader->AnimData.AnimSeq[PolyAnim->animation] );
					if ( Uint16Pnt )
						PolyAnim->CurrentOffset = Uint16Pnt - Mloadheader->AnimData.AnimSeq[PolyAnim->animation];
					else
					{
						// animation has reached the end of its sequence
						PolyAnim->CurrentOffset = (u_int16_t) -1;
						if ( PolyAnim->WhenStoppedTriggerMod != -1 )
							ApplyTriggerMod( &TrigMods[ PolyAnim->WhenStoppedTriggerMod ] );
					}
					smemo_invalidate(); /* shared UVs just stepped */
				}
				
			}
			PolyAnim++;
		}
	}
}
/*===================================================================
	Procedure	:		Handle All Animation Commands
	Input		:		MLOADHEADER *
	Output		:		Nothing
===================================================================*/
u_int16_t * HandleAnimCommands( POLYANIM * PolyAnim , u_int16_t * AnimData , u_int16_t * OrgAnimAdr)
{
	u_int16_t Frame;
	u_int16_t LoopDepth;
	while( ( Frame = *AnimData++ ) >= TEXANIM_COMMAND )
	{
		switch( Frame )
		{
		case TEXANIM_COMMAND:
			break;
		case TEXANIM_DURATION:
			PolyAnim->MasterTime = (float) (*AnimData++) * 0.06F;
			PolyAnim->CurrentTime = PolyAnim->MasterTime;
			break;
		case TEXANIM_DURATION1:
			PolyAnim->CurrentTime = (float) (*AnimData++) * 0.06F;
			break;
		case TEXANIM_LOOPINIT:
			LoopDepth = *AnimData++;
			PolyAnim->loopstack[LoopDepth] = *AnimData++;
			break;
		case TEXANIM_LOOPEND:
			LoopDepth = *AnimData++;
			if( (PolyAnim->loopstack[LoopDepth] == 0xffff ) || ( (PolyAnim->loopstack[LoopDepth] -= 1 ) != 0 ) )
			{
				AnimData = OrgAnimAdr + *AnimData;
			}else{
 				AnimData++;
			}
			break;
		case TEXANIM_END:
			PolyAnim->State = TEXTUREANIM_Stop;
			return NULL;
			break;
		}
	}
	PolyAnim->newframe = Frame;
 	return AnimData;
}


/*===================================================================
	Procedure	:		Set up Sound Info...
	Input		:		MLOADHEADER *
	Output		:		Nothing
===================================================================*/
void InitSoundInfo( MLOADHEADER * /*Mloadheader*/ )
{
}


static void ReadSoundInfo( MLOADHEADER *m, FileReader & in )
{
	int j, k;

	for ( j = 0; j < m->num_groups; j++ )
	{
		for ( k = 0; k < m->num_groups; k++ )
		{
			SoundInfo[ j ][ k ] = in.f32();
		}
	}
}

/*===================================================================
	Procedure	:		Trigger Background Animation to change state...
	Input		:		u_int16_t * Data
				:		first u_int16_t is the Polyanim index number...
				:		the second u_int16_t is the state to change that anim to
	Output		:		Nothing
===================================================================*/
void TriggerBackgroundAnimationGo( u_int16_t * Data )
{
	POLYANIM * PolyAnim;
	TEXTUREANIMINFOINDEX * TAnimInfoIndexPnt;

	TAnimInfoIndexPnt = Mloadheader.AnimData.TAnimInfoIndex;
	if( !TAnimInfoIndexPnt )
		return;
	TAnimInfoIndexPnt += *Data;
	PolyAnim = Mloadheader.Group[TAnimInfoIndexPnt->Group].polyanim[TAnimInfoIndexPnt->Execbuf];
	PolyAnim += TAnimInfoIndexPnt->PolyAnim;
	PolyAnim->State = TEXTUREANIM_Go;
}
/*===================================================================
	Procedure	:		Trigger Background Animation to change state...
	Input		:		u_int16_t * Data
				:		first u_int16_t is the Polyanim index number...
				:		the second u_int16_t is the state to change that anim to
	Output		:		Nothing
===================================================================*/
void TriggerBackgroundAnimationStop( u_int16_t * Data )
{
	POLYANIM * PolyAnim;
	TEXTUREANIMINFOINDEX * TAnimInfoIndexPnt;

	TAnimInfoIndexPnt = Mloadheader.AnimData.TAnimInfoIndex;
	if( !TAnimInfoIndexPnt )
		return;
	TAnimInfoIndexPnt += *Data;
	PolyAnim = Mloadheader.Group[TAnimInfoIndexPnt->Group].polyanim[TAnimInfoIndexPnt->Execbuf];
	PolyAnim += TAnimInfoIndexPnt->PolyAnim;
	PolyAnim->State = TEXTUREANIM_Stop;
}

FILE *LoadTextureAnimations( FILE *f )
{
	u_int16_t group;
	int	execbuf;
	int	i;
	POLYANIM * PolyAnim;

	for ( group = 0; group < Mloadheader.num_groups; group++ )
	{
		for( execbuf=0 ; execbuf<Mloadheader.Group[group].num_execbufs; execbuf++)
		{
			PolyAnim = Mloadheader.Group[group].polyanim[execbuf];
			
			for( i = 0 ; i < Mloadheader.Group[group].num_animating_polys[execbuf] ; i++ )
			{
				fread( &PolyAnim->State, sizeof( PolyAnim->State ), 1, f );
				fread( &PolyAnim->MasterTime, sizeof( PolyAnim->MasterTime ), 1, f );
				fread( &PolyAnim->CurrentTime, sizeof( PolyAnim->CurrentTime ), 1, f );
				fread( &PolyAnim->CurrentOffset, sizeof( PolyAnim->CurrentOffset ), 1, f );
				fread( &PolyAnim->newframe, sizeof( PolyAnim->newframe ), 1, f );
				fread( PolyAnim->loopstack, sizeof( PolyAnim->loopstack ), 1, f );
				PolyAnim++;
			}
		}
	}
	return f;
}

FILE *SaveTextureAnimations( FILE *f )
{
	u_int16_t group;
	int	execbuf;
	int	i;
	POLYANIM * PolyAnim;

	for ( group = 0; group < Mloadheader.num_groups; group++ )
	{
		for( execbuf=0 ; execbuf<Mloadheader.Group[group].num_execbufs; execbuf++)
		{
			PolyAnim = Mloadheader.Group[group].polyanim[execbuf];
			
			for( i = 0 ; i < Mloadheader.Group[group].num_animating_polys[execbuf] ; i++ )
			{
				fwrite( &PolyAnim->State, sizeof( PolyAnim->State ), 1, f );
				fwrite( &PolyAnim->MasterTime, sizeof( PolyAnim->MasterTime ), 1, f );
				fwrite( &PolyAnim->CurrentTime, sizeof( PolyAnim->CurrentTime ), 1, f );
				fwrite( &PolyAnim->CurrentOffset, sizeof( PolyAnim->CurrentOffset ), 1, f );
				fwrite( &PolyAnim->newframe, sizeof( PolyAnim->newframe ), 1, f );
				fwrite( PolyAnim->loopstack, sizeof( PolyAnim->loopstack ), 1, f );
				PolyAnim++;
			}
		}
	}
	return f;
}

