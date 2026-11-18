/* mxload.cpp -- loads static (.mx) models
 *
 * PreMxload reads the file and queues its textures; Mxload builds each
 * execbuf's vertex, index and normal buffers and texture groups, keeps
 * originalVerts (the baked vertex colours that lighting restores each frame)
 * and, for models flagged StoreTriangles, a copy of the triangles (the
 * title screen intersects them to lay menu text across its 3D models). It
 * also reads the model's texture animations and its fire
 * points and spot effects. Everything is copied out of the file, which is
 * freed at the end of Mxload. Models loaded with Panel set are the 3D HUD
 * panel; their animations tagged "Primary", "Secondary", "Mine" or
 * "MineNum" are remembered so the HUD can switch frames.
 *
 * .mx layout (little-endian, packed; read through FileReader, counts u16):
 *
 *   magic, version (u32 each)
 *   num_texture_files, texture names (NUL-terminated)       -- PreMxload
 *   num_groups
 *   per group: num_execbufs
 *     per execbuf:
 *       execute-buffer size (unused), exec_type (bit 0: has
 *       transparencies), num_vertices
 *       OLDLVERTEX[num_vertices]       x y z, reserved, colour, specular, tu tv
 *       num_texture_groups
 *       per texture group:
 *         texture type, first vertex and vertex count (all unused),
 *         tpage, num_triangles, MFACE[num_triangles]  v1 v2 v3, flags (bit 0:
 *         colour-keyed), face normal
 *   a u16 that is skipped (an old format flag)
 *   num_animations
 *   per animation: frame count and max loops (unused), length,
 *       u16[length] (frame numbers and TEXANIM_* commands)
 *   if num_animations: per group, per execbuf: num_animating_polys
 *     per poly: tag (32 bytes), animation, frames, vertices,
 *               u16[vertices] (vertex numbers), { tu, tv }[vertices * frames]
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
#include "game/main.h"
#include <stdio.h>
#include "math/new3d.h"
#include "math/quat.h"
#include "game/compobjects.h"
#include "game/bgobjects.h"
#include "game/object.h"
#include "rendering/stereomemo.h"
#include "net/networking.h"
#include "data/mxload.h"
#include "game/models.h"

#include "audio/sfx.h"
#include "game/spotfx.h"
#include "util/util.h"
#include "game/oct2.h"
#include "data/mload.h"
#include "data/tload.h"
#include "util/hunk_alloc.h"
#include "util/file_reader.h"

/*===================================================================
		Defines
===================================================================*/
constexpr int MX_VERSION_NUMBER = 2;

/*===================================================================
		Globals...
===================================================================*/

constexpr int NUMOFPOLYSPER3DPANELITEM = 2;
POLYANIM * Panel3DPolyAnims_Primary[NUMOFPOLYSPER3DPANELITEM];
POLYANIM * Panel3DPolyAnims_Secondary[NUMOFPOLYSPER3DPANELITEM];
POLYANIM * Panel3DPolyAnims_Mine[NUMOFPOLYSPER3DPANELITEM];
POLYANIM * Panel3DPolyAnims_MineNum[NUMOFPOLYSPER3DPANELITEM];

void Check3DPanelPolyAnim(POLYANIM * PolyAnim );

/*===================================================================
	Procedure	:		Load .Mx File
	Input		:		char	*	Filename , MXLOADHEADER *
	Output		:		Nothing
===================================================================*/
bool Mxload( char * Filename, MXLOADHEADER * Mxloadheader , bool Panel, bool StoreTriangles )
{

	TRIANGLE		FacePnt; // was a pointer
	TRIANGLE	*TempFacePnt; ZERO_STACK_MEM(TempFacePnt);
	u_int16_t			exec_type = 0;			// the execbuf's type (bit 0: has transparencies)
	u_int16_t			num_vertices = 0;		// overall number of verts
	u_int16_t			num_texture_groups = 0; // number of triangle groups
	u_int16_t			num_triangles = 0;		// number of triangles in group
	LVERTEX	*lpLVERTEX = NULL;
	LVERTEX	*lpBufStart = NULL;
	WORD			*lpIndices = NULL;
	NORMAL		*lpNormals = NULL;
	int				ibIndex = 0;
	int				i,e,o;
	u_int16_t			tpage = 0;
	int				execbuf;
	int				group;
	u_int16_t			animation,frames,vertices;
	POLYANIM	*	PolyAnim;
	TANIMUV		*	TanimUV;
	int			*	intPnt;
	u_int16_t			num_animations;
	u_int16_t			animsize;
	int				r,g,b,a;
	COLOR		color;
	u_int16_t			NumSpotFX;
	PVSPOTFX	*	SpotFXPtr;
	u_int16_t			NumFirePoints;
	PVFIREPOINT *	FirePointPtr;
	u_int32_t			Colour;
	int				colourkey = 0;

	int8_t			SFXFilename[ 128 ];

	int triangleCount = 0;
	int indexOffset = 0;
	int tempInt;

	DebugPrintf("Mxload - %s\n",Filename);

	// Mxloadheader is not valid until everything has been done..
	Mxloadheader->state = false;

	if( Panel == true )
	{
		if( DrawPanel == false )
		{
			return true;
		}
	}

	if( Panel )
	{
		// clear out the 3d Panel PolyAnim Data....
		for( i = 0 ; i < NUMOFPOLYSPER3DPANELITEM ; i++ )
		{
			Panel3DPolyAnims_Primary[i]=NULL;
			Panel3DPolyAnims_Secondary[i]=NULL;
			Panel3DPolyAnims_Mine[i]=NULL;
			Panel3DPolyAnims_MineNum[i]=NULL;
		}
	}

	if( Mxloadheader->Buffer == NULL)
	{
		Msg( "Mxload() Buffer was not allocated in %s\n", Filename );
		return false;
	}
	FileReader in( Mxloadheader->Buffer );

	/*	get the number of groups	*/
	Mxloadheader->num_groups = in.u16();

	if ( Mxloadheader->num_groups > MAXGROUPS )
	{
		Msg( "Mxload() Num groups > MAXGROUPS in %s\n", Filename );
		return false;
	}

	for( group=0 ; group<Mxloadheader->num_groups; group++)
	{
		/*	get the number of execbufs in this group	*/
		Mxloadheader->Group[group].num_execbufs = in.u16();

		if ( Mxloadheader->Group[group].num_execbufs > MAXEXECBUFSPERGROUP )
		{
			Msg( "Mxload() numexecbufs > MAXEXECBUFSPERGROUP in %s\n", Filename );
			return false;
		}

		for( execbuf=0 ; execbuf<Mxloadheader->Group[group].num_execbufs; execbuf++)
		{
			num_texture_groups = 0;

			in.skip( sizeof( u_int16_t ) );	// Direct3D execute-buffer size (unused)

			/*	get the type of execution buffer	*/
			exec_type = in.u16();

			/*	get the number of verts in execution buffer	*/
			num_vertices = in.u16();

			/*	record the execbuf's type	*/
			Mxloadheader->Group[group].exec_type[execbuf] = exec_type;

			/*	record how many verts the execbuf has	*/
			Mxloadheader->Group[group].num_verts_per_execbuf[execbuf] = num_vertices;

			/*	create a vertex buffer	*/
			if (!FSCreateVertexBuffer(&Mxloadheader->Group[group].renderObject[execbuf], num_vertices))
			{
				Msg( "Mxload : MakeExecBuffer Failed\n" );
				return false;
			}
			Mxloadheader->Group[group].renderObject[execbuf].vertOwner = VERT_OWNER_MODEL;

			DebugPrintf("created buffer to hold :%d verts\n", num_vertices);

			/*	lock the vertex buffer	*/
			if (!(FSLockVertexBuffer(&Mxloadheader->Group[group].renderObject[execbuf], &lpLVERTEX)))
			{
				Msg( "Mxload() lock failed in %s\n", Filename );
				return false;
			}

			lpBufStart = lpLVERTEX;

			/*	copy the vertices into the vertex buffer	*/
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

				color = RGBA_MAKE( r , g , b , a  );
				lpLVERTEX[i].color = color;

				// the file's specular colour is unused (LVERTEX has no specular)
			}

			/* Keep the vertices as loaded: lighting restores baked colours from them each frame. */
			Mxloadheader->Group[group].originalVerts[execbuf] = (LVERTEX *)malloc(sizeof(LVERTEX) * num_vertices);
			memmove(Mxloadheader->Group[group].originalVerts[execbuf], &lpLVERTEX[0], sizeof(LVERTEX) * num_vertices);//memcpy

			//	Stuff to clip if bounding box is off screen

			num_texture_groups = in.u16();
			DebugPrintf("texture groups: %d\n", num_texture_groups);
			Mxloadheader->Group[group].num_texture_groups[execbuf] = num_texture_groups;


			triangleCount = 0;
			tempInt = 0;

			/* find out how many vertexes we'll need to load into our vertex buffer */
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
			if (!FSCreateIndexBuffer(&Mxloadheader->Group[group].renderObject[execbuf], triangleCount * 3))
			{
				return false;
			}

			DebugPrintf("created index buffer to hold :%d incidices\n", triangleCount * 3);

			/*	lock the index buffer	*/
			if (!(FSLockIndexBuffer(&Mxloadheader->Group[group].renderObject[execbuf], &lpIndices)))
			{
				Msg( "Mxload() lock failed in %s\n", Filename );
				return false;
			}

      if (!FSCreateNormalBuffer(&Mxloadheader->Group[group].renderObject[execbuf], triangleCount))
      {
        DebugPrintf("Mxload() failed to create normal buffer in %s\n", Filename);
        return false;
      }
      if (!(FSLockNormalBuffer(&Mxloadheader->Group[group].renderObject[execbuf], &lpNormals)))
      {
        Msg( "Mxload() normal lock failed in %s\n", Filename );
        return false;
      }

			/* Allocate this execbuf's textureGroups[] from the TAG_LEVEL
			 * hunk, sized to num_texture_groups. Bulk-freed via
			 * Hunk_FreeAll(TAG_LEVEL) on level transition. */
			if ( num_texture_groups > 0 )
			{
				Mxloadheader->Group[group].renderObject[execbuf].textureGroups =
				    (TEXTUREGROUP *) Hunk_Alloc(TAG_LEVEL,
				                                num_texture_groups * sizeof(TEXTUREGROUP));
				if ( !Mxloadheader->Group[group].renderObject[execbuf].textureGroups )
				{
					Msg( "Mxload() hunk-alloc failed for textureGroups[%u] %s\n",
					     (unsigned)num_texture_groups, Filename );
					return false;
				}
			}

			ibIndex = 0;
			indexOffset = 0;

			for ( i=0 ; i<num_texture_groups; i++)
			{
				in.skip( sizeof( u_int16_t ) );	// texture type (unused)
				in.skip( sizeof( u_int16_t ) );	// first vertex (unused)
				in.skip( sizeof( u_int16_t ) );	// group vertex count (unused)
				tpage = in.u16();
				num_triangles = in.u16();
				DebugPrintf("trinagles %d\n", num_triangles);

				TempFacePnt = (TRIANGLE *) /*lpLVERTEX*/lpIndices;


				/*	copy the triangles into the index and normal buffers	*/
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
					if( Mxloadheader->num_texture_files )
						FixUV( &FacePnt, lpBufStart, Mxloadheader->TloadIndex[tpage], Mxloadheader->Group[group].originalVerts[execbuf]);
					lpNormals++;
					tempInt+=3;
				}

				// BJD - check this. correct value to place here?
				Mxloadheader->Group[ group ].renderObject[execbuf].textureGroups[i].numVerts = num_vertices;//num_triangles * 3;//num_triangles * 3;
				Mxloadheader->Group[ group ].renderObject[execbuf].textureGroups[i].numTriangles = num_triangles;

				/* keep track of our offset into our index buffer */
				Mxloadheader->Group[ group ].renderObject[execbuf].textureGroups[i].startIndex = indexOffset;
				indexOffset += num_triangles * 3;

				if( Mxloadheader->num_texture_files == 0 )
				{
					Mxloadheader->Group[ group ].renderObject[execbuf].textureGroups[i].texture = NULL;
					Mxloadheader->Group[ group ].renderObject[execbuf].textureGroups[i].colourkey = false;
				}
				else
				{
					Mxloadheader->Group[ group ].renderObject[execbuf].textureGroups[i].texture = Tloadheader.lpTexture[Mxloadheader->TloadIndex[tpage]];
					Mxloadheader->Group[ group ].renderObject[execbuf].textureGroups[i].colourkey = Tloadheader.ColourKey[Mxloadheader->TloadIndex[tpage]];
				}
			}

			DebugPrintf("put %d indices into IB\n", tempInt);

			// 3D panel models: put the cull state back to the default
			if( Panel )
			{
				reset_cull();
			}

			if (StoreTriangles)
			{	Mxloadheader->Group[group].num_polys_per_execbuf[execbuf] = triangleCount;//num_triangles;
				Mxloadheader->Group[group].poly_ptr[execbuf] = (TRIANGLE *)malloc( sizeof (TRIANGLE) * /*num_triangles*/triangleCount);

				memmove(Mxloadheader->Group[group].poly_ptr[execbuf], TempFacePnt, sizeof (TRIANGLE) * /*num_triangles*/triangleCount);//memcpy

			}
			else
			{
				Mxloadheader->Group[group].num_polys_per_execbuf[execbuf] = 0;
				Mxloadheader->Group[group].poly_ptr[execbuf] = NULL;
			}

			/*	unlock the vertex buffer	*/
			if (!(FSUnlockVertexBuffer(&Mxloadheader->Group[group].renderObject[execbuf])))
			{
				Msg( "Mxload() vb unlock failed in %s\n", Filename );
				return false ;
			}

			/*	unlock the index buffer	*/
			if (!(FSUnlockIndexBuffer(&Mxloadheader->Group[group].renderObject[execbuf])))
			{
				Msg( "Mxload() ib unlock failed in %s\n", Filename );
				return false ;
			}

      if (!(FSUnlockNormalBuffer(&Mxloadheader->Group[group].renderObject[execbuf])))
      {
        Msg( "Mxload() normal unlock failed in %s\n", Filename );
        return false ;
      }

			Mxloadheader->Group[group].renderObject[execbuf].numTextureGroups = num_texture_groups;


			/* update the renderObject */

		}
	}

	// Background Animation Stuff......
	in.skip( sizeof( u_int16_t ) );		// skip old mxv flag..
	num_animations = in.u16();
	Mxloadheader->AnimData.num_animations = num_animations;

	if( num_animations )
	{
		for( i = 0 ; i < num_animations ; i++ )
		{
			in.skip( sizeof( u_int16_t ) );	// frames (unused)
			in.skip( sizeof( u_int16_t ) );	// max loops (unused)
			animsize = in.u16();
			Mxloadheader->AnimData.AnimSeq[i] = (u_int16_t *) malloc( animsize * sizeof( u_int16_t ) );
			in.bytes( Mxloadheader->AnimData.AnimSeq[i], animsize * sizeof( u_int16_t ) );
		}

		for( group=0 ; group<Mxloadheader->num_groups; group++)
		{
			for( execbuf=0 ; execbuf<Mxloadheader->Group[group].num_execbufs; execbuf++)
			{
				// Animating background polys......
				Mxloadheader->Group[group].num_animating_polys[execbuf] = in.u16();
				if( Mxloadheader->Group[group].num_animating_polys[execbuf] != 0 )
				{
					Mxloadheader->Group[group].polyanim[execbuf] = (POLYANIM*)malloc( Mxloadheader->Group[group].num_animating_polys[execbuf] * sizeof( POLYANIM ) );


					PolyAnim = Mxloadheader->Group[group].polyanim[execbuf];
					if( !PolyAnim )
					{
						Msg( "Mxload() Polyanim failed in %s\n", Filename );
						return false;
					}

					if (!(FSLockVertexBuffer(&Mxloadheader->Group[ group ].renderObject[execbuf], &lpLVERTEX)))
					{
						Msg( "Mxload : Lock ExecBuffer failed\n" );
						return false;
					}

					for( i = 0 ; i < Mxloadheader->Group[group].num_animating_polys[execbuf] ; i++ )
					{
						in.bytes( PolyAnim->Tag, sizeof( PolyAnim->Tag ) );

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
							Msg( "Mxload() PolyAnim->Vert failed\n", Filename );
							return false;
						}
						PolyAnim->UVs  = (TANIMUV *) malloc( vertices * frames * sizeof(TANIMUV) );
						if( !PolyAnim->UVs)
						{
							Msg( "Mxload() PolyAnim->UVs failed\n", Filename );
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

						FixUV_Anim( PolyAnim, lpLVERTEX, Mxloadheader->Group[group].originalVerts[execbuf] );

						if( Panel )
						{
							Check3DPanelPolyAnim( PolyAnim );
						}

						PolyAnim++;
					}

					if (!(FSUnlockVertexBuffer(&Mxloadheader->Group[group].renderObject[execbuf])))
					{
						Msg( "Mxload : Unlock ExecBuffer failed\n" );
						return false ;
					}
				}
			}
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
			Mxloadheader->NumFirePoints = NumFirePoints;
			Mxloadheader->FirePoints = (PVFIREPOINT *) malloc( NumFirePoints * sizeof( PVFIREPOINT ) );

			if( !Mxloadheader->FirePoints )
			{
				Mxloadheader->NumFirePoints = 0;
				return( false );
			}

			FirePointPtr = Mxloadheader->FirePoints;

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
			Mxloadheader->FirePoints = NULL;
			Mxloadheader->NumFirePoints = 0;
		}

		NumSpotFX = in.u16();

		if( NumSpotFX )
		{
			Mxloadheader->NumSpotFX = NumSpotFX;
			Mxloadheader->SpotFX = (PVSPOTFX *) malloc( NumSpotFX * sizeof( PVSPOTFX ) );

			if( !Mxloadheader->SpotFX )
			{
				Mxloadheader->NumSpotFX = 0;
				return( false );
			}

			SpotFXPtr = Mxloadheader->SpotFX;

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
			Mxloadheader->SpotFX = NULL;
			Mxloadheader->NumSpotFX = 0;
		}
	}
	else
	{
		Mxloadheader->NumFirePoints = 0;
		Mxloadheader->FirePoints = NULL;
		Mxloadheader->NumSpotFX = 0;
		Mxloadheader->SpotFX = NULL;
	}

	/* Everything the model keeps has been copied out of the file. */
	free( Mxloadheader->OrgAddr );
	Mxloadheader->OrgAddr = NULL;
	Mxloadheader->Buffer = NULL;

	if ( colourkey )
		DebugPrintf( "Mxload: %d colourkey triangles found\n", colourkey );
	// Mxloadheader is valid and can be executed...
	Mxloadheader->state = true;
	return( true );
}
/*===================================================================
	Procedure	:		Execute all group buffers for a Mxloadheader
	Input		;		MXLOADHEADER *
				:		u_int16_t		Models[] Index
	Output		:		FALSE/true
===================================================================*/

bool ExecuteMxloadHeader( MXLOADHEADER * Mxloadheader, u_int16_t Model  )
{
	int		i,e,f;
	int		group;
	RENDERMATRIX Matrix;
	bool	Display;
    LVERTEX	*lpPointer = NULL;
	LVERTEX	*lpLVERTEX = NULL;
	POLYANIM * PolyAnim;
	u_int32_t * u_int32Pnt;
	TANIMUV * TanimUV;
	bool	VertBufferLocked;

	ModelTextureAnimation( Mxloadheader );

	if (Mxloadheader->state == true )
	{
		for ( group=0 ; group<Mxloadheader->num_groups ; group++)
		{
			for ( i=0 ; i<Mxloadheader->Group[group].num_execbufs; i++)
			{
				VertBufferLocked = false;

				if( Mxloadheader->Group[group].num_animating_polys[i] && Mxloadheader->AnimData.num_animations )
				{

					PolyAnim = Mxloadheader->Group[group].polyanim[i];

					for( f = 0 ; f < Mxloadheader->Group[group].num_animating_polys[i] ; f++ )
					{

						if( PolyAnim->currentframe != PolyAnim->newframe )
						{
							// something has changed....
							if( !VertBufferLocked )
							{
								if (!(FSLockVertexBuffer(&Mxloadheader->Group[group].renderObject[i], &lpPointer)))
								{
									return false;
								}

								VertBufferLocked = true;
							}

							u_int32Pnt = (u_int32_t *) PolyAnim->vert;
							for( e = 0 ; e < PolyAnim->vertices ; e++ )
							{

								lpLVERTEX = lpPointer+ *u_int32Pnt++;
								TanimUV = PolyAnim->UVs;
								TanimUV += e + (PolyAnim->vertices * PolyAnim->newframe);

								memcpy(&lpLVERTEX->tu, &TanimUV->u, 4);
								memcpy(&lpLVERTEX->tv, &TanimUV->v, 4);
							}
							PolyAnim->currentframe = PolyAnim->newframe;
						}
						PolyAnim++;
					}
				}
				if( VertBufferLocked )
				{
					// unlock it..
					if (!(FSUnlockVertexBuffer(&Mxloadheader->Group[group].renderObject[i])))
					{
						return false;
					}
					VertBufferLocked = false;
				}

				if( Mxloadheader->Group[group].exec_type[i]&HASTRANSPARENCIES )
				{

					if( Model != (u_int16_t) -1 )
					{
						if( ( Models[ Model ].Flags & MODFLAG_Light ) )
						{
							if( !LightModel2( Models[ Model ].ModelNum, &Models[ Model ].Pos ) ) Display = false;
						}
						switch( Models[ Model ].Func )
						{
							case MODFUNC_ScaleDonut:
								ProcessModel2( Mxloadheader, Models[ Model ].Scale, Models[ Model ].MaxScale, 1, 1, 1 );

							default:
								break;
						}
					}

					if (!FSGetWorld(&Matrix))
					{
						return false;
					}

					if( Models[ Model ].Flags & MODFLAG_UseClipGroup )
					{
						AddTransExe( &Matrix , &Mxloadheader->Group[group].renderObject[i] , 0, Model, Models[ Model ].ClipGroup, Mxloadheader->Group[ group ].num_verts_per_execbuf[i] );
					}
					else
					{
						AddTransExe( &Matrix , &Mxloadheader->Group[group].renderObject[i] , 0, Model, Models[ Model ].Group, Mxloadheader->Group[ group ].num_verts_per_execbuf[i] );
					}
				}
				else
				{
					Display = true;

					if( Model != (u_int16_t) -1 )
					{

						if( ( Models[ Model ].Flags & MODFLAG_Light ) )
						{
							if( !LightModel2( Models[ Model ].ModelNum, &Models[ Model ].Pos ) ) Display = false;
						}

						switch( Models[ Model ].Func )
						{
							case MODFUNC_Explode:
								ProcessModel( Mxloadheader, Models[ Model ].Scale, Models[ Model ].MaxScale, 1, 0, 0 );
								break;

							case MODFUNC_Scale:
							case MODFUNC_Scale2:
								ProcessModel( Mxloadheader, Models[ Model ].Scale, Models[ Model ].MaxScale, 0, 0, 1 );
								break;
							case MODFUNC_Regen:
								ProcessModel( Mxloadheader, Models[ Model ].Scale, Models[ Model ].MaxScale, 0, 1, 0 );
								break;

							case MODFUNC_OrbitPulsar:
								if( ( Ships[ Models[ Model ].Ship ].Object.Flags & SHIP_Stealth ) )
								{
									if( !LightModel( Models[ Model ].ModelNum, &Models[ Model ].Pos ) ) Display = false;
								}
								break;

							case MODFUNC_ScaleDonut:
								break;

							case MODFUNC_RestoreColours:
								RestoreColourMxloadHeader(Mxloadheader );
								break;

							case MODFUNC_Nothing:
							default:
								break;
						}
					}

					if( Display )
					{
#ifdef __3DS__
						{ c3d_set_object_proctex(true); }
#endif
						bool ok = draw_object(&Mxloadheader->Group[group].renderObject[i]);
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
	}
	return true;
}

/*
 * ReleaseMxloadheader
 * Release the render objects and everything the loader allocated
 */
void
ReleaseMxloadheader( MXLOADHEADER * Mxloadheader )
{
    int i,e;
    int group;
	POLYANIM * PolyAnim;

	if ( !Mxloadheader->state )
		return;

    for (group = 0; group < Mxloadheader->num_groups; group++)
	{
	    for (i = 0; i < Mxloadheader->Group[group].num_execbufs; i++)
		{

			FSReleaseRenderObject(&Mxloadheader->Group[group].renderObject[i]);

			if (Mxloadheader->Group[group].poly_ptr[i])
			{
				free(Mxloadheader->Group[group].poly_ptr[i]);
				Mxloadheader->Group[group].poly_ptr[i] = NULL;
			}

			/* Per-execbuf vertex copy from mxload.cpp — same leak
			 * pattern as mload.cpp, fixed there too. */
			if (Mxloadheader->Group[group].originalVerts[i])
			{
				free(Mxloadheader->Group[group].originalVerts[i]);
				Mxloadheader->Group[group].originalVerts[i] = NULL;
			}

			PolyAnim = Mxloadheader->Group[group].polyanim[i];

			if( PolyAnim )
			{
				for( e = 0 ; e < Mxloadheader->Group[group].num_animating_polys[i] ; e++ )
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
		   		free(Mxloadheader->Group[group].polyanim[i]);
				Mxloadheader->Group[group].polyanim[i] = NULL;
			}
		}
	}

	for( i = 0 ; i < Mxloadheader->AnimData.num_animations ; i++ )
	{
		free(Mxloadheader->AnimData.AnimSeq[i]);
		Mxloadheader->AnimData.AnimSeq[i] = NULL;
	}

	if( Mxloadheader->SpotFX && Mxloadheader->NumSpotFX )
	{
		free( Mxloadheader->SpotFX );
		Mxloadheader->SpotFX = NULL;
		Mxloadheader->NumSpotFX = 0;
	}

	if( Mxloadheader->FirePoints && Mxloadheader->NumFirePoints )
	{
		free( Mxloadheader->FirePoints );
		Mxloadheader->FirePoints = NULL;
		Mxloadheader->NumFirePoints = 0;
	}

	if ( Mxloadheader->OrgAddr )
	{
		free( Mxloadheader->OrgAddr );
		Mxloadheader->OrgAddr = NULL;
	}

	Mxloadheader->state = false;
}

/*===================================================================
	Procedure	:		Pre - Load .Mx File
	Input		:		char	*	Filename , MXLOADHEADER *
	Output		:		Nothing
===================================================================*/
bool PreMxload( char * Filename, MXLOADHEADER * Mxloadheader , bool Panel, bool LevelSpecific )
{
	long			File_Size;
	long			Read_Size;
	char		*	Buffer;
	int			i;
	u_int32_t			MagicNumber;
	u_int32_t			VersionNumber;
	int8_t			TempFilename[ 256 ];

	// Mxloadheader is not valid until everything has been done..
	Mxloadheader->state = false;
	Mxloadheader->Buffer = NULL;


	if( Panel == true )
	{
		if( DrawPanel == false )
		{
			return true;
		}
	}


	File_Size = Get_File_Size( Filename );

	if( !File_Size )
	{
		Msg( "PreMxload() File %s does not exist\n", Filename );
		return( false );
	}

	Buffer = (char*)calloc( 1, File_Size + 32 );

	if( Buffer == NULL )
	{
		Msg( "PreMxload() Allocate buffer for %s failed\n", Filename );
		return( false );
	}

	Read_Size = Read_File( Filename, Buffer, File_Size );

	if( Read_Size != File_Size )
	{
		Msg( "PreMxload() Error reading file %s\n", Filename );
		return( false );
	}

	Mxloadheader->OrgAddr = Buffer;

	FileReader in( Buffer );
	MagicNumber = in.u32();
	VersionNumber = in.u32();

	if( ( MagicNumber != MAGIC_NUMBER ) || ( VersionNumber != MX_VERSION_NUMBER  ) )
	{
		Msg( "PreMxload() Incompatible model ( .MX ) file %s", Filename );
		return( false );
	}

	Mxloadheader->num_texture_files = in.u16();

	if ( Mxloadheader->num_texture_files > MAXTPAGESPERMXLOAD )
	{
		Msg( "PreMxload() Too many textures in %s\n", Filename );
		return false;
	}

	if ( Mxloadheader->num_texture_files !=0)
	{
		/*	put the filenames in the tex struct	*/
		for( i = 0;	 i < Mxloadheader->num_texture_files; i++)
		{
			strcpy( Mxloadheader->ImageFile[i], in.cstring() );

			if( LevelSpecific )
				GetLevelTexturePath( (char*)&TempFilename[ 0 ], &Mxloadheader->ImageFile[i][0], &ShortLevelNames[ LevelNum ][ 0 ] );
			else
				sprintf( (char*)&TempFilename[ 0 ], "data\\textures\\%s", &Mxloadheader->ImageFile[i][0] );

			Mxloadheader->TloadIndex[i] = AddTexture( &Tloadheader , (char*)&TempFilename[ 0 ], (u_int16_t) (Panel ^ true) , false, 0, 0 );
			if( Mxloadheader->TloadIndex[i] == -1 )
			{
				Msg( "PreMxload() Too many TPages\n" );
				return false;
			}
		}

	}

	Mxloadheader->Buffer = (const char *) in.pos();
	return( true );
}

/*===================================================================
	Procedure	:		Execute all group buffers for a Mxloadheader
	Input		;		MXLOADHEADER *
				:		u_int16_t		Models[] Index
	Output		:		FALSE/true
===================================================================*/
bool ReallyExecuteMxloadHeader( MXLOADHEADER * Mxloadheader, u_int16_t /*Model*/ )
{
	int			i;
	int			group;

	if (Mxloadheader->state == true )
	{
		for ( group=0 ; group<Mxloadheader->num_groups ; group++)
		{
			for ( i=0 ; i<Mxloadheader->Group[group].num_execbufs; i++)
			{
#ifdef __3DS__
				{ c3d_set_object_proctex(true); }
#endif
				bool ok = draw_object(&Mxloadheader->Group[group].renderObject[i]);
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
	return true;
}

/*===================================================================
	Procedure	:		Handle All Animation For an Mloadheader
	Input		:		MLOADHEADER *
	Output		:		Nothing
===================================================================*/
void ModelTextureAnimation( MXLOADHEADER * Mxloadheader  )
{
	u_int16_t		*	Uint16Pnt;
	int	i;
	int	execbuf;
	int	group;
	POLYANIM * PolyAnim;
	u_int16_t * AnimData;
	float	ExtraTime;
	if( !Mxloadheader->AnimData.num_animations )
		return;
	if( AnimOncePerFrame == Mxloadheader->AnimOncePerFrame )
		return;
	Mxloadheader->AnimOncePerFrame = AnimOncePerFrame;


	for( group=0 ; group<Mxloadheader->num_groups; group++)
	{
		for( execbuf=0 ; execbuf<Mxloadheader->Group[group].num_execbufs; execbuf++)
		{
			PolyAnim = Mxloadheader->Group[group].polyanim[execbuf];

			for( i = 0 ; i < Mxloadheader->Group[group].num_animating_polys[execbuf] ; i++ )
			{

				if( ( PolyAnim->State == TEXTUREANIM_Go ) &&
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
						AnimData = ( Mxloadheader->AnimData.AnimSeq[PolyAnim->animation] + PolyAnim->CurrentOffset );
						Uint16Pnt = HandleAnimCommands( PolyAnim , AnimData , Mxloadheader->AnimData.AnimSeq[PolyAnim->animation] );
						PolyAnim->CurrentOffset = Uint16Pnt - Mxloadheader->AnimData.AnimSeq[PolyAnim->animation];
						smemo_invalidate_frame(); /* model UVs just stepped (never in the persistent cache) */
					}

				}
   				PolyAnim++;
			}
		}
	}
}

/*===================================================================
	Procedure	:		Restore the coolours for a Mxloadheader..
	Input		;		MXLOADHEADER *
	Output		:		FALSE/true
===================================================================*/

bool RestoreColourMxloadHeader( MXLOADHEADER * Mxloadheader1 )
{
	int		i,e;
	int		group;
	LVERTEX	*dstVert = NULL;
	LVERTEX			*VertPtr;

	if( !Mxloadheader1->state )
		return false;
	for ( group=0 ; group<Mxloadheader1->num_groups ; group++)
	{
		for ( i=0 ; i<Mxloadheader1->Group[group].num_execbufs; i++)
		{
			if (!(FSLockVertexBuffer(&Mxloadheader1->Group[group].renderObject[i], &dstVert)))
			{
				return false;
			}

			VertPtr = Mxloadheader1->Group[group].originalVerts[i];

			for( e = 0 ; e < Mxloadheader1->Group[group].num_verts_per_execbuf[i] ; e++ )
			{
				dstVert->color = VertPtr->color;
				VertPtr++;
				dstVert++;
			}

			if (!(FSUnlockVertexBuffer(&Mxloadheader1->Group[group].renderObject[i])))
			{
				return false;
			}
		}
	}
	return true;
}
/*===================================================================
	Procedure	:		CheckFor 3d Panel PolyAnims..
	Input		;		POLYANIM * PolyAnim
	Output		:		NOTHING
===================================================================*/
void Check3DPanelPolyAnim(POLYANIM * PolyAnim )
{
	int j;
	// Make a note of any polyanims that are to do with the 3D Panel.....
	if( strcasecmp( "Primary" , (char*) &PolyAnim->Tag[0] ) == 0 )
	{
		for( j = 0 ; j < NUMOFPOLYSPER3DPANELITEM ; j++ )
		{
			if( Panel3DPolyAnims_Primary[j] == NULL )
				Panel3DPolyAnims_Primary[j] = PolyAnim;
		}
	}
	else if( strcasecmp( "Secondary" , (char*) &PolyAnim->Tag[0] ) == 0 )
	{
		for( j = 0 ; j < NUMOFPOLYSPER3DPANELITEM ; j++ )
		{
			if( Panel3DPolyAnims_Secondary[j] == NULL )
				Panel3DPolyAnims_Secondary[j] = PolyAnim;
		}
	}
	else if( strcasecmp( "Mine" , (char*) &PolyAnim->Tag[0] ) == 0 )
	{
		for( j = 0 ; j < NUMOFPOLYSPER3DPANELITEM ; j++ )
		{
			if( Panel3DPolyAnims_Mine[j] == NULL )
				Panel3DPolyAnims_Mine[j] = PolyAnim;
		}

	}
	else if( strcasecmp( "MineNum" , (char*) &PolyAnim->Tag[0] ) == 0 )
	{
		for( j = 0 ; j < NUMOFPOLYSPER3DPANELITEM ; j++ )
		{
			if( Panel3DPolyAnims_MineNum[j] == NULL )
				Panel3DPolyAnims_MineNum[j] = PolyAnim;
		}
	}
}

