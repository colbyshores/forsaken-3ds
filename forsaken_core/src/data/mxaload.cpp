/*==========================================================================
 *
 *  File: Mxaload.c
 *	loads in a bin....
 *  extracts the names of the ppm's and creates a suface and material...
 *  make an execution list per group of....
 *  make a list of D3DVERTEX's.....and TRIANGLE's....
***************************************************************************/

/*
Execution list format with animated vertices (*.mxa):

num_texture_files : u_int16_t
texture_file_name[num_texture_files] : '\0' separated strings
num_groups : u_int16_t
{
	num_exec_lists : u_int16_t
	{
	  exec_type : u_int16_t // 0 = entirely opaque, 1 = contains transparencies
	  num_vertices : u_int16_t
	  vertex(x,y,z,reserved,colour,specular,tu,tv)[num_vertices] : x, y, z float, others u_int32_t
	  num_texture_groups : u_int16_t
	  {
	    texture_type : u_int16_t // 0 = normal, 1 = environment mapped
		start_vertex : u_int16_t
		num_vertices : u_int16_t
		texture_no : u_int16_t
		num_triangles : u_int16_t
		triangles(v0,v1,v2,pad16,nx,ny,nz)[num_triangles] : v? u_int16_t, pad16 u_int16_t, n? float
	  }[num_texture_groups]
	}[num_exec_lists]
}[num_groups]
mxtype : u_int16_t // always 2 for mxa format
num_frames : u_int16_t // number of animation frames
{
	{
		{
			{
				num_anim_vertices : u_int16_t // number of animating vertices for this texture group
				vertex(x, y, z)[num_anim_vertices] : all float
			}[num_texture_groups]
		}[num_exec_lists]
	}[num_groups]
}[num_frames]
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

	char		*	Buffer;
	char		*	tempBuffer;
	int16_t		*	Int16Pnt;
	u_int32_t		*	Uint32Pnt;
	u_int16_t		*	Uint16Pnt;
	float		*	FloatPnt;
	MFACE		*	MFacePnt;
	TRIANGLE		FacePnt; // was a pointer
	LPTRIANGLE	TempFacePnt; ZERO_STACK_MEM(TempFacePnt);
	u_int16_t			exec_type;			// the type of execute buffer
	u_int16_t			num_vertices;		// overall number of verts
	u_int16_t			num_texture_groups;// number of triangle groups
	u_int16_t			num_triangles;		// number of triangles in group
	u_int16_t			group_vertex_start; // where in the vert list to start processing
	LPOLDLVERTEX lpLVERTEX2;
	LPLVERTEX	lpLVERTEX;
	WORD			*lpIndices = NULL;
	NORMAL		*lpNormals = NULL;
	int				ibIndex = 0;
	u_int16_t			frame;
	u_int16_t			texgroup;
	u_int16_t			num_anim_vertices;
	int				i,e;
	u_int16_t			tpage = 0;
	int				execbuf;
	int				group;
	u_int16_t			ExecSize;

	u_int16_t			NumSpotFX;
	PVSPOTFX *		SpotFXPtr;
	u_int16_t			NumFirePoints;
	PVFIREPOINT *	FirePointPtr;
	u_int32_t			Colour;
	int				colourkey = 0;
#if	MXA_VERSION_NUMBER == 2
	int16_t			c;
	int8_t		*	Int8Pnt;
	int8_t			SFXFilename[ 128 ];
#endif

	int numTriangles = 0;
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

	Buffer = Mxaloadheader->Buffer;
	if( Buffer == NULL)
	{
		Msg( "Mxaload() Unable to allocate buffer in %s\n", Filename );
		return false;
	}


	/*	get the number of groups	*/
	Uint16Pnt = (u_int16_t *) Buffer;
	Mxaloadheader->num_groups = *Uint16Pnt++;
	Buffer = (char *) Uint16Pnt;

	if ( Mxaloadheader->num_groups > MAXGROUPS )
	{
		Msg( "Mxaload() num_groups > MAXGROUPS\n", Filename );
		return false;
	}

	for( group=0 ; group<Mxaloadheader->num_groups; group++)
	{
		/*	get the number of execbufs in this group	*/
		Uint16Pnt = (u_int16_t *) Buffer;
		Mxaloadheader->Group[group].num_execbufs = *Uint16Pnt++;
		Buffer = (char *) Uint16Pnt;

		if ( Mxaloadheader->Group[group].num_execbufs > MAXEXECBUFSPERGROUP )
		{
			Msg( "Mxaload() num_execbufs > MAXEXECBUFSPERGROUP\n", Filename );
			return false;
		}

		for( execbuf=0 ; execbuf<Mxaloadheader->Group[group].num_execbufs; execbuf++)
		{
			Uint16Pnt = (u_int16_t *) Buffer;

			ExecSize = 	*Uint16Pnt++;
			ExecSize += 512;

			/*	get the type of execution buffer	*/
			exec_type = *Uint16Pnt++;
			/*	get the number of verts in execution buffer	*/
			num_vertices =	*Uint16Pnt++;
			Buffer = (char *) Uint16Pnt;

			/*	record how what type of exec buffer	*/
			Mxaloadheader->Group[group].exec_type[execbuf] = exec_type;

			/*	record how many verts there are in the exec buffer	*/
			Mxaloadheader->Group[group].num_verts_per_execbuf[execbuf] = num_vertices;

			lpLVERTEX2 = (LPOLDLVERTEX ) Buffer;

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

				/* lpLVERTEX2 points into a buffer at variable alignment
				   (filename strings precede it); copy 32-byte OLDLVERTEX
				   to a stack-aligned local before float reads. */
				__typeof__(*lpLVERTEX2) _ovx_local;
				memcpy_unaligned(&_ovx_local, lpLVERTEX2, sizeof(*lpLVERTEX2));
				LPOLDLVERTEX old = &_ovx_local;

				lpLVERTEX[i].x = old->x;
				lpLVERTEX[i].y = old->y;
				lpLVERTEX[i].z = old->z;
				lpLVERTEX[i].tu = old->tu;
				lpLVERTEX[i].tv = old->tv;
				lpLVERTEX[i].color = old->color;

#if !ACTUAL_TRANS
				lpLVERTEX[i].color |= 0xFF000000;
#endif
				// nothing used specular so we removed it from LVERTEX
				// lpLVERTEX->specular = lpLVERTEX2->specular;
//				lpLVERTEX->dwReserved = 0;
				lpLVERTEX2++;
			}

			/* bjd - allows us to retrieve copies of the original vertices in the new format! */
			Mxaloadheader->Group[group].originalVerts[execbuf] = (LPLVERTEX)malloc(sizeof(LVERTEX) * num_vertices);
			memmove(Mxaloadheader->Group[group].originalVerts[execbuf], &lpLVERTEX[0], sizeof(LVERTEX) * num_vertices);//memcpy
	
			
			Buffer = (char *) lpLVERTEX2;
			
			Uint16Pnt = (u_int16_t *) Buffer;
			num_texture_groups = *Uint16Pnt++;    
			Buffer = (char *) Uint16Pnt;		
			Mxaloadheader->Group[group].num_texture_groups[execbuf] = num_texture_groups;

			/* find out how many vertexes we'll need to load into our vertex buffer */
			triangleCount = 0;
			tempInt = 0;
			tempBuffer = Buffer;

			for ( i=0 ; i<num_texture_groups; i++)
			{
				u_int16_t *temp = (u_int16_t *) tempBuffer;
				
				/* skip over unwated stuff */
				temp += 4;

				numTriangles = *temp++;

				tempBuffer = (char*) temp;
				MFacePnt = (MFACE *) tempBuffer;

				MFacePnt += numTriangles;
				tempBuffer = (char *) MFacePnt; 
				triangleCount += numTriangles;
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
				Uint16Pnt = (u_int16_t *) Buffer;
				Uint16Pnt++;	// texture type (unused)
				group_vertex_start = *Uint16Pnt++;
				Uint16Pnt++;	// group vertex count (unused)
				tpage = *Uint16Pnt++;
				num_triangles = *Uint16Pnt++;    
				Buffer = (char *) Uint16Pnt;		
	
				Mxaloadheader->Group[group].texture_group_vert_off[execbuf][i] = (u_int32_t) (group_vertex_start*sizeof(LVERTEX));

				MFacePnt = (MFACE *) Buffer;

				/* bjd - CHECK - might need to be start of buffer */
				TempFacePnt = (LPTRIANGLE ) /*lpLVERTEX*/lpIndices;

				/*	copy the faces data into the execute buffer	*/
				for( e=0; e<num_triangles; e++)
				{
					FacePnt.v1 = MFacePnt->v1;
					FacePnt.v2 = MFacePnt->v2;
					FacePnt.v3 = MFacePnt->v3;

					lpIndices[ibIndex] = FacePnt.v1;
					ibIndex++;
					lpIndices[ibIndex] = FacePnt.v2;
					ibIndex++;
					lpIndices[ibIndex] = FacePnt.v3;
					ibIndex++;

					memcpy(&lpNormals->nx, &MFacePnt->nx, 4);
					memcpy(&lpNormals->ny, &MFacePnt->ny, 4);
					memcpy(&lpNormals->nz, &MFacePnt->nz, 4);

					if ( MFacePnt->pad & 1 )
					{
						// colourkey triangle found
						colourkey++;
						MFacePnt->pad &= ~1;
					}
					MFacePnt++;
					lpNormals++;
					tempInt+=3;
				}

				DebugPrintf("put %d verts into VB\n", tempInt);

				Buffer = (char *) MFacePnt;

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
				

	Uint16Pnt = (u_int16_t *) Buffer;
	// is there any vispoly info.....
	if ( *Uint16Pnt++ == 2)				// 2 means .mxa format
	{
/*===================================================================
 *		Particular Stuff Related Anims	
===================================================================*/
		Mxaloadheader->num_frames = *Uint16Pnt++;

		Buffer = (char * ) Uint16Pnt;

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
			Mxaloadheader->aligned_verts = NULL;

			{
				/* First pass: record frame_pnts into raw buffer and count total
				 * MXAVERT bytes.  We'll allocate an aligned copy afterwards so
				 * InterpFrames can use direct pointer access instead of per-vertex
				 * memcpy on every frame. */
				for( frame=0 ; frame<Mxaloadheader->num_frames; frame++)
				{
					for( group=0 ; group<Mxaloadheader->num_groups; group++)
					{
						for( execbuf=0 ; execbuf<Mxaloadheader->Group[group].num_execbufs; execbuf++)
						{
							if (!(FSLockVertexBuffer(&Mxaloadheader->Group[ group ].renderObject[execbuf], &lpLVERTEX)))
							{
								Msg( "Mxaload : Lock VertexBuffer failed\n" );
								return false;
							}

							for( texgroup=0; texgroup < Mxaloadheader->Group[group].num_texture_groups[execbuf] ; texgroup++)
							{
								Uint16Pnt = (u_int16_t *) Buffer;
								num_anim_vertices = *Uint16Pnt++;
								Buffer = (char * ) Uint16Pnt;
								Mxaloadheader->num_anim_vertices[frame][group][execbuf][texgroup] = num_anim_vertices;
								Mxaloadheader->frame_pnts[frame][group][execbuf][texgroup] = (MXAVERT*) Buffer;
								lpLVERTEX = 	( LPLVERTEX ) ( lpLVERTEX +( (u_int32_t) Mxaloadheader->Group[group].texture_group_vert_off[execbuf][texgroup]));
								lpLVERTEX += 8;
								Buffer += ( num_anim_vertices * sizeof( MXAVERT ) );
							}

							if (!(FSUnlockVertexBuffer(&Mxaloadheader->Group[group].renderObject[execbuf])))
							{
								Msg( "Mxaload : Unlock VertexBuffer failed\n" );
								return false ;
							}
						}
					}
				}

				/* Allocate a single aligned buffer and pack MXAVERT arrays
				 * consecutively (without the u16 count-fields that appear
				 * between them in the raw file).  malloc returns 8-byte
				 * aligned; MXAVERT is 32 bytes; so every packed array is
				 * 4-byte aligned and vldr in InterpFrames is safe.
				 *
				 * Important: the previous implementation preserved raw
				 * offsets, which meant a frame_pnts that landed at
				 * BufferStart+2 stayed at aligned+2 — still 2-mod-4 and
				 * still crashing on ARM11. Packing fixes that. */
				{
					size_t total = 0;
					for( frame=0 ; frame<Mxaloadheader->num_frames; frame++)
						for( group=0 ; group<Mxaloadheader->num_groups; group++)
							for( execbuf=0 ; execbuf<Mxaloadheader->Group[group].num_execbufs; execbuf++)
								for( texgroup=0; texgroup < Mxaloadheader->Group[group].num_texture_groups[execbuf] ; texgroup++)
									total += (size_t)Mxaloadheader->num_anim_vertices[frame][group][execbuf][texgroup]
									         * sizeof(MXAVERT);

					if ( total )
					{
						char *aligned = (char*)malloc( total );
						if ( aligned )
						{
							char *dst = aligned;
							Mxaloadheader->aligned_verts = aligned;

							for( frame=0 ; frame<Mxaloadheader->num_frames; frame++)
							{
								for( group=0 ; group<Mxaloadheader->num_groups; group++)
								{
									for( execbuf=0 ; execbuf<Mxaloadheader->Group[group].num_execbufs; execbuf++)
									{
										for( texgroup=0; texgroup < Mxaloadheader->Group[group].num_texture_groups[execbuf] ; texgroup++)
										{
											size_t nbytes =
												(size_t)Mxaloadheader->num_anim_vertices[frame][group][execbuf][texgroup]
												* sizeof(MXAVERT);
											/* Source is the raw pointer (possibly 2-mod-4); use
											   byte-wise copy so GCC doesn't fuse it to `ldm`. */
											memcpy_unaligned( dst,
												Mxaloadheader->frame_pnts[frame][group][execbuf][texgroup],
												nbytes );
											Mxaloadheader->frame_pnts[frame][group][execbuf][texgroup] =
												(MXAVERT*) dst;
											dst += nbytes;
										}
									}
								}
							}
						}
					}
				}
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
	Uint16Pnt = (u_int16_t *) Buffer;
	if( *Uint16Pnt++ )
	{
		NumFirePoints = *Uint16Pnt++;

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
				FirePointPtr->ID = *Uint16Pnt++;				// ID ( u_int16_t )
				FloatPnt = (float *) Uint16Pnt;

				memcpy_unaligned(&FirePointPtr->Pos, FloatPnt, 4*3);				// Pos ( 3 floats )
				FloatPnt+=3;
				memcpy_unaligned(&FirePointPtr->Dir, FloatPnt, 3*4);				// Dir ( 3 floats )
				FloatPnt+=3;
				memcpy_unaligned(&FirePointPtr->Up, FloatPnt, 3*4);				// Up ( 3 floats )
				FloatPnt+=3;
				Uint16Pnt = (u_int16_t *) FloatPnt;
				FirePointPtr++;
			}
		}
		else
		{
			Mxaloadheader->FirePoints = NULL;
			Mxaloadheader->NumFirePoints = 0;
		}

		NumSpotFX = *Uint16Pnt++;

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
				SpotFXPtr->Type = *Uint16Pnt++;				// Type ( u_int16_t )
				FloatPnt = (float *) Uint16Pnt;

				memcpy_unaligned(&SpotFXPtr->Pos, FloatPnt, 4*3);
				FloatPnt+=3;
				memcpy_unaligned(&SpotFXPtr->DirVector.x, FloatPnt, 4*3);
				FloatPnt+=3;
				memcpy_unaligned(&SpotFXPtr->UpVector, FloatPnt, 4*3);
				FloatPnt+=3;
				memcpy(&SpotFXPtr->StartDelay, FloatPnt++, 4); SpotFXPtr->StartDelay*= ANIM_SECOND;	// Start Delay
				memcpy(&SpotFXPtr->ActiveDelay, FloatPnt++,4); SpotFXPtr->ActiveDelay*= ANIM_SECOND;	// Active Delay
				memcpy(&SpotFXPtr->InactiveDelay, FloatPnt++, 4); SpotFXPtr->InactiveDelay*= ANIM_SECOND;	// Inactive Delay
				Int16Pnt = (int16_t *) FloatPnt;
				SpotFXPtr->Primary = (int8_t) *Int16Pnt++;	// Primary ( int16_t )
				SpotFXPtr->Secondary = (int8_t) *Int16Pnt++;	// Secondary ( int16_t )
				Uint32Pnt = (u_int32_t *) Int16Pnt;
				Colour = *Uint32Pnt++;

#if	MXA_VERSION_NUMBER == 2

				Int8Pnt = (int8_t *) Uint32Pnt;

				c = 0;
				do
				{
					SFXFilename[ c ] = *Int8Pnt++;
					c++;
				}
				while( SFXFilename[ ( c - 1 ) ] != 0 );
				
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

				FloatPnt = (float *) Int8Pnt;
				memcpy(&SpotFXPtr->SoundFXVolume, FloatPnt++, 4);
				memcpy(&SpotFXPtr->SoundFXSpeed, FloatPnt++, 4);
				Uint16Pnt = (u_int16_t *) FloatPnt;
#else
				Uint16Pnt = (u_int16_t *) Uint32Pnt;
#endif

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

		Buffer = (char *) Uint16Pnt;
	}
	else
	{
		Mxaloadheader->NumFirePoints = 0;
		Mxaloadheader->FirePoints = NULL;
		Mxaloadheader->NumSpotFX = 0;
		Mxaloadheader->SpotFX = NULL;
		Buffer = (char *) ( Uint16Pnt + 1 );
	}

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
#ifdef RENDERER_C3D
					{ c3d_set_object_proctex(true); }
#endif
					bool ok = draw_object(&Mxaloadheader->Group[group].renderObject[i]);
#ifdef RENDERER_C3D
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

		if ( Mxaloadheader->aligned_verts )
			free( Mxaloadheader->aligned_verts );
		Mxaloadheader->aligned_verts = NULL;
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
	u_int16_t		*	Uint16Pnt;
	int			i;
	char	*	FileNamePnt;
	MXALOADHEADER * Mxaloadheader;
	u_int32_t		*	Uint32Pnt;
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

	Uint32Pnt = (u_int32_t *) Buffer;
	MagicNumber = *Uint32Pnt++;
	VersionNumber = *Uint32Pnt++;
	Buffer = (char *) Uint32Pnt;

	if( ( MagicNumber != MAGIC_NUMBER ) || ( VersionNumber != MXA_VERSION_NUMBER  ) )
	{
		Msg( "PreMxaload() Incompatible model( .MXA ) file %s", Filename );
		return( false );
	}

	Uint16Pnt = (u_int16_t *) Buffer;

	Mxaloadheader->num_texture_files = *Uint16Pnt++;
	Buffer = (char *) Uint16Pnt;		

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
			FileNamePnt = (char *) &Mxaloadheader->ImageFile[i];
			while ( ( *FileNamePnt++ = *Buffer++ ) != 0 );
	
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

	Mxaloadheader->Buffer = Buffer;

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
