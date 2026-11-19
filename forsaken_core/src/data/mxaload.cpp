/* mxaload.cpp -- loads animated (.mxa) models
 *
 * PreMxaload reads the file and queues its textures; Mxaload builds the
 * render objects the same way as mxload.cpp (vertex colours are forced
 * opaque; StoreTriangles keeps the triangles for the title screen) and reads the animation frames into one block, anim_verts, that
 * frame_pnts point into, plus the fire points and spot effects. The file is
 * freed at the end of Mxaload. InterpFrames blends two frames into the
 * vertex buffer each time the model's animation moves: position and UV
 * into the live vertices, colour into originalVerts, per vertex as its
 * MXA_ANIM_* flags say.
 *
 * .mxa layout (little-endian, packed; read through FileReader, counts u16):
 *
 *   magic, version (u32 each)
 *   num_texture_files, texture names (NUL-terminated)       -- PreMxaload
 *   num_groups
 *   per group: num_meshes
 *     per mesh:
 *       execute-buffer size (unused), mesh_type (bit 0: has
 *       transparencies), num_vertices
 *       OLDLVERTEX[num_vertices]       x y z, reserved, colour, specular, tu tv
 *       num_texture_groups
 *       per texture group:
 *         texture type (unused), first vertex, vertex count (unused), tpage,
 *         num_triangles, MFACE[num_triangles]  v1 v2 v3, flags (bit 0:
 *         colour-keyed), face normal
 *   format flag: 2 = animation frames follow (any other value is not
 *       consumed: it is read again as the fire point flag)
 *   if 2: num_frames; if num_frames > 1, per frame, per group, per mesh,
 *       per texture group: count, MXAVERT[count]  x y z, colour, specular,
 *       tu tv, flags (MXA_ANIM_POS / RGB / UV)
 *   fire point flag: if non-zero,
 *     num_fire_points, per point: id, pos, dir, up (VECTORs)
 *     num_spot_fx, per effect: type, pos, dir, up (VECTORs), start /
 *       active / inactive delays (float seconds), primary and secondary
 *       weapon (i16), colour (u32), sound name (NUL-terminated), sound
 *       volume and speed (float)
 */

/*===================================================================
		Include File...
===================================================================*/
#include <stdio.h>
#include "game/main.h"
#include "math/new3d.h"
#include "math/quat.h"
#include "game/compobjects.h"
#include "game/bgobjects.h"
#include "game/object.h"
#include "net/networking.h"
#include "game/triggers.h"
#include "game/pickups.h"
#include "data/mxload.h"
#include "data/mxaload.h"

#include "audio/sfx.h"
#include "game/spotfx.h"
#include "util/util.h"
#include "game/oct2.h"
#include "data/mload.h"
#include "data/tload.h"
#include "game/ships.h"
#include "util/hunk_alloc.h"
#include "util/file_reader.h"

/*===================================================================
		Defines
===================================================================*/
constexpr int MXA_VERSION_NUMBER = 2;


/*===================================================================
		Globals...
===================================================================*/
/* Allocated with ModelHeaders by PreInitModel; see models.cpp. */
MXALOADHEADER *MxaModelHeaders = nullptr;

/*===================================================================
	Procedure	:		Load .Mx File
	Input		:		char	*	Filename , MXALOADHEADER *
	Output		:		Nothing
===================================================================*/
bool Mxaload( char * Filename, MXALOADHEADER * Mxaloadheader, bool StoreTriangles  )
{

	TRIANGLE		FacePnt; // was a pointer
	TRIANGLE	*TempFacePnt; ZERO_STACK_MEM(TempFacePnt);
	u_int16_t			mesh_type;			// the mesh's type (bit 0: has transparencies)
	u_int16_t			num_vertices;		// overall number of verts
	u_int16_t			num_texture_groups;// number of triangle groups
	u_int16_t			num_triangles;		// number of triangles in group
	u_int16_t			group_vertex_start; // where in the vert list to start processing
	LVERTEX	*LVert;
	WORD			*Indices = nullptr;
	NORMAL		*Normals = nullptr;
	int				ibIndex = 0;
	u_int16_t			frame;
	u_int16_t			texgroup;
	int				i,e;
	u_int16_t			tpage = 0;
	int				mesh;
	int				group;

	u_int16_t			NumSpotFX;
	PVSPOTFX *		SpotFXPtr;
	u_int16_t			NumFirePoints;
	PVFIREPOINT *	FirePointPtr;
	u_int32_t			Colour;
	int				colourkey = 0;
	int8_t			SFXFilename[ 128 ];

	int triangleCount = 0;
	int indexOffset = 0;
	int tempInt;

	// Mxaloadheader is not valid until everything has been done..
	Mxaloadheader->state = false;
	Mxaloadheader->WantedFrame = 0;
	Mxaloadheader->CurrentFrame = 0;
	Mxaloadheader->Interp = 0.0F;
	Mxaloadheader->Time = 0.0F;

	DebugPrintf("MxaLoad\n");

	if( Mxaloadheader->Buffer == nullptr)
	{
		Msg( "Mxaload() Unable to allocate buffer in %s\n", Filename );
		return false;
	}
	FileReader in( Mxaloadheader->Buffer );

	/*	get the number of groups	*/
	Mxaloadheader->num_groups = in.u16();

	if ( Mxaloadheader->num_groups > MAXGROUPS )
	{
		Msg( "Mxaload() num_groups > MAXGROUPS\n", Filename );
		return false;
	}

	for( group=0 ; group<Mxaloadheader->num_groups; group++)
	{
		/*	get the number of meshes in this group	*/
		Mxaloadheader->Group[group].num_meshes = in.u16();

		if ( Mxaloadheader->Group[group].num_meshes > MAXMESHESPERGROUP )
		{
			Msg( "Mxaload() num_meshes > MAXMESHESPERGROUP\n", Filename );
			return false;
		}

		for( mesh=0 ; mesh<Mxaloadheader->Group[group].num_meshes; mesh++)
		{
			in.skip( sizeof( u_int16_t ) );	// Direct3D execute-buffer size (unused)

			/*	get the type of execution buffer	*/
			mesh_type = in.u16();
			/*	get the number of verts in execution buffer	*/
			num_vertices = in.u16();

			/*	record the mesh's type	*/
			Mxaloadheader->Group[group].mesh_type[mesh] = mesh_type;

			/*	record how many verts the mesh has	*/
			Mxaloadheader->Group[group].num_verts_per_mesh[mesh] = num_vertices;

			/*	create a vertex buffer	*/
			if (!FSCreateVertexBuffer(&Mxaloadheader->Group[group].renderObject[mesh], num_vertices))
			{
				return false;
			}
			Mxaloadheader->Group[group].renderObject[mesh].vertOwner = VERT_OWNER_MXA;

			DebugPrintf("created buffer to hold :%d verts\n", num_vertices);

			/*	lock the vertex buffer	*/
			if (!(FSLockVertexBuffer(&Mxaloadheader->Group[group].renderObject[mesh], &LVert)))
			{
				Msg( "Mxload() lock failed in %s\n", Filename );
				return false;
			}


			/*	copy the vertices into the vertex buffer	*/
			for ( i=0 ; i<num_vertices; i++)
			{
				OLDLVERTEX old;
				in.bytes( &old, sizeof( old ) );

				LVert[i].x = old.x;
				LVert[i].y = old.y;
				LVert[i].z = old.z;
				LVert[i].tu = old.tu;
				LVert[i].tv = old.tv;
				LVert[i].color = old.color;

				LVert[i].color |= 0xFF000000;
				// the file's specular colour is unused (LVERTEX has no specular)
			}

			/* Keep the vertices as loaded: lighting restores baked colours from them each frame. */
			Mxaloadheader->Group[group].originalVerts[mesh] = (LVERTEX *)malloc(sizeof(LVERTEX) * num_vertices);
			memmove(Mxaloadheader->Group[group].originalVerts[mesh], &LVert[0], sizeof(LVERTEX) * num_vertices);//memcpy


			num_texture_groups = in.u16();
			Mxaloadheader->Group[group].num_texture_groups[mesh] = num_texture_groups;

			/* find out how many vertexes we'll need to load into our vertex buffer */
			triangleCount = 0;
			tempInt = 0;

			{
				FileReader scan = in;
				for ( i=0 ; i<num_texture_groups; i++)
				{
					scan.skip( 4 * sizeof( u_int16_t ) );
					u_int16_t numTriangles = scan.u16();
					scan.skip( numTriangles * sizeof( MFACE ) );
					triangleCount += numTriangles;
				}
			}

			/*	create an index buffer	*/
			if (!FSCreateIndexBuffer(&Mxaloadheader->Group[group].renderObject[mesh], triangleCount * 3))
			{
				return false;
			}

			DebugPrintf("created index buffer to hold :%d incidices\n", triangleCount * 3);

			/*	lock the index buffer	*/
			if (!(FSLockIndexBuffer(&Mxaloadheader->Group[group].renderObject[mesh], &Indices)))
			{
				Msg( "Mxload() lock failed in %s\n", Filename );
				return false;
			}

			if (!FSCreateNormalBuffer(&Mxaloadheader->Group[group].renderObject[mesh], triangleCount))
			{
				DebugPrintf("Mxload() failed to create normal buffer in %s\n", Filename);
				return false;
			}
			if (!(FSLockNormalBuffer(&Mxaloadheader->Group[group].renderObject[mesh], &Normals)))
			{
				Msg( "Mxload() normal lock failed in %s\n", Filename );
				return false;
			}

			/* Allocate this mesh's textureGroups[] from the TAG_LEVEL
			 * hunk, sized to num_texture_groups. Bulk-freed via
			 * Hunk_FreeAll(TAG_LEVEL) on level transition. */
			if ( num_texture_groups > 0 )
			{
				Mxaloadheader->Group[group].renderObject[mesh].textureGroups =
					(TEXTUREGROUP *) Hunk_Alloc(TAG_LEVEL,
												num_texture_groups * sizeof(TEXTUREGROUP));
				if ( !Mxaloadheader->Group[group].renderObject[mesh].textureGroups )
				{
					Msg( "Mxaload() hunk-alloc failed for textureGroups[%u] %s\n",
						 (unsigned)num_texture_groups, Filename );
					return false;
				}
			}

			ibIndex = 0;
			indexOffset = 0;

			for ( i=0 ; i<num_texture_groups; i++)
			{
				in.skip( sizeof( u_int16_t ) );	// texture type (unused)
				group_vertex_start = in.u16();
				in.skip( sizeof( u_int16_t ) );	// group vertex count (unused)
				tpage = in.u16();
				num_triangles = in.u16();

				Mxaloadheader->Group[group].texture_group_vert_off[mesh][i] = (u_int32_t) (group_vertex_start*sizeof(LVERTEX));

				/* the index buffer holds every triangle in order, so StoreTriangles copies it from the start */
				TempFacePnt = (TRIANGLE *) /*LVert*/Indices;

				/*	copy the triangles into the index and normal buffers	*/
				for( e=0; e<num_triangles; e++)
				{
					MFACE face;
					in.bytes( &face, sizeof( face ) );
					FacePnt.v1 = face.v1;
					FacePnt.v2 = face.v2;
					FacePnt.v3 = face.v3;

					Indices[ibIndex] = FacePnt.v1;
					ibIndex++;
					Indices[ibIndex] = FacePnt.v2;
					ibIndex++;
					Indices[ibIndex] = FacePnt.v3;
					ibIndex++;

					Normals->nx = face.nx;
					Normals->ny = face.ny;
					Normals->nz = face.nz;

					if ( face.pad & 1 )
					{
						// colourkey triangle found
						colourkey++;
					}
					Normals++;
					tempInt+=3;
				}

				DebugPrintf("put %d verts into VB\n", tempInt);

				// BJD - check this. correct value to place here?
				Mxaloadheader->Group[ group ].renderObject[mesh].textureGroups[i].numVerts = num_vertices;//num_triangles * 3;
				Mxaloadheader->Group[ group ].renderObject[mesh].textureGroups[i].numTriangles = num_triangles;

				/* keep track of our offset into our index buffer */
				Mxaloadheader->Group[ group ].renderObject[mesh].textureGroups[i].startIndex = indexOffset;
				indexOffset += num_triangles * 3;

				Mxaloadheader->Group[ group ].renderObject[mesh].textureGroups[i].texture = Tloadheader.TexHandle[Mxaloadheader->TloadIndex[tpage]];
				Mxaloadheader->Group[ group ].renderObject[mesh].textureGroups[i].colourkey = Tloadheader.ColourKey[Mxaloadheader->TloadIndex[tpage]];
			}

			if (StoreTriangles)
			{	Mxaloadheader->Group[group].num_polys_per_mesh[mesh] = /*num_triangles*/triangleCount;
				Mxaloadheader->Group[group].poly_ptr[mesh] = (TRIANGLE *)malloc( sizeof (TRIANGLE) * /*num_triangles*/triangleCount);

				memmove(Mxaloadheader->Group[group].poly_ptr[mesh], TempFacePnt, sizeof (TRIANGLE) * /*num_triangles*/triangleCount);//memcpy
			}
			else
			{
				Mxaloadheader->Group[group].num_polys_per_mesh[mesh] = 0;
				Mxaloadheader->Group[group].poly_ptr[mesh] = nullptr;
			}

			if (!(FSUnlockVertexBuffer(&Mxaloadheader->Group[group].renderObject[mesh])))
			{
				Msg( "Mxaload() unlock failed in %s\n", Filename );
				return false ;
			}

			/*	unlock the index buffer	*/
			if (!(FSUnlockIndexBuffer(&Mxaloadheader->Group[group].renderObject[mesh])))
			{
				Msg( "Mxload() ib unlock failed in %s\n", Filename );
				return false ;
			}

			if (!(FSUnlockNormalBuffer(&Mxaloadheader->Group[group].renderObject[mesh])))
			{
				Msg( "Mxload() normal unlock failed in %s\n", Filename );
				return false ;
			}

			Mxaloadheader->Group[group].renderObject[mesh].numTextureGroups = num_texture_groups;

		}
	}


	// is there any vispoly info..... (any other value is not consumed: it
	// is read again below as the fire point flag)
	if ( in.peek_u16() == 2)				// 2 means .mxa format
	{
		in.skip( sizeof( u_int16_t ) );
/*===================================================================
 *		Particular Stuff Related Anims
===================================================================*/
		Mxaloadheader->num_frames = in.u16();

		if ( Mxaloadheader->num_frames > 1 )
		{
			if ( !( Mxaloadheader->num_anim_vertices = (MXAVERTCOUNT *) calloc( Mxaloadheader->num_frames, sizeof( MXAVERTCOUNT ) ) ) )
			{
				Msg( "Mxaload() calloc(num_frames=%d) failed in %s\n", Mxaloadheader->num_frames, Filename );
				return false;
			}
			if ( !( Mxaloadheader->frame_pnts = (MXAVERTFRAME *) calloc( Mxaloadheader->num_frames, sizeof( MXAVERTFRAME ) ) ) )
			{
				Msg( "Mxaload() calloc(num_frames=%d) failed in %s\n", Mxaloadheader->num_frames, Filename );
				return false;
			}

			/* Each frame stores, per texture group, a vertex count and that
			   many MXAVERTs. Size all of them first, then copy them into one
			   block (anim_verts) that frame_pnts point into. */
			size_t total = 0;
			{
				FileReader scan = in;
				for( frame=0 ; frame<Mxaloadheader->num_frames; frame++)
					for( group=0 ; group<Mxaloadheader->num_groups; group++)
						for( mesh=0 ; mesh<Mxaloadheader->Group[group].num_meshes; mesh++)
							for( texgroup=0; texgroup < Mxaloadheader->Group[group].num_texture_groups[mesh] ; texgroup++)
							{
								u_int16_t count = scan.u16();
								scan.skip( count * sizeof( MXAVERT ) );
								total += count;
							}
			}

			MXAVERT * dst = nullptr;
			if ( total )
			{
				dst = (MXAVERT *) malloc( total * sizeof( MXAVERT ) );
				if ( !dst )
				{
					Msg( "Mxaload() malloc(%u frame verts) failed in %s\n", (unsigned) total, Filename );
					return false;
				}
			}
			Mxaloadheader->anim_verts = dst;

			for( frame=0 ; frame<Mxaloadheader->num_frames; frame++)
				for( group=0 ; group<Mxaloadheader->num_groups; group++)
					for( mesh=0 ; mesh<Mxaloadheader->Group[group].num_meshes; mesh++)
						for( texgroup=0; texgroup < Mxaloadheader->Group[group].num_texture_groups[mesh] ; texgroup++)
						{
							u_int16_t count = in.u16();
							Mxaloadheader->num_anim_vertices[frame][group][mesh][texgroup] = count;
							Mxaloadheader->frame_pnts[frame][group][mesh][texgroup] = dst;
							in.bytes( dst, count * sizeof( MXAVERT ) );
							dst += count;
						}
		}
		else
		{
			Mxaloadheader->num_frames = 0;
			Mxaloadheader->num_anim_vertices = nullptr;
			Mxaloadheader->frame_pnts = nullptr;
		}
	}

/*===================================================================
	Point Direction Data
===================================================================*/
	if( in.u16() )
	{
		NumFirePoints = in.u16();

		if( NumFirePoints )
		{
			Mxaloadheader->NumFirePoints = NumFirePoints;
			Mxaloadheader->FirePoints = (PVFIREPOINT *) malloc( NumFirePoints * sizeof( PVFIREPOINT ) );

			if( !Mxaloadheader->FirePoints )
			{
				Mxaloadheader->NumFirePoints = 0;
				return( false );
			}

			FirePointPtr = Mxaloadheader->FirePoints;

			for( i = 0; i < NumFirePoints; i++ )
			{
				FirePointPtr->ID = in.u16();					// ID ( u_int16_t )
				FirePointPtr->Pos = in.vec();			// Pos ( 3 floats )
				FirePointPtr->Dir = in.vec();			// Dir ( 3 floats )
				FirePointPtr->Up = in.vec();			// Up ( 3 floats )
				FirePointPtr++;
			}
		}
		else
		{
			Mxaloadheader->FirePoints = nullptr;
			Mxaloadheader->NumFirePoints = 0;
		}

		NumSpotFX = in.u16();

		if( NumSpotFX )
		{
			Mxaloadheader->NumSpotFX = NumSpotFX;
			Mxaloadheader->SpotFX = (PVSPOTFX *) malloc( NumSpotFX * sizeof( PVSPOTFX ) );

			if( !Mxaloadheader->SpotFX )
			{
				Mxaloadheader->NumSpotFX = 0;
				return( false );
			}

			SpotFXPtr = Mxaloadheader->SpotFX;

			for( i = 0; i < NumSpotFX; i++ )
			{
				SpotFXPtr->Type = in.u16();					// Type ( u_int16_t )
				SpotFXPtr->Pos = in.vec();			// Pos ( 3 floats )
				SpotFXPtr->DirVector = in.vec();	// Dir ( 3 floats )
				SpotFXPtr->UpVector = in.vec();	// Up ( 3 floats )
				SpotFXPtr->StartDelay = in.f32() * ANIM_SECOND;		// Start Delay
				SpotFXPtr->ActiveDelay = in.f32() * ANIM_SECOND;	// Active Delay
				SpotFXPtr->InactiveDelay = in.f32() * ANIM_SECOND;	// Inactive Delay
				SpotFXPtr->Primary = (int8_t) in.i16();		// Primary ( int16_t )
				SpotFXPtr->Secondary = (int8_t) in.i16();	// Secondary ( int16_t )
				Colour = in.u32();

				strcpy( (char*)&SFXFilename[ 0 ], in.cstring() );

				if( SpotFXPtr->Type == SPOTFX_SoundFX )
				{
					if( ( SpotFXPtr->ActiveDelay == 0.0F ) && ( SpotFXPtr->InactiveDelay == 0.0F ) )
					{
						Msg( "PointDirSpotfx (SoundFX) has on and off time of 0" );
						SpotFXPtr->SoundFX = -1;
					}
					else
					{
						SpotFXPtr->SoundFX = ReturnSFXIndex( (char*)&SFXFilename[ 0 ] );
					}
				}
				else
				{
					SpotFXPtr->SoundFX = -1;
				}

				SpotFXPtr->SoundFXVolume = in.f32();
				SpotFXPtr->SoundFXSpeed = in.f32();

				SpotFXPtr->Red = (u_int8_t) ( ( Colour >> 16 ) & 255 );
				SpotFXPtr->Green = (u_int8_t) ( ( Colour >> 8 ) & 255 );
				SpotFXPtr->Blue = (u_int8_t) ( Colour & 255 );
				SpotFXPtr++;
			}
		}
		else
		{
			Mxaloadheader->SpotFX = nullptr;
			Mxaloadheader->NumSpotFX = 0;
		}
	}
	else
	{
		Mxaloadheader->NumFirePoints = 0;
		Mxaloadheader->FirePoints = nullptr;
		Mxaloadheader->NumSpotFX = 0;
		Mxaloadheader->SpotFX = nullptr;
	}

	/* Everything the model keeps has been copied out of the file. */
	free( Mxaloadheader->OrgAddr );
	Mxaloadheader->OrgAddr = nullptr;
	Mxaloadheader->Buffer = nullptr;

	if ( colourkey )
		DebugPrintf( "Mxaload: %d colourkey triangles found\n", colourkey );
	// Mxaloadheader is valid and can be executed...
	Mxaloadheader->state = true;
	return( true );
}

/*===================================================================
	Procedure	:		Execute all group buffers for a Mxaloadheader
	Input		;		MXALOADHEADER *
	Output		:		FALSE/true
===================================================================*/

bool ExecuteMxaloadHeader( MXALOADHEADER * Mxaloadheader, u_int16_t in_group  )
{
	int i;
	int group;
	RENDERMATRIX Matrix;
	if (Mxaloadheader->state == true )
	{
		for ( group=0 ; group<Mxaloadheader->num_groups ; group++)
		{
			for ( i=0 ; i<Mxaloadheader->Group[group].num_meshes; i++)
			{
				if( Mxaloadheader->Group[group].mesh_type[i]&HASTRANSPARENCIES )
				{
					if (!FSGetWorld(&Matrix))
					{
						return false;
					}
					AddTransExe( &Matrix , &Mxaloadheader->Group[group].renderObject[i] , 0, INDEX_NONE, in_group, Mxaloadheader->Group[ group ].num_verts_per_mesh[i] );
				}
				else
				{
#ifdef __3DS__
					{ c3d_set_object_proctex(true); }
#endif
					bool ok = draw_object(&Mxaloadheader->Group[group].renderObject[i]);
#ifdef __3DS__
					{ c3d_set_object_proctex(false); }
#endif
					if (!ok)
					{
						return false;
					}
				}
			}
		}
	}
	return true;
}

/*
 * ReleaseMxaloadheader
 * Release the render objects and everything the loader allocated
 */

void ReleaseMxaloadheader( MXALOADHEADER * Mxaloadheader )
{
	int i;
	int group;

	if ( !Mxaloadheader->state )
		return;
	for (group = 0; group < Mxaloadheader->num_groups; group++)
	{
		for (i = 0; i < Mxaloadheader->Group[group].num_meshes; i++)
		{
			FSReleaseRenderObject(&Mxaloadheader->Group[group].renderObject[i]);
			if (Mxaloadheader->Group[group].poly_ptr[i])
			{
				free(Mxaloadheader->Group[group].poly_ptr[i]);
				Mxaloadheader->Group[group].poly_ptr[i] = nullptr;
			}

			/* Per-mesh vertex copy from mxaload.cpp — same leak
			 * pattern as mload/mxload, fixed there too. */
			if (Mxaloadheader->Group[group].originalVerts[i])
			{
				free(Mxaloadheader->Group[group].originalVerts[i]);
				Mxaloadheader->Group[group].originalVerts[i] = nullptr;
			}
		}
	}

	if ( Mxaloadheader->OrgAddr )
	{
		free( Mxaloadheader->OrgAddr );
		Mxaloadheader->OrgAddr = nullptr;
	}

	if( Mxaloadheader->SpotFX && Mxaloadheader->NumSpotFX )
	{
		free( Mxaloadheader->SpotFX );
		Mxaloadheader->SpotFX = nullptr;
		Mxaloadheader->NumSpotFX = 0;
	}

	if( Mxaloadheader->FirePoints && Mxaloadheader->NumFirePoints )
	{
		free( Mxaloadheader->FirePoints );
		Mxaloadheader->FirePoints = nullptr;
		Mxaloadheader->NumFirePoints = 0;
	}

	if ( Mxaloadheader->num_frames > 1 )
	{
		if ( Mxaloadheader->num_anim_vertices )
			free( Mxaloadheader->num_anim_vertices );
		Mxaloadheader->num_anim_vertices = nullptr;

		if ( Mxaloadheader->frame_pnts )
			free ( Mxaloadheader->frame_pnts );
		Mxaloadheader->frame_pnts = nullptr;

		free( Mxaloadheader->anim_verts );
		Mxaloadheader->anim_verts = nullptr;
	}

	Mxaloadheader->state = false;
}

/*===================================================================
	Procedure	:		Pre - Load .Mxa File
	Input		:		char	*	Filename , MXALOADHEADER *
	Output		:		Nothing
===================================================================*/
bool PreMxaload( char * Filename, MXALOADHEADER * Mxaloadheaders, int header_num, bool LevelSpecific )
{
	long			File_Size;
	long			Read_Size;
	char		*	Buffer;
	int			i;
	MXALOADHEADER * Mxaloadheader;
	u_int32_t			MagicNumber;
	u_int32_t			VersionNumber;
	int8_t			TempFilename[ 256 ];

	Mxaloadheader = &Mxaloadheaders[header_num];

	// Mxaloadheader is not valid until everything has been done..
	Mxaloadheader->state = false;
	Mxaloadheader->Buffer = nullptr;

	File_Size = Get_File_Size( Filename );

	if( !File_Size )
	{
		Msg( "PreMxaload() File %s not found\n", Filename );
		return( false );
	}

	Buffer = (char*)calloc( 1, File_Size + 32 );

	if( Buffer == nullptr )
	{
		Msg( "PreMxaload() Unable to allocate buffer in %s\n", Filename );
		return( false );
	}

	Read_Size = Read_File( Filename, Buffer, File_Size );

	if( Read_Size != File_Size )
	{
		Msg( "PreMxaload() Error reading %s\n", Filename );
		return( false );
	}

	Mxaloadheader->OrgAddr = Buffer;

	FileReader in( Buffer );
	MagicNumber = in.u32();
	VersionNumber = in.u32();

	if( ( MagicNumber != MAGIC_NUMBER ) || ( VersionNumber != MXA_VERSION_NUMBER  ) )
	{
		Msg( "PreMxaload() Incompatible model( .MXA ) file %s", Filename );
		return( false );
	}

	Mxaloadheader->num_texture_files = in.u16();

	if ( Mxaloadheader->num_texture_files > MAXTPAGESPERMXALOAD )
	{
		Msg( "PreMxaload() Too many textures in mxamodel\n%s\n", Filename );
		return false;
	}

	if ( Mxaloadheader->num_texture_files !=0)
	{
		/*	put the filenames in the tex struct	*/
		for( i = 0;	 i < Mxaloadheader->num_texture_files; i++)
		{
			strcpy( Mxaloadheader->ImageFile[i], in.cstring() );

			if( LevelSpecific )
				GetLevelTexturePath( (char*)&TempFilename[ 0 ], &Mxaloadheader->ImageFile[i][0], &ShortLevelNames[ LevelNum ][ 0 ] );
			else
				sprintf( (char*)&TempFilename[ 0 ], "data\\textures\\%s", &Mxaloadheader->ImageFile[i][0] );

			if (Mxaloadheader->AllocateTPage & LOAD_TPAGES)
				Mxaloadheader->TloadIndex[i] = AddTexture( &Tloadheader , (char*)&TempFilename[0] , true ,false, 0, 0 );

			if( Mxaloadheader->TloadIndex[i] == -1 )
			{
				Msg( "PreMxaload() Too many TPages\n" );
				return false;
			}
		}
	}

	Mxaloadheader->Buffer = (const char *) in.pos();

	return true;
}

/*===================================================================
	Procedure	:		Interp between frames for MXA Anim...
	Input		:		MXALOADHEADER * Mxaloadheader
				:		int FrameFrom, int FrameTo , float Interp
	Output		:		FALSE/true
===================================================================*/
bool	InterpFrames( MXALOADHEADER * Mxaloadheader , int FromFrame, int ToFrame , float Interp )
{
	LVERTEX	*BufStart = nullptr;
	LVERTEX	*LVert;
	LVERTEX	*OrigLVert;
	MXAVERT *		FromVert;
	MXAVERT *		ToVert;
	u_int16_t			texgroup;
	u_int16_t			num_anim_vertices;
	int mesh;
	int group;
	COLOR_RGBA * color;
	COLOR_RGBA * from_color;
	COLOR_RGBA * to_color;

	if ( Mxaloadheader->num_frames <= 1 ) return true;

	for( group=0 ; group<Mxaloadheader->num_groups; group++)
	{
		for( mesh=0 ; mesh<Mxaloadheader->Group[group].num_meshes; mesh++)
		{
			if (!(FSLockVertexBuffer(&Mxaloadheader->Group[group].renderObject[mesh], &BufStart)))
			{
				return false;
			}

			for( texgroup=0; texgroup < Mxaloadheader->Group[group].num_texture_groups[mesh] ; texgroup++)
			{
				/* A mesh's vertex list starts with the 8 corners of its
				   bounding box (written by the 1998 exporter for Direct3D's
				   off-screen test; no triangle uses them), and the file's
				   texture-group start vertex counts from after them. So the
				   group's first real vertex, where its frame data begins, is
				   start + 8. Checked against every shipped .mxa: all 726
				   meshes have the box, every group's triangles lie in
				   start + 8 .. start + 8 + count, and frame 0 matches the
				   base vertices at this offset. (Some Remaster-era .mx
				   models have no box, but .mx models never come through
				   here.) */
				LVert = 	( LVERTEX *) ( (char*) BufStart+( (u_int32_t) Mxaloadheader->Group[group].texture_group_vert_off[mesh][texgroup]));
				LVert += 8;

				OrigLVert = ( LVERTEX *) ( (char*) Mxaloadheader->Group[group].originalVerts[mesh] + ( (u_int32_t) Mxaloadheader->Group[group].texture_group_vert_off[mesh][texgroup]));
				OrigLVert += 8;

				FromVert = ( MXAVERT * ) Mxaloadheader->frame_pnts[FromFrame][group][mesh][texgroup];
				ToVert = ( MXAVERT * ) Mxaloadheader->frame_pnts[ToFrame][group][mesh][texgroup];

				for( num_anim_vertices = 0 ; num_anim_vertices <  Mxaloadheader->num_anim_vertices[FromFrame][group][mesh][texgroup] ; num_anim_vertices++ )
				{
					if ( FromVert->flags & MXA_ANIM_POS )
					{
						LVert->x = FromVert->x + ( ToVert->x - FromVert->x ) * Interp;
						LVert->y = FromVert->y + ( ToVert->y - FromVert->y ) * Interp;
						LVert->z = FromVert->z + ( ToVert->z - FromVert->z ) * Interp;
					}
					if ( FromVert->flags & MXA_ANIM_RGB )
					{
						color = (COLOR_RGBA *) &OrigLVert->color;


						from_color = (COLOR_RGBA *) &FromVert->color;
						to_color = (COLOR_RGBA *) &ToVert->color;
						color->r = from_color->r + (int8_t) fast_floorf( ( to_color->r - from_color->r ) * Interp );
						color->g = from_color->g + (int8_t) fast_floorf( ( to_color->g - from_color->g ) * Interp );
						color->b = from_color->b + (int8_t) fast_floorf( ( to_color->b - from_color->b ) * Interp );
					}
					if ( FromVert->flags & MXA_ANIM_UV )
					{
						LVert->tu = FromVert->tu + ( ToVert->tu - FromVert->tu ) * Interp;
						LVert->tv = FromVert->tv + ( ToVert->tv - FromVert->tv ) * Interp;
					}

					LVert++;
					OrigLVert++;
					FromVert++;
					ToVert++;
				}
			}
			if (!(FSUnlockVertexBuffer(&Mxaloadheader->Group[group].renderObject[mesh])))
			{
				return false;
			}
		}
	}

	return true;
}
