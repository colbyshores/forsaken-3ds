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
 *   per group: num_execbufs
 *     per execbuf:
 *       execute-buffer size (unused), exec_type (bit 0: has
 *       transparencies), num_vertices
 *       OLDLVERTEX[num_vertices]       x y z, reserved, colour, specular, tu tv
 *       num_texture_groups
 *       per texture group:
 *         texture type (unused), first vertex, vertex count (unused), tpage,
 *         num_triangles, MFACE[num_triangles]  v1 v2 v3, flags (bit 0:
 *         colour-keyed), face normal
 *   format flag: 2 = animation frames follow (any other value is not
 *       consumed: it is read again as the fire point flag)
 *   if 2: num_frames; if num_frames > 1, per frame, per group, per execbuf,
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
#define	MXA_VERSION_NUMBER	2


/*===================================================================
		Globals...	
===================================================================*/
/* Pointer to a TAG_LEVEL hunk allocation. See ModelHeaders comment
 * in models.c — same migration, same rationale. */
MXALOADHEADER *MxaModelHeaders = NULL;

/*===================================================================
	Procedure	:		Load .Mx File
	Input		:		char	*	Filename , MXALOADHEADER *
	Output		:		Nothing
===================================================================*/
bool Mxaload( char * Filename, MXALOADHEADER * Mxaloadheader, bool StoreTriangles  )
{

	TRIANGLE		FacePnt; // was a pointer
	LPTRIANGLE	TempFacePnt; ZERO_STACK_MEM(TempFacePnt);
	u_int16_t			exec_type;			// the type of execute buffer
	u_int16_t			num_vertices;		// overall number of verts
	u_int16_t			num_texture_groups;// number of triangle groups
	u_int16_t			num_triangles;		// number of triangles in group
	u_int16_t			group_vertex_start; // where in the vert list to start processing
	LPLVERTEX	lpLVERTEX;
	WORD			*lpIndices = NULL;
	NORMAL		*lpNormals = NULL;
	int				ibIndex = 0;
	u_int16_t			frame;
	u_int16_t			texgroup;
	int				i,e;
	u_int16_t			tpage = 0;
	int				execbuf;
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

	if( Mxaloadheader->Buffer == NULL)
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
		/*	get the number of execbufs in this group	*/
		Mxaloadheader->Group[group].num_execbufs = in.u16();

		if ( Mxaloadheader->Group[group].num_execbufs > MAXEXECBUFSPERGROUP )
		{
			Msg( "Mxaload() num_execbufs > MAXEXECBUFSPERGROUP\n", Filename );
			return false;
		}

		for( execbuf=0 ; execbuf<Mxaloadheader->Group[group].num_execbufs; execbuf++)
		{
			in.skip( sizeof( u_int16_t ) );	// D3D execute-buffer size (unused)

			/*	get the type of execution buffer	*/
			exec_type = in.u16();
			/*	get the number of verts in execution buffer	*/
			num_vertices = in.u16();

			/*	record how what type of exec buffer	*/
			Mxaloadheader->Group[group].exec_type[execbuf] = exec_type;

			/*	record how many verts there are in the exec buffer	*/
			Mxaloadheader->Group[group].num_verts_per_execbuf[execbuf] = num_vertices;

			/*	create a vertex buffer	*/
			if (!FSCreateVertexBuffer(&Mxaloadheader->Group[group].renderObject[execbuf], num_vertices))
			{
				return false;
			}
			Mxaloadheader->Group[group].renderObject[execbuf].vertOwner = VERT_OWNER_MXA;

			DebugPrintf("created buffer to hold :%d verts\n", num_vertices);

			/*	lock the vertex buffer	*/
			if (!(FSLockVertexBuffer(&Mxaloadheader->Group[group].renderObject[execbuf], &lpLVERTEX)))
			{
				Msg( "Mxload() lock failed in %s\n", Filename );
				return false;
			}


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
				lpLVERTEX[i].color = old.color;

				lpLVERTEX[i].color |= 0xFF000000;
				// the file's specular colour is unused (LVERTEX has no specular)
			}

			/* bjd - allows us to retrieve copies of the original vertices in the new format! */
			Mxaloadheader->Group[group].originalVerts[execbuf] = (LPLVERTEX)malloc(sizeof(LVERTEX) * num_vertices);
			memmove(Mxaloadheader->Group[group].originalVerts[execbuf], &lpLVERTEX[0], sizeof(LVERTEX) * num_vertices);//memcpy
	
			
			num_texture_groups = in.u16();
			Mxaloadheader->Group[group].num_texture_groups[execbuf] = num_texture_groups;

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
			if (!FSCreateIndexBuffer(&Mxaloadheader->Group[group].renderObject[execbuf], triangleCount * 3))
			{
				return false;
			}

			DebugPrintf("created index buffer to hold :%d incidices\n", triangleCount * 3);

			/*	lock the index buffer	*/
			if (!(FSLockIndexBuffer(&Mxaloadheader->Group[group].renderObject[execbuf], &lpIndices)))
			{
				Msg( "Mxload() lock failed in %s\n", Filename );
				return false;
			}

			if (!FSCreateNormalBuffer(&Mxaloadheader->Group[group].renderObject[execbuf], triangleCount))
			{
				DebugPrintf("Mxload() failed to create normal buffer in %s\n", Filename);
				return false;
			}
			if (!(FSLockNormalBuffer(&Mxaloadheader->Group[group].renderObject[execbuf], &lpNormals)))
			{
				Msg( "Mxload() normal lock failed in %s\n", Filename );
				return false;
			}

			/* Allocate this execbuf's textureGroups[] from the TAG_LEVEL
			 * hunk, sized to num_texture_groups. Bulk-freed via
			 * Hunk_FreeAll(TAG_LEVEL) on level transition. */
			if ( num_texture_groups > 0 )
			{
				Mxaloadheader->Group[group].renderObject[execbuf].textureGroups =
				    (TEXTUREGROUP *) Hunk_Alloc(TAG_LEVEL,
				                                num_texture_groups * sizeof(TEXTUREGROUP));
				if ( !Mxaloadheader->Group[group].renderObject[execbuf].textureGroups )
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
	
				Mxaloadheader->Group[group].texture_group_vert_off[execbuf][i] = (u_int32_t) (group_vertex_start*sizeof(LVERTEX));

				/* bjd - CHECK - might need to be start of buffer */
				TempFacePnt = (LPTRIANGLE ) /*lpLVERTEX*/lpIndices;

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
					lpNormals++;
					tempInt+=3;
				}

				DebugPrintf("put %d verts into VB\n", tempInt);

				// BJD - check this. correct value to place here?
				Mxaloadheader->Group[ group ].renderObject[execbuf].textureGroups[i].numVerts = num_vertices;//num_triangles * 3;
				Mxaloadheader->Group[ group ].renderObject[execbuf].textureGroups[i].numTriangles = num_triangles;

				/* keep track of our offset into our index buffer */
				Mxaloadheader->Group[ group ].renderObject[execbuf].textureGroups[i].startIndex = indexOffset;
				indexOffset += num_triangles * 3;

				Mxaloadheader->Group[ group ].renderObject[execbuf].textureGroups[i].texture = Tloadheader.lpTexture[Mxaloadheader->TloadIndex[tpage]];
				Mxaloadheader->Group[ group ].renderObject[execbuf].textureGroups[i].colourkey = Tloadheader.ColourKey[Mxaloadheader->TloadIndex[tpage]];
			}

			if (StoreTriangles)
			{	Mxaloadheader->Group[group].num_polys_per_execbuf[execbuf] = /*num_triangles*/triangleCount;
				Mxaloadheader->Group[group].poly_ptr[execbuf] = (LPTRIANGLE)malloc( sizeof (TRIANGLE) * /*num_triangles*/triangleCount);			
			
				memmove(Mxaloadheader->Group[group].poly_ptr[execbuf], TempFacePnt, sizeof (TRIANGLE) * /*num_triangles*/triangleCount);//memcpy
			}
			else
			{
				Mxaloadheader->Group[group].num_polys_per_execbuf[execbuf] = 0;
				Mxaloadheader->Group[group].poly_ptr[execbuf] = NULL;			
			}

			if (!(FSUnlockVertexBuffer(&Mxaloadheader->Group[group].renderObject[execbuf])))
			{
				Msg( "Mxaload() unlock failed in %s\n", Filename );
				return false ;
			}

			/*	unlock the index buffer	*/
			if (!(FSUnlockIndexBuffer(&Mxaloadheader->Group[group].renderObject[execbuf])))
			{
				Msg( "Mxload() ib unlock failed in %s\n", Filename );
				return false ;
			}

			if (!(FSUnlockNormalBuffer(&Mxaloadheader->Group[group].renderObject[execbuf])))
			{
				Msg( "Mxload() normal unlock failed in %s\n", Filename );
				return false ;
			}

			Mxaloadheader->Group[group].renderObject[execbuf].numTextureGroups = num_texture_groups;

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
						for( execbuf=0 ; execbuf<Mxaloadheader->Group[group].num_execbufs; execbuf++)
							for( texgroup=0; texgroup < Mxaloadheader->Group[group].num_texture_groups[execbuf] ; texgroup++)
							{
								u_int16_t count = scan.u16();
								scan.skip( count * sizeof( MXAVERT ) );
								total += count;
							}
			}

			MXAVERT * dst = NULL;
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
					for( execbuf=0 ; execbuf<Mxaloadheader->Group[group].num_execbufs; execbuf++)
						for( texgroup=0; texgroup < Mxaloadheader->Group[group].num_texture_groups[execbuf] ; texgroup++)
						{
							u_int16_t count = in.u16();
							Mxaloadheader->num_anim_vertices[frame][group][execbuf][texgroup] = count;
							Mxaloadheader->frame_pnts[frame][group][execbuf][texgroup] = dst;
							in.bytes( dst, count * sizeof( MXAVERT ) );
							dst += count;
						}
		}
		else
		{
			Mxaloadheader->num_frames = 0;
			Mxaloadheader->num_anim_vertices = NULL;
			Mxaloadheader->frame_pnts = NULL;
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
			Mxaloadheader->FirePoints = NULL;
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
			Mxaloadheader->SpotFX = NULL;
			Mxaloadheader->NumSpotFX = 0;
		}
	}
	else
	{
		Mxaloadheader->NumFirePoints = 0;
		Mxaloadheader->FirePoints = NULL;
		Mxaloadheader->NumSpotFX = 0;
		Mxaloadheader->SpotFX = NULL;
	}

	/* Everything the model keeps has been copied out of the file. */
	free( Mxaloadheader->OrgAddr );
	Mxaloadheader->OrgAddr = NULL;
	Mxaloadheader->Buffer = NULL;

	if ( colourkey )
		DebugPrintf( "Mxaload: %d colourkey triangles found\n", colourkey );
	// Mxaloadheader is valid and can be executed...
	Mxaloadheader->state = true;
	return( true );
}

/*===================================================================
	Procedure	:		Execute all group buffers for a Mxaloadheader
	Input		;		MXALOADHEADER *
	Output		:		FLASE/true
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
			for ( i=0 ; i<Mxaloadheader->Group[group].num_execbufs; i++)
			{
				if( Mxaloadheader->Group[group].exec_type[i]&HASTRANSPARENCIES )
				{
					if (!FSGetWorld(&Matrix))
					{
						return false;
					}
					AddTransExe( &Matrix , &Mxaloadheader->Group[group].renderObject[i] , 0, (u_int16_t) -1, in_group, Mxaloadheader->Group[ group ].num_verts_per_execbuf[i] );
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
 * Release Execute buffers
 */

void ReleaseMxaloadheader( MXALOADHEADER * Mxaloadheader )
{
    int i;
    int group;

	if ( !Mxaloadheader->state )
		return;
    for (group = 0; group < Mxaloadheader->num_groups; group++)
	{
	    for (i = 0; i < Mxaloadheader->Group[group].num_execbufs; i++)
		{
			FSReleaseRenderObject(&Mxaloadheader->Group[group].renderObject[i]);
			if (Mxaloadheader->Group[group].poly_ptr[i])
			{
				free(Mxaloadheader->Group[group].poly_ptr[i]);
				Mxaloadheader->Group[group].poly_ptr[i] = NULL;
			}

			/* Per-execbuf vertex copy from mxaload.c:281 — same leak
			 * pattern as mload/mxload, fixed there too. */
			if (Mxaloadheader->Group[group].originalVerts[i])
			{
				free(Mxaloadheader->Group[group].originalVerts[i]);
				Mxaloadheader->Group[group].originalVerts[i] = NULL;
			}
		}
	}

	if ( Mxaloadheader->OrgAddr )
	{
		free( Mxaloadheader->OrgAddr );
		Mxaloadheader->OrgAddr = NULL;
	}
	
	if( Mxaloadheader->SpotFX && Mxaloadheader->NumSpotFX )
	{
		free( Mxaloadheader->SpotFX );
		Mxaloadheader->SpotFX = NULL;
		Mxaloadheader->NumSpotFX = 0;
	}

	if( Mxaloadheader->FirePoints && Mxaloadheader->NumFirePoints )
	{
		free( Mxaloadheader->FirePoints );
		Mxaloadheader->FirePoints = NULL;
		Mxaloadheader->NumFirePoints = 0;
	}

	if ( Mxaloadheader->num_frames > 1 )
	{
		if ( Mxaloadheader->num_anim_vertices )
			free( Mxaloadheader->num_anim_vertices );
		Mxaloadheader->num_anim_vertices = NULL;

		if ( Mxaloadheader->frame_pnts )
			free ( Mxaloadheader->frame_pnts );
		Mxaloadheader->frame_pnts = NULL;

		free( Mxaloadheader->anim_verts );
		Mxaloadheader->anim_verts = NULL;
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
	Mxaloadheader->Buffer = NULL;

	File_Size = Get_File_Size( Filename );	

	if( !File_Size )
	{
		Msg( "PreMxaload() File %s not found\n", Filename );
		return( false );
	}

	Buffer = (char*)calloc( 1, File_Size + 32 );

	if( Buffer == NULL )
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
				Mxaloadheader->TloadIndex[i] = AddTexture( &Tloadheader , (char*)&TempFilename[0] , true , true ,false, 0, 0 );
			
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
	Output		:		FLASE/true
===================================================================*/
bool	InterpFrames( MXALOADHEADER * Mxaloadheader , int FromFrame, int ToFrame , float Interp )
{
    LPLVERTEX	lpBufStart = NULL;
	LPLVERTEX	lpLVERTEX;
	LPLVERTEX	lpLVERTEX2;
	MXAVERT *		FromVert;
	MXAVERT *		ToVert;
	u_int16_t			texgroup;
	u_int16_t			num_anim_vertices;
	int execbuf;
	int group;
	COLOR_RGBA * color;
	COLOR_RGBA * from_color;
	COLOR_RGBA * to_color;

	if ( Mxaloadheader->num_frames <= 1 ) return true;

	for( group=0 ; group<Mxaloadheader->num_groups; group++)
	{
		for( execbuf=0 ; execbuf<Mxaloadheader->Group[group].num_execbufs; execbuf++)
		{
			if (!(FSLockVertexBuffer(&Mxaloadheader->Group[group].renderObject[execbuf], &lpBufStart)))
			{
				return false;
			}
		
			for( texgroup=0; texgroup < Mxaloadheader->Group[group].num_texture_groups[execbuf] ; texgroup++)
			{
				lpLVERTEX = 	( LPLVERTEX ) ( (char*) lpBufStart+( (u_int32_t) Mxaloadheader->Group[group].texture_group_vert_off[execbuf][texgroup]));
				lpLVERTEX += 8;

				lpLVERTEX2 = ( LPLVERTEX ) ( (char*) Mxaloadheader->Group[group].originalVerts[execbuf] + ( (u_int32_t) Mxaloadheader->Group[group].texture_group_vert_off[execbuf][texgroup]));
				lpLVERTEX2 += 8;
				
				FromVert = ( MXAVERT * ) Mxaloadheader->frame_pnts[FromFrame][group][execbuf][texgroup];
				ToVert = ( MXAVERT * ) Mxaloadheader->frame_pnts[ToFrame][group][execbuf][texgroup];

				for( num_anim_vertices = 0 ; num_anim_vertices <  Mxaloadheader->num_anim_vertices[FromFrame][group][execbuf][texgroup] ; num_anim_vertices++ )
				{
					if ( FromVert->flags & MXA_ANIM_POS )
					{
						lpLVERTEX->x = FromVert->x + ( ToVert->x - FromVert->x ) * Interp;
						lpLVERTEX->y = FromVert->y + ( ToVert->y - FromVert->y ) * Interp;
						lpLVERTEX->z = FromVert->z + ( ToVert->z - FromVert->z ) * Interp;
					}
					if ( FromVert->flags & MXA_ANIM_RGB )
					{
						color = (COLOR_RGBA *) &lpLVERTEX2->color;


						from_color = (COLOR_RGBA *) &FromVert->color;
						to_color = (COLOR_RGBA *) &ToVert->color;
						color->r = from_color->r + (int8_t) fast_floorf( ( to_color->r - from_color->r ) * Interp );
						color->g = from_color->g + (int8_t) fast_floorf( ( to_color->g - from_color->g ) * Interp );
						color->b = from_color->b + (int8_t) fast_floorf( ( to_color->b - from_color->b ) * Interp );
					}
					if ( FromVert->flags & MXA_ANIM_UV )
					{
						lpLVERTEX->tu = FromVert->tu + ( ToVert->tu - FromVert->tu ) * Interp;
						lpLVERTEX->tv = FromVert->tv + ( ToVert->tv - FromVert->tv ) * Interp;
					}

					lpLVERTEX++;
					lpLVERTEX2++;
					FromVert++;
					ToVert++;
				}
			}
			if (!(FSUnlockVertexBuffer(&Mxaloadheader->Group[group].renderObject[execbuf])))
			{
				return false;
			}
		}
	}
	
	return true;
}
