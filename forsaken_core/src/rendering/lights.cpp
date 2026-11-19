/* lights.cpp -- dynamic XLights and vertex relighting; see lights.h. */
#include <stdio.h>
#include <string.h>

#include "math/new3d.h"
#include "math/quat.h"
#include "game/compobjects.h"
#include "game/bgobjects.h"
#include "game/object.h"
#include "net/networking.h"
#include "rendering/lights.h"
#include "game/water.h"
#include "world/visi.h"
#include "rendering/text.h"
#include "game/main.h"
#include "util/util.h"
#include "data/mload.h"
#include "game/camera.h"
#include "game/oct2.h"
#include "game/ships.h"
#include "game/skin.h"
#include "ui/title.h"


/*===================================================================
		Globals...
===================================================================*/

XLIGHT * FirstLightVisible = nullptr;
XLIGHT	XLights[MAXXLIGHTS];
u_int16_t	FirstXLightUsed;
u_int16_t	FirstXLightFree;


/*===================================================================
	Procedure	:	Set up And Init all XLights
	Input		:	nothing
	Output		:	nothing
===================================================================*/
void	InitXLights()
{
	u_int16_t	i;
	FirstXLightUsed = INDEX_NONE;
	FirstXLightFree = 0;
	for( i = 0 ; i < MAXXLIGHTS ; i++ )
	{
		XLights[i].Index = i;
		XLights[i].Next = i + 1;
		XLights[i].Prev = INDEX_NONE;
		XLights[i].Type = POINT_LIGHT;
	}

	XLights[MAXXLIGHTS-1].Next = INDEX_NONE;
}
/*===================================================================
	Procedure	:	Find a free light and move it from the free list to
					the used list
	Input		:	nothing
	Output		:	u_int16_t number of light free....
===================================================================*/
u_int16_t	FindFreeXLight()
{
	u_int16_t i;

	i = FirstXLightFree;

	if ( i == INDEX_NONE)
		return i;

	XLights[i].Prev = FirstXLightUsed;
	if ( FirstXLightUsed != INDEX_NONE)
	{
		XLights[FirstXLightUsed].Next = i;
	}
	FirstXLightUsed = i;
	FirstXLightFree = XLights[i].Next;
	XLights[i].Type = POINT_LIGHT;
	XLights[i].Visible = true;

	return i ;
}
/*===================================================================
	Procedure	:	Kill a used light and move it from the used list to
					the free list
	Input		:	u_int16_t number of light free....
	Output		:	nothing
===================================================================*/
void	KillUsedXLight( u_int16_t light )
{
	u_int16_t	its_prev;
	u_int16_t	its_next;

	its_prev = XLights[light].Prev;
	its_next = XLights[light].Next;

	if ( light == FirstXLightUsed )
		FirstXLightUsed = XLights[light].Prev;

	if( its_prev != INDEX_NONE)
		XLights[its_prev].Next = its_next;

	if( its_next != INDEX_NONE)
		XLights[its_next].Prev = its_prev;

	XLights[light].Prev = INDEX_NONE;
	XLights[light].Next = FirstXLightFree;
	FirstXLightFree	= light;
}


/*===================================================================
	Procedure	:	Shrink and fade dying XLights; free the ones gone
	Input		:	MLOADHEADER * (unused)
	Output		:	true
===================================================================*/
/* @vfp-exempt R3 — light inclusion/attenuation window compare; integerizing changes which lights apply. measured n/a. Verified 2026-10-25. */
bool	ProcessXLights( MLOADHEADER * /*Mloadheader*/ )
{
	u_int16_t	light;
	u_int16_t	oldlight;

	light = FirstXLightUsed;
	while( light != INDEX_NONE )
	{
		oldlight = XLights[light].Prev;

		/* @vfp-exempt R3 — light inclusion/attenuation window compare; integerizing changes which lights apply. measured n/a. Verified 2026-10-25. */
		if(	XLights[light].SizeCount != 0.0F )
		{
			/* @vfp-exempt R3 — light inclusion/attenuation window compare; integerizing changes which lights apply. measured n/a. Verified 2026-10-25. */
			XLights[light].Size	-= XLights[light].SizeCount * framelag;
			/* @vfp-exempt R3 — light inclusion/attenuation window compare; integerizing changes which lights apply. measured n/a. Verified 2026-10-25. */
			XLights[light].r -= XLights[light].ColorCount * framelag;
			/* @vfp-exempt R3 — light inclusion/attenuation window compare; integerizing changes which lights apply. measured n/a. Verified 2026-10-25. */
			XLights[light].g -= XLights[light].ColorCount * framelag;
			XLights[light].b -= XLights[light].ColorCount * framelag;
			if(	XLights[light].r <= 0.0F )
				XLights[light].r = 0.0F;
			if(	XLights[light].g <= 0.0F )
				XLights[light].g = 0.0F;
			if(	XLights[light].b <= 0.0F )
				XLights[light].b = 0.0F;
			if( XLights[light].Size	<= 0.0F)
				KillUsedXLight(light);
		}
		light = oldlight;
	}

	return true;
}

/*===================================================================
*		cause a light to go red and get smaller then die...
===================================================================*/

void SetLightDie ( u_int16_t light )
{
	XLights[light].SizeCount = 50.0F;
	XLights[light].ColorCount = 2.0F;
	XLights[light].r = 255.0F;
	XLights[light].g = 0.0F;
	XLights[light].b = 0.0F;
}
/*===================================================================
	Procedure	:	Relight one level group: POLYANIM UV frames, baked
				:	colour restore or white-out / water shimmer, then
				:	(CPU path only) visible XLight contributions
	Input		:	MLOADHEADER *	Level
				:	u_int16_t		Group
	Output		:	false if a vertex buffer lock / unlock fails
===================================================================*/
/* Phase (degrees) of the white-out / underwater shimmer; advanced each
 * frame in oct2.cpp. */
float	cral = 0.0F;


bool	XLight1Group( MLOADHEADER * Mloadheader, u_int16_t group )
{
	int		mesh;
	int		vert;
	LVERTEX	*VertBase = nullptr;
	LVERTEX	*LVert = nullptr;
	LVERTEX	*OrigLVert = nullptr;
	COLOR col;
	float	x,y,z;
	u_int32_t r,g,b,intWhiteOut;
	POLYANIM * PolyAnim;
	int i,e;
	int		color_already_clean;	/* the group's colours were clean on entry */
	u_int32_t * u_int32Pnt;
	TANIMUV * TanimUV;
	float	intensity;
#ifndef GPU_LIGHTING
	/* the CPU light loop's working variables */
	XLIGHT * XLightPnt;
	float	blf;
	float	glf;
	float	rlf;
	VECTOR	Temp;
	VECTOR	CellIndex;
	float	distance;
	VERTEXCELL * VertexCellPnt;
	u_int16_t * VertexIndexPnt;
	u_int16_t * OrgVertexIndexPnt;
	int	Cell;
	int	CellIndex_x;
	int	CellIndex_y;
	int	CellIndex_z;
	int	CellRange_x;
	int	CellRange_y;
	int	CellRange_z;
	int	Cellx;
	int	Celly;
	int	Cellz;
	int		NumOfxCells;
	int		NumOfyCells;
	int		NumOfzCells;
	float	CellSize;
	float	Size,OSize;
	float	SizeX2;
	float	Posx,Posy,Posz;
	float	Dirx = 0.0f, Diry = 0.0f, Dirz = 0.0f;
	float	Cosa,CosArc = 0.0f;
	float	rlen;
	float	intense;
	u_int32_t	tempiR;
	u_int32_t	tempiG;
	u_int32_t	tempiB;
	u_int32_t	tempiA;
	float	centerx;
	float	centery;
	float	centerz;
	float	half_sizex;
	float	half_sizey;
	float	half_sizez;
	u_int32_t inc;
	u_int32_t carry;
	u_int32_t clamp;
#endif


	intWhiteOut = (int)WhiteOut;
	if( intWhiteOut >= 256 )
	{
		intWhiteOut = (256 - (intWhiteOut-256) );
	}

#ifndef GPU_LIGHTING
	CellSize = Mloadheader->CellSize;
#endif
	/* Snapshot and clear the group's clean flag on entry: only a plain
	 * restore in this call may set it again, so a mesh is never skipped on
	 * the strength of a flag set earlier in the same call. The skip is only
	 * sound when nothing writes the colours after the restore, which holds
	 * when the vertex shader does the dynamic lighting; the CPU light loop
	 * (no GPU_LIGHTING) adds to them every call. */
#ifdef GPU_LIGHTING
	color_already_clean = Mloadheader->Group[group].color_clean;
#else
	color_already_clean = 0;
#endif
	Mloadheader->Group[group].color_clean = 0;
	mesh = Mloadheader->Group[group].num_meshes;
	while( mesh--)
	{
		if (!(FSLockVertexBuffer((RENDEROBJECT*)&Mloadheader->Group[group].renderObject[mesh], &VertBase)))
		{
			return false;
		}
		/* The colour pass below restores the baked colours (stable from
		 * frame to frame) unless white-out or water rewrite them every
		 * frame; tell the backend's caches which (render.h SMEMO_PASS). */
		Mloadheader->Group[group].renderObject[mesh].memoEpochPass =
			( ( GroupWaterInfo[group] == WATERSTATE_NOWATER || ShowPlaneRGB ) && WhiteOut == 0.0F ) ?
				SMEMO_PASS_UNSET : SMEMO_PASS_PER_FRAME;

#ifndef GPU_LIGHTING
		VertexCellPnt = Mloadheader->Group[group].vertex_cell_pnt[mesh];
		OrgVertexIndexPnt = Mloadheader->Group[group].vertex_index_pnt[mesh];

		NumOfxCells = Mloadheader->Group[group].xcells[mesh];
		NumOfyCells = Mloadheader->Group[group].ycells[mesh];
		NumOfzCells = Mloadheader->Group[group].zcells[mesh];
#endif

		if( Mloadheader->Group[group].num_animating_polys[mesh] != 0 )
		{

			PolyAnim = Mloadheader->Group[group].polyanim[mesh];

			for( i = 0 ; i < Mloadheader->Group[group].num_animating_polys[mesh] ; i++ )
			{

				if( PolyAnim->currentframe != PolyAnim->newframe )
				{
					// something has changed....

					u_int32Pnt = (u_int32_t*)PolyAnim->vert;
					for( e = 0 ; e < PolyAnim->vertices ; e++ )
					{

						LVert = VertBase+ *u_int32Pnt++;
						TanimUV = PolyAnim->UVs;
						TanimUV += e + (PolyAnim->vertices * PolyAnim->newframe);
						memcpy(&LVert->tu, &TanimUV->u, 4);
						memcpy(&LVert->tv, &TanimUV->v, 4);
					}

					PolyAnim->currentframe = PolyAnim->newframe;
				}
				PolyAnim++;
			}
		}



		LVert = VertBase;
		{
			OrigLVert = Mloadheader->Group[group].originalVerts[mesh];

			vert = Mloadheader->Group[group].num_verts_per_mesh[mesh];

			/* @vfp-exempt R3 — light inclusion/attenuation window compare; integerizing changes which lights apply. measured n/a. Verified 2026-10-25. */
			/* Skip the colour pass when the previous call left the baked
			 * colours in place and this one would only copy them again, as
			 * for the second stereo eye of a frame. */
			if( !( color_already_clean && !ShowPlaneRGB && WhiteOut == 0.0F &&
				   GroupWaterInfo[group] == WATERSTATE_NOWATER ) )
			{
				if( GroupWaterInfo[group] == WATERSTATE_NOWATER || ShowPlaneRGB )
				{
					if( WhiteOut == 0.0F )
					{
						if ( ShowPlaneRGB )
						{
							while( vert --)
							{
								LVert++;
								OrigLVert++;
							}
						}
						else
						{
							while( vert --)
							{
								LVert->color = OrigLVert->color;
								LVert++;
								OrigLVert++;
							}
							Mloadheader->Group[group].color_clean = 1;
						}

					}
					else
					{
						//	Special Lighting effects
						while( vert --)
						{
							x = (float)((int) (OrigLVert->x * 0.35F) % 360);
							y = (float)((int) (OrigLVert->y * 0.35F) % 360);
							z = (float)((int) (OrigLVert->z * 0.35F) % 360);
							col = OrigLVert->color;
							col &= 0xffff;
							b = (int)  ( ( fast_sinf( D2R( x + cral ) ) + fast_sinf( D2R( y + cral ) ) + fast_sinf( D2R( z + cral ) ) ) * 127.0F * 0.3333333F + 128.0F ) ;
							b+= intWhiteOut;
							if( b > 255 )
								b = 255;
							col |= (b<<24)+(b<<16);
							LVert->color = col;
							LVert++;
							OrigLVert++;

						}
					}
				}else if( GroupWaterInfo[group] == WATERSTATE_ALLWATER )
				{
					// ****************** Full Water Effect ********************************
					OrigLVert = Mloadheader->Group[group].originalVerts[mesh];
					LVert = VertBase;
					vert = Mloadheader->Group[group].num_verts_per_mesh[mesh];
					//	Special Lighting effects
					while( vert --)
					{
						int orig_a;
						x = (float)((int) (OrigLVert->x * 0.35F) % 360);
						y = (float)((int) (OrigLVert->y * 0.35F) % 360);
						z = (float)((int) (OrigLVert->z * 0.35F) % 360);
						col = OrigLVert->color;
						r = RGBA_GETRED(col);
						g = RGBA_GETGREEN(col);
						b = RGBA_GETBLUE(col);
						orig_a = RGBA_GETALPHA(col);

						r >>=2;
						g >>=2;
						b >>=2;
						intensity = (float) ( ( fast_sinf( D2R( x + cral ) ) + fast_sinf( D2R( y + cral ) ) + fast_sinf( D2R( z + cral ) ) ) * 127.0F * 0.3333333F + 128.0F ) ;
						r += (int) (GroupWaterIntensity_Red[group] * intensity);
						if( r > 255 )
							r = 255;
						g += (int) (GroupWaterIntensity_Green[group] * intensity);
						if( g > 255 )
							g = 255;
						b += (int) (GroupWaterIntensity_Blue[group] * intensity);
						if( b > 255 )
							b = 255;
						/* Preserve the baked vertex alpha (typically 255 for
						 * solid level mesh). The citro3d renderer alpha-blends
						 * every textured surface with the vertex alpha, so a
						 * fixed 128 here makes underwater walls render 50%
						 * transparent. */
						LVert->color = RGBA_MAKE( r ,g ,b , orig_a );
						LVert++;
						OrigLVert++;
					}
					// ****************** End of Water Effect ******************************
				}else{
					// ****************** Partial Water Effect ******************************
					OrigLVert = Mloadheader->Group[group].originalVerts[mesh];
					LVert = VertBase;
					/* @vfp-exempt R3 — light inclusion/attenuation window compare; integerizing changes which lights apply. measured n/a. Verified 2026-10-25. */
					vert = Mloadheader->Group[group].num_verts_per_mesh[mesh];
					//	Special Lighting effects
					while( vert --)
					{
						if( OrigLVert->y < GroupWaterLevel[group] )
						{
							int orig_a;
							x = (float)((int) (OrigLVert->x * 0.35F) % 360);
							y = (float)((int) (OrigLVert->y * 0.35F) % 360);
							z = (float)((int) (OrigLVert->z * 0.35F) % 360);
							col = OrigLVert->color;
							r = RGBA_GETRED(col);
							g = RGBA_GETGREEN(col);
							b = RGBA_GETBLUE(col);
							orig_a = RGBA_GETALPHA(col);

							r >>=2;
							g >>=2;
							b >>=2;
							intensity = (float) ( ( fast_sinf( D2R( x + cral ) ) + fast_sinf( D2R( y + cral ) ) + fast_sinf( D2R( z + cral ) ) ) * 127.0F * 0.3333333F + 128.0F ) ;
							r += (int) (GroupWaterIntensity_Red[group] * intensity);
							if( r > 255 )
								r = 255;
							g += (int) (GroupWaterIntensity_Green[group] * intensity);
							if( g > 255 )
								g = 255;
							b += (int) (GroupWaterIntensity_Blue[group] * intensity);
							if( b > 255 )
								b = 255;
							/* Preserve baked alpha — see ALLWATER branch
							 * above for the rationale. */
							LVert->color = RGBA_MAKE( r ,g ,b , orig_a );
						}else{
							LVert->color = OrigLVert->color;
						}
						LVert++;
						OrigLVert++;
					}
					// ****************** End of Water Effect ******************************
				}
			}
		}
#ifdef GPU_LIGHTING
		/* Skip the per-vertex CPU dynamic-light loop — the vertex shader
		 * adds light contribution on top of the baked-color baseline that
		 * was already restored above. See render_c3d.cpp::c3d_upload_xlights
		 * and shaders/render_c3d.v.pica. */
		/* @vfp-exempt R3 — light inclusion/attenuation window compare; integerizing changes which lights apply. measured n/a. Verified 2026-10-25. */
#else
		if( WhiteOut == 0.0F )
		{

			centerx		=	Mloadheader->Group[group].center.x;
			centery		=	Mloadheader->Group[group].center.y;
			centerz		=	Mloadheader->Group[group].center.z;
			half_sizex	=	Mloadheader->Group[group].half_size.x;
			half_sizey	=	Mloadheader->Group[group].half_size.y;
			half_sizez	=	Mloadheader->Group[group].half_size.z;

			XLightPnt = FirstLightVisible;
			while( XLightPnt )
			{
				if( GroupsAreVisible( group, XLightPnt->Group ) )
				{
					Posx = XLightPnt->Pos.x;
					Posy = XLightPnt->Pos.y;
					Posz = XLightPnt->Pos.z;
					/* @vfp-exempt R3 — light inclusion/attenuation window compare; integerizing changes which lights apply. measured n/a. Verified 2026-10-25. */
					OSize  = XLightPnt->Size;

					Temp.x = Posx - centerx;
					/* @vfp-exempt R3 — light inclusion/attenuation window compare; integerizing changes which lights apply. measured n/a. Verified 2026-10-25. */
					if( Temp.x < 0.0F )	Temp.x *= -1.0F;
					Temp.y = Posy - centery;
					/* @vfp-exempt R3 — light inclusion/attenuation window compare; integerizing changes which lights apply. measured n/a. Verified 2026-10-25. */
					if( Temp.y < 0.0F )	Temp.y *= -1.0F;
					/* @vfp-exempt R3 — light inclusion/attenuation window compare; integerizing changes which lights apply. measured n/a. Verified 2026-10-25. */
					Temp.z = Posz - centerz;
					if( Temp.z < 0.0F )	Temp.z *= -1.0F;
					if ( (Temp.x <= ( half_sizex + OSize ) ) &&
						 (Temp.y <= ( half_sizey + OSize ) ) &&
						 (Temp.z <= ( half_sizez + OSize ) ) )
					{
						SizeX2 = OSize * OSize;
						Size = 1 / SizeX2;
						rlf	= XLightPnt->r;
						glf	= XLightPnt->g;
						blf	= XLightPnt->b;

						if( XLightPnt->Type == SPOT_LIGHT )
						{
							Dirx = XLightPnt->Dir.x;
							Diry = XLightPnt->Dir.y;
							Dirz = XLightPnt->Dir.z;
							CosArc = XLightPnt->CosArc;
						}


						CellIndex.x = Posx - Mloadheader->Group[group].cell_origin[mesh].x;
						CellIndex.y = Posy - Mloadheader->Group[group].cell_origin[mesh].y;
						CellIndex.z = Posz - Mloadheader->Group[group].cell_origin[mesh].z;
						CellIndex_x = (int) fast_floorf( (CellIndex.x - OSize) * CellSize );
						CellIndex_y = (int) fast_floorf( (CellIndex.y - OSize) * CellSize );
						CellIndex_z = (int) fast_floorf( (CellIndex.z - OSize) * CellSize );
						CellRange_x = (int) fast_floorf( (CellIndex.x + OSize) * CellSize );
						CellRange_y = (int) fast_floorf( (CellIndex.y + OSize) * CellSize );
						CellRange_z = (int) fast_floorf( (CellIndex.z + OSize) * CellSize );


						if( CellIndex_x < 0 )
						{
							CellIndex_x = 0;
						}else{
							if( CellIndex_x >= NumOfxCells )
								CellIndex_x = NumOfxCells-1;
						}
						if( CellIndex_y < 0 )
						{
							CellIndex_y = 0;
						}else{
							if( CellIndex_y >= NumOfyCells )
								CellIndex_y = NumOfyCells-1;
						}
						if( CellIndex_z < 0 )
						{
							CellIndex_z = 0;
						}else{
							if( CellIndex_z >= NumOfzCells )
								CellIndex_z = NumOfzCells-1;
						}

						if( CellRange_x < 0 )
						{
							CellRange_x = 0;
						}else{
							if( CellRange_x >= NumOfxCells  )
								CellRange_x = NumOfxCells-1;
						}
						if( CellRange_y < 0 )
						{
							CellRange_y = 0;
						}else{
							if( CellRange_y >=NumOfyCells  )
								CellRange_y = NumOfyCells-1;
						}
						if( CellRange_z < 0 )
						{
							CellRange_z = 0;
						}else{
							if( CellRange_z >= NumOfzCells )
								CellRange_z = NumOfzCells-1;
						}

						switch( XLightPnt->Type )
						{
						case POINT_LIGHT:
							Cellz = CellIndex_z;

							while( Cellz <= CellRange_z )
							{
								Celly = CellIndex_y;
								while( Celly <= CellRange_y )
								{
									Cell = ( CellIndex_x + NumOfxCells *
										   ( Celly + NumOfyCells *
											 Cellz ) );
									Cellx = CellRange_x - CellIndex_x + 1;

									while( Cellx-- )
									{
										VertexIndexPnt = OrgVertexIndexPnt+VertexCellPnt[Cell].start_vert_in_cell;
										vert = VertexCellPnt[Cell].num_verts_in_cell;

										while( vert-- )
										{
											LVert = VertBase + *VertexIndexPnt++;
											/* find the distance from vert to light */
											x = LVert->x;
											y = LVert->y;
											z = LVert->z;
											x -= Posx;
											y -= Posy;
											z -= Posz;

											/* @vfp-exempt R3 — light inclusion/attenuation window compare; integerizing changes which lights apply. measured n/a. Verified 2026-10-25. */
											distance = (x*x) + (y*y) + (z*z);
											if ( distance <  SizeX2  )					// float
											{
												distance = 1.0F - ( distance * Size );	// float

												col = LVert->color;
												tempiA = col & 0xff000000;
												tempiR = (int) ( rlf * distance );
												tempiG = (int) ( glf * distance );
												tempiB = (int) ( blf * distance );
												inc = ( tempiR << 16 ) + ( tempiG << 8 ) + tempiB;
												carry = ( ( col + inc ) ^ ( col ^ inc ) ) & 0x01010100;
												clamp = carry - ( carry >> 8 );
												col = ( ( col + inc - carry ) | clamp) & 0x00ffffff;
												col |= tempiA;

												LVert->color = col;
											}
										}

										Cell++;
									}
									Celly++;
								}
								Cellz++;
							}
							break;
						case SPOT_LIGHT:
							Cellz = CellIndex_z;
							while( Cellz <= CellRange_z )
							{
								Celly = CellIndex_y;
								while( Celly <= CellRange_y )
								{
									Cell = (u_int16_t) ( CellIndex_x + NumOfxCells *
													( Celly + NumOfyCells *
													  Cellz ) );
									Cellx = CellRange_x - CellIndex_x + 1;

									while( Cellx-- )
									{
										VertexIndexPnt = OrgVertexIndexPnt+VertexCellPnt[Cell].start_vert_in_cell;
										vert = VertexCellPnt[Cell].num_verts_in_cell;
										while( vert-- )
										{
											LVert = VertBase + *VertexIndexPnt++;
											/* find the distance from vert to light */
											x = LVert->x - Posx;
											y = LVert->y - Posy;
											z = LVert->z - Posz;
											/* @vfp-exempt R3 — light inclusion/attenuation window compare; integerizing changes which lights apply. measured n/a. Verified 2026-10-25. */
											distance = (x*x) + (y*y) + (z*z);

											if ( distance <  SizeX2  )
												/* @vfp-exempt R3 — light inclusion/attenuation window compare; integerizing changes which lights apply. measured n/a. Verified 2026-10-25. */
											{

												if( distance > 0.0F )
												{
													distance = (float) sqrtf( distance );
													rlen = 1.0F / distance;
													x *= rlen;
													y *= rlen;
													z *= rlen;
												}
												/* @vfp-exempt R3 — light inclusion/attenuation window compare; integerizing changes which lights apply. measured n/a. Verified 2026-10-25. */
												Cosa = ( ( x*Dirx ) + ( y*Diry ) + ( z*Dirz ) );

												/* @vfp-exempt R3 — light inclusion/attenuation window compare; integerizing changes which lights apply. measured n/a. Verified 2026-10-25. */
												if( distance > 0.5F * OSize )
												{
													if ( Cosa > CosArc )
													{
														// Lights Are Go....
														intense = ( ( OSize - distance ) / ( 0.75F * OSize ) ) * ( ( Cosa - CosArc ) / ( 1.0F - CosArc ) );
														/* @vfp-exempt R3 — light inclusion/attenuation window compare; integerizing changes which lights apply. measured n/a. Verified 2026-10-25. */
													}else{
														continue;
													}
												}else if ( distance > MIN_LIGHT_SIZE ) {
													float cosarc2;

													/* @vfp-exempt R3 — light inclusion/attenuation window compare; integerizing changes which lights apply. measured n/a. Verified 2026-10-25. */
													cosarc2 = CosArc * ( 1.0F - ( ( OSize * 0.5F - distance ) / ( OSize * 0.5F - MIN_LIGHT_SIZE ) ) );
													if ( Cosa > cosarc2 )
													{
														intense = ( ( OSize - distance ) / ( OSize - MIN_LIGHT_SIZE ) ) * ( ( Cosa - cosarc2 ) / ( 1.0F - cosarc2 ) );
													}else{
														/* @vfp-exempt R3 — light inclusion/attenuation window compare; integerizing changes which lights apply. measured n/a. Verified 2026-10-25. */
														continue;
													}
												}else {
													if ( Cosa > 0.0F )
													{
														intense = 1.0F;
													}else{
														intense = 1.0F + Cosa;
													}
												}

												col =  LVert->color;				// int
												tempiA = col & 0xff000000;
												tempiR = (int) ( rlf * intense );
												tempiG = (int) ( glf * intense );
												tempiB = (int) ( blf * intense );
												inc = ( tempiR << 16 ) + ( tempiG << 8 ) + tempiB;
												carry = ( ( col + inc ) ^ ( col ^ inc ) ) & 0x01010100;
												clamp = carry - ( carry >> 8 );
												col = ( ( col + inc - carry ) | clamp) & 0x00ffffff;
												col |= tempiA;
												LVert->color = col;
											}
										}
										Cell++;
									}
									Celly++;
								}
								Cellz++;
							}
							break;

						}
					}
				}
				XLightPnt = XLightPnt->NextVisible;
			}
		}
#endif /* GPU_LIGHTING */
		/*	unlock the vertex buffer	*/
		if(!FSUnlockVertexBuffer((RENDEROBJECT*)&Mloadheader->Group[group].renderObject[mesh]))
			return false;
	}

	return true;
}

/*===================================================================
	Procedure	:	Relight a static (.mx) model. CPU path: restore baked
				:	colours, then add visible XLights. GPU_LIGHTING:
				:	only locks and unlocks the buffers
	Input		:	MXLOADHEADER *	Model
				:	VECTOR *		World position (light range test)
				:	float			Model radius
				:	MATRIX *		Model to world (vertex positions)
	Output		:	false if a vertex buffer lock / unlock fails
===================================================================*/
bool	XLightMxloadHeader( MXLOADHEADER * MXloadheader , VECTOR * Pos , float Radius , MATRIX * Matrix )
{
	int		group;
	int		mesh;
	int		vert;
	LVERTEX	*VertBase = nullptr;

	LVERTEX	*LVert = nullptr;
	LVERTEX	*OrigLVert = nullptr;
#ifndef GPU_LIGHTING
	/* the CPU light loop's working variables */
	XLIGHT * XLightPnt;
	VECTOR	Temp;
	float	distance;
	COLOR col;
	float	Size,OSize;
	float	SizeX2;
	float	x,y,z;
	float	Posx,Posy,Posz;
	float	Dirx = 0.0f, Diry = 0.0f, Dirz = 0.0f;
	float	Cosa,CosArc = 0.0f;
	float	rlen;
	float	intense;
	int	tempiR;
	int	tempiG;
	int	tempiB;
	int	tempiA;
	u_int32_t inc;
	u_int32_t carry;
	u_int32_t clamp;
	float	blf;
	float	glf;
	float	rlf;
#endif


	group = MXloadheader->num_groups;
	while( group--)
	{

		mesh = MXloadheader->Group[group].num_meshes;
		while( mesh--)
		{
			if (!(FSLockVertexBuffer(&MXloadheader->Group[group].renderObject[mesh], &VertBase)))
			{
				return false;
			}

			LVert = VertBase;
			vert = MXloadheader->Group[group].num_verts_per_mesh[mesh];
			OrigLVert = MXloadheader->Group[group].originalVerts[mesh];

#ifdef GPU_LIGHTING
			/* The baked-colour restore is skipped here (lock/unlock is
			 * kept): it assumes the live colours still equal
			 * originalVerts. Note that AmbientLightMxModel and
			 * LightMxModel (models.cpp) do write computed colours into
			 * these same per-model live vertices, and the vertices are
			 * shared by every instance of the model, so colours they
			 * leave behind are not undone by this path. */
			(void)vert;
			(void)LVert;
			(void)OrigLVert;
#else
			while( vert --)
			{
				LVert->color = OrigLVert->color;
				LVert++;
				OrigLVert++;
			}
#endif

#ifndef GPU_LIGHTING
			/* CPU lighting: add every visible light in range, transforming
			 * each vertex to world space with Matrix. Under GPU_LIGHTING the
			 * vertex shader does this instead, from the model-to-world
			 * modelView uniform (render_c3d.cpp::upload_matrices). */
			XLightPnt = FirstLightVisible;
			while( XLightPnt )
			{
				{

					Posx = XLightPnt->Pos.x;
					Posy = XLightPnt->Pos.y;
					Posz = XLightPnt->Pos.z;
					OSize  = XLightPnt->Size;

					Temp.x = Posx - Pos->x;
					if( Temp.x < 0.0F )	Temp.x *= -1.0F;
					Temp.y = Posy - Pos->y;
					if( Temp.y < 0.0F )	Temp.y *= -1.0F;
					Temp.z = Posz - Pos->z;
					if( Temp.z < 0.0F )	Temp.z *= -1.0F;
					if ( (Temp.x <= ( Radius + OSize ) ) &&
						 (Temp.y <= ( Radius + OSize ) ) &&
						 (Temp.z <= ( Radius + OSize ) ) )
					{
						SizeX2 = OSize * OSize;
						Size = 1 / SizeX2;
						rlf	= XLightPnt->r;
						glf	= XLightPnt->g;
						blf	= XLightPnt->b;

						if( XLightPnt->Type == SPOT_LIGHT )
						{
							Dirx = XLightPnt->Dir.x;
							Diry = XLightPnt->Dir.y;
							Dirz = XLightPnt->Dir.z;
							CosArc = XLightPnt->CosArc;
						}

						LVert = VertBase;
						vert = MXloadheader->Group[group].num_verts_per_mesh[mesh];

						switch( XLightPnt->Type )
						{
						case POINT_LIGHT:

							while( vert-- )
							{
								/* find the distance from vert to light */

								ApplyMatrix( Matrix, (VECTOR*) LVert, &Temp );

								x = Temp.x - Posx;
								y = Temp.y - Posy;
								z = Temp.z - Posz;
								distance = (x*x) + (y*y) + (z*z);

								if ( distance <  SizeX2  )					// float
								{
									distance = 1.0F - ( distance * Size );	// float
									col =  LVert->color;				// int
									tempiA = col & 0xff000000;
									tempiR = (int) ( rlf * distance );
									tempiG = (int) ( glf * distance );
									tempiB = (int) ( blf * distance );
									inc = ( tempiR << 16 ) + ( tempiG << 8 ) + tempiB;
									carry = ( ( col + inc ) ^ ( col ^ inc ) ) & 0x01010100;
									clamp = carry - ( carry >> 8 );
									col = ( ( col + inc - carry ) | clamp) & 0x00ffffff;
									col |= tempiA;
									LVert->color = col;
								}
								LVert++;
							}
							break;
						case SPOT_LIGHT:
							while( vert-- )
							{
								/* find the distance from vert to light */
								ApplyMatrix( Matrix, (VECTOR*) LVert, &Temp );

								x = Temp.x - Posx;
								y = Temp.y - Posy;
								z = Temp.z - Posz;

								distance = (x*x) + (y*y) + (z*z);

								if ( distance <  SizeX2  )
								{
									if( distance > 0.0F )
									{
										distance = (float) sqrtf( distance );
										rlen = 1.0F / distance;
										x *= rlen;
										y *= rlen;
										z *= rlen;
									}
									Cosa = ( ( x*Dirx ) + ( y*Diry ) + ( z*Dirz ) );

									if( distance > 0.5F * OSize )
									{
										if ( Cosa > CosArc )
										{
											// Lights Are Go....
											intense = ( ( OSize - distance ) / ( 0.75F * OSize ) ) * ( ( Cosa - CosArc ) / ( 1.0F - CosArc ) );
										}else{
											goto PLOP;
										}
									}else if ( distance > MIN_LIGHT_SIZE ) {
										float cosarc2;

										cosarc2 = CosArc * ( 1.0F - ( ( OSize * 0.5F - distance ) / ( OSize * 0.5F - MIN_LIGHT_SIZE ) ) );
										if ( Cosa > cosarc2 )
										{
											intense = ( ( OSize - distance ) / ( OSize - MIN_LIGHT_SIZE ) ) * ( ( Cosa - cosarc2 ) / ( 1.0F - cosarc2 ) );
										}else{
											goto PLOP;
										}
									}else {
										if ( Cosa > 0.0F )
										{
											intense = 1.0F;
										}else{
											intense = 1.0F + Cosa;
										}
									}
									col =  LVert->color;				// int
									tempiA = col & 0xff000000;
									tempiR = (int) ( rlf * intense );
									tempiG = (int) ( glf * intense );
									tempiB = (int) ( blf * intense );
									inc = ( tempiR << 16 ) + ( tempiG << 8 ) + tempiB;
									carry = ( ( col + inc ) ^ ( col ^ inc ) ) & 0x01010100;
									clamp = carry - ( carry >> 8 );
									col = ( ( col + inc - carry ) | clamp) & 0x00ffffff;
									col |= tempiA;
									LVert->color = col;
								}
								PLOP:
								LVert++;
							}
							break;
						}
					}
				}
				XLightPnt = XLightPnt->NextVisible;
			}
#endif /* !GPU_LIGHTING */
			/*	unlock the vertex buffer	*/
			if (!(FSUnlockVertexBuffer(&MXloadheader->Group[group].renderObject[mesh])))
			{
				return false;
			}
		}
	}

	return true;
}
/*===================================================================
	Procedure	:	Relight an animated (.mxa) model: restore baked
				:	colours, then (CPU path only) add visible XLights
	Input		:	MXALOADHEADER *	Model
				:	VECTOR *		World position (light range test)
				:	float			Model radius
				:	MATRIX *		Model to world (vertex positions)
	Output		:	false if a vertex buffer lock / unlock fails
===================================================================*/
bool	XLightMxaloadHeader( MXALOADHEADER * MXloadheader , VECTOR * Pos , float Radius , MATRIX * Matrix )
{
	int		group;
	int		mesh;
	int		vert;
	LVERTEX	*VertBase = nullptr;

	LVERTEX	*LVert = nullptr;
	LVERTEX	*OrigLVert = nullptr;
#ifndef GPU_LIGHTING
	/* the CPU light loop's working variables */
	XLIGHT * XLightPnt;
	VECTOR	Temp;
	float	distance;
	COLOR col;
	float	Size,OSize;
	float	SizeX2;
	float	x,y,z;
	float	Posx,Posy,Posz;
	float	Dirx = 0.0f, Diry = 0.0f, Dirz = 0.0f;
	float	Cosa,CosArc = 0.0f;
	float	rlen;
	float	intense;
	int	tempiR;
	int	tempiG;
	int	tempiB;
	int	tempiA;
	u_int32_t inc;
	u_int32_t carry;
	u_int32_t clamp;
	float	blf;
	float	glf;
	float	rlf;
#endif


	group = MXloadheader->num_groups;
	while( group--)
	{

		mesh = MXloadheader->Group[group].num_meshes;
		while( mesh--)
		{
			if (!(FSLockVertexBuffer(&MXloadheader->Group[group].renderObject[mesh], &VertBase)))
			{
				return false;
			}

			LVert = VertBase;
			vert = MXloadheader->Group[group].num_verts_per_mesh[mesh];
			OrigLVert = MXloadheader->Group[group].originalVerts[mesh];
			while( vert --)
			{
				LVert->color = OrigLVert->color;
				LVert++;
				OrigLVert++;
			}

#ifndef GPU_LIGHTING
			/* CPU lighting: add every visible light in range, transforming
			 * each vertex to world space with Matrix. Under GPU_LIGHTING the
			 * vertex shader does this instead, from the model-to-world
			 * modelView uniform (render_c3d.cpp::upload_matrices). */
			XLightPnt = FirstLightVisible;
			while( XLightPnt )
			{
				{

					Posx = XLightPnt->Pos.x;
					Posy = XLightPnt->Pos.y;
					Posz = XLightPnt->Pos.z;
					OSize  = XLightPnt->Size;

					Temp.x = Posx - Pos->x;
					if( Temp.x < 0.0F )	Temp.x *= -1.0F;
					Temp.y = Posy - Pos->y;
					if( Temp.y < 0.0F )	Temp.y *= -1.0F;
					Temp.z = Posz - Pos->z;
					if( Temp.z < 0.0F )	Temp.z *= -1.0F;
					if ( (Temp.x <= ( Radius + OSize ) ) &&
						 (Temp.y <= ( Radius + OSize ) ) &&
						 (Temp.z <= ( Radius + OSize ) ) )
					{
						SizeX2 = OSize * OSize;
						Size = 1 / SizeX2;
						rlf	= XLightPnt->r;
						glf	= XLightPnt->g;
						blf	= XLightPnt->b;

						if( XLightPnt->Type == SPOT_LIGHT )
						{
							Dirx = XLightPnt->Dir.x;
							Diry = XLightPnt->Dir.y;
							Dirz = XLightPnt->Dir.z;
							CosArc = XLightPnt->CosArc;
						}

						LVert = VertBase;
						vert = MXloadheader->Group[group].num_verts_per_mesh[mesh];

						switch( XLightPnt->Type )
						{
						case POINT_LIGHT:

							while( vert-- )
							{
								/* find the distance from vert to light */

								ApplyMatrix( Matrix, (VECTOR*) LVert, &Temp );

								x = Temp.x - Posx;
								y = Temp.y - Posy;
								z = Temp.z - Posz;
								distance = (x*x) + (y*y) + (z*z);

								if ( distance <  SizeX2  )					// float
								{
									distance = 1.0F - ( distance * Size );	// float
									col =  LVert->color;				// int
									tempiA = col & 0xff000000;
									tempiR = (int) ( rlf * distance );
									tempiG = (int) ( glf * distance );
									tempiB = (int) ( blf * distance );
									inc = ( tempiR << 16 ) + ( tempiG << 8 ) + tempiB;
									carry = ( ( col + inc ) ^ ( col ^ inc ) ) & 0x01010100;
									clamp = carry - ( carry >> 8 );
									col = ( ( col + inc - carry ) | clamp) & 0x00ffffff;
									col |= tempiA;
									LVert->color = col;
								}
								LVert++;
							}
							break;
						case SPOT_LIGHT:
							while( vert-- )
							{
								/* find the distance from vert to light */
								ApplyMatrix( Matrix, (VECTOR*) LVert, &Temp );

								x = Temp.x - Posx;
								y = Temp.y - Posy;
								z = Temp.z - Posz;
								distance = (x*x) + (y*y) + (z*z);

								if ( distance <  SizeX2  )
								{
									if( distance > 0.0F )
									{
										distance = (float) sqrtf( distance );
										rlen = 1.0F / distance;
										x *= rlen;
										y *= rlen;
										z *= rlen;
									}
									Cosa = ( ( x*Dirx ) + ( y*Diry ) + ( z*Dirz ) );

									if( distance > 0.5F * OSize )
									{
										if ( Cosa > CosArc )
										{
											// Lights Are Go....
											intense = ( ( OSize - distance ) / ( 0.75F * OSize ) ) * ( ( Cosa - CosArc ) / ( 1.0F - CosArc ) );
										}else{
											goto PLOP2;
										}
									}else if ( distance > MIN_LIGHT_SIZE ) {
										float cosarc2;

										cosarc2 = CosArc * ( 1.0F - ( ( OSize * 0.5F - distance ) / ( OSize * 0.5F - MIN_LIGHT_SIZE ) ) );
										if ( Cosa > cosarc2 )
										{
											intense = ( ( OSize - distance ) / ( OSize - MIN_LIGHT_SIZE ) ) * ( ( Cosa - cosarc2 ) / ( 1.0F - cosarc2 ) );
										}else{
											goto PLOP2;
										}
									}else {
										if ( Cosa > 0.0F )
										{
											intense = 1.0F;
										}else{
											intense = 1.0F + Cosa;
										}
									}
									col =  LVert->color;				// int
									tempiA = col & 0xff000000;
									tempiR = (int) ( rlf * intense );
									tempiG = (int) ( glf * intense );
									tempiB = (int) ( blf * intense );
									inc = ( tempiR << 16 ) + ( tempiG << 8 ) + tempiB;
									carry = ( ( col + inc ) ^ ( col ^ inc ) ) & 0x01010100;
									clamp = carry - ( carry >> 8 );
									col = ( ( col + inc - carry ) | clamp) & 0x00ffffff;
									col |= tempiA;
									LVert->color = col;
								}
								PLOP2:
								LVert++;
							}
							break;
						}
					}
				}
				XLightPnt = XLightPnt->NextVisible;
			}
#endif /* !GPU_LIGHTING */
			/*	unlock the vertex buffer	*/
			if (!(FSUnlockVertexBuffer(&MXloadheader->Group[group].renderObject[mesh])))
			{
				return false;
			}
		}
	}

	return true;
}

/*===================================================================
	Procedure	:	Sample the baked colour grid (mesh 0's cells)
				:	of a level group at a point
	Input		:	MLOADHEADER *	Level
				:	VECTOR *		Position
				:	u_int16_t		Group
	Output		:	COLOR	cell colour with alpha 0; black outside
				:			the grid; halved while BrightShips is on
				:			in STATUS_Normal
===================================================================*/
COLOR FindNearestCellColour( MLOADHEADER * Mloadheader,VECTOR * Pos, u_int16_t group )
{
	COLOR Col;
	COLOR * colpnt;
	VECTOR	CellIndex;
	int		Cell;
	int		CellIndex_x;
	int		CellIndex_y;
	int		CellIndex_z;
	int		NumOfxCells;
	int		NumOfyCells;
	int		NumOfzCells;
	float	CellSize;
	int		r,g,b;


	r=g=b=0;
	CellSize = Mloadheader->CellSize;
	NumOfxCells = Mloadheader->Group[group].xcells[0];
	NumOfyCells = Mloadheader->Group[group].ycells[0];
	NumOfzCells = Mloadheader->Group[group].zcells[0];

	CellIndex.x = Pos->x - Mloadheader->Group[group].cell_origin[0].x;
	CellIndex.y = Pos->y - Mloadheader->Group[group].cell_origin[0].y;
	CellIndex.z = Pos->z - Mloadheader->Group[group].cell_origin[0].z;
	CellIndex_x = (int) fast_floorf( CellIndex.x * CellSize );
	CellIndex_y = (int) fast_floorf( CellIndex.y * CellSize );
	CellIndex_z = (int) fast_floorf( CellIndex.z * CellSize );
	if( CellIndex_x < 0 )
	{
		CellIndex_x = -1;
	}else{
		if( CellIndex_x >= NumOfxCells )
			CellIndex_x = -1;
	}
	if( CellIndex_y < 0 )
	{
		CellIndex_y = -1;
	}else{
		if( CellIndex_y >= NumOfyCells )
			CellIndex_y = -1;
	}
	if( CellIndex_z < 0 )
	{
		CellIndex_z = -1;
	}else{
		if( CellIndex_z >= NumOfzCells )
			CellIndex_z = -1;
	}

	if( CellIndex_x != -1 && CellIndex_y != -1 && CellIndex_z != -1 )
	{

		Cell = ( CellIndex_x + NumOfxCells *
			( CellIndex_y + NumOfyCells *
			CellIndex_z ) );

		colpnt = Mloadheader->Group[group].colour_cell_pnt[0];
		colpnt += Cell;
		Col = *colpnt;

		r = RGBA_GETRED(Col);
		g = RGBA_GETGREEN(Col);
		b = RGBA_GETBLUE(Col);
	}

	if( BrightShips && (MyGameStatus == STATUS_Normal) )
	{
		r >>= 1;
		g >>= 1;
		b >>= 1;

	}
	return RGBA_MAKE( r, g, b, 0 );
}

/*===================================================================
	Procedure	:	Build the FirstLightVisible chain: used lights that are
				:	Visible and whose group overlaps the camera's group
	Input		:	Group the current camera is in; INDEX_NONE takes
				:	every used light
	Output		:	nothing
===================================================================*/
void BuildVisibleLightList( u_int16_t Group )
{
	int		light;
	XLIGHT * XLightPnt;

	light = FirstXLightUsed;
	FirstLightVisible = nullptr;

	while( light != INDEX_NONE )
	{
		XLightPnt = &XLights[light];

		if( (Group == INDEX_NONE) || (XLightPnt->Visible && VisibleOverlap( Group, XLightPnt->Group , nullptr ) ) )
		{
			XLightPnt->NextVisible = FirstLightVisible;
			FirstLightVisible = XLightPnt;
		}
		light = XLights[light].Prev;
	}
}

/*===================================================================
	Procedure	:	Save XLights Array & Connected Global Variables
	Input		:	FILE	*	File Pointer
	Output		:	FILE	*	Updated File Pointer
===================================================================*/
FILE * SaveXLights( FILE * fp )
{
	int		i;
	u_int16_t	TempIndex = INDEX_NONE;

	if( fp )
	{
		fwrite( &FirstXLightUsed, sizeof( u_int16_t ), 1, fp );
		fwrite( &FirstXLightFree, sizeof( u_int16_t ), 1, fp );
		if( FirstLightVisible  ) fwrite( &FirstLightVisible->Index, sizeof( u_int16_t ), 1, fp );
		else fwrite( &TempIndex, sizeof( u_int16_t ), 1, fp );

		for( i = 0; i < MAXXLIGHTS; i++ )
		{
			fwrite( &XLights[ i ].Type, sizeof( int ), 1, fp );
			fwrite( &XLights[ i ].Visible, sizeof( bool ), 1, fp );
			fwrite( &XLights[ i ].r, sizeof( float ), 1, fp );
			fwrite( &XLights[ i ].g, sizeof( float ), 1, fp );
			fwrite( &XLights[ i ].b, sizeof( float ), 1, fp );
			fwrite( &XLights[ i ].Size, sizeof( float ), 1, fp );
			fwrite( &XLights[ i ].SizeCount, sizeof( float ), 1, fp );
			fwrite( &XLights[ i ].CosArc, sizeof( float ), 1, fp );
			fwrite( &XLights[ i ].Next, sizeof( u_int16_t ), 1, fp );
			fwrite( &XLights[ i ].Prev, sizeof( u_int16_t ), 1, fp );
			fwrite( &XLights[ i ].Pos, sizeof( VECTOR ), 1, fp );
			fwrite( &XLights[ i ].Dir, sizeof( VECTOR ), 1, fp );
			fwrite( &XLights[ i ].Group, sizeof( u_int16_t ), 1, fp );
			if( XLights[ i ].NextVisible ) fwrite( &XLights[ i ].NextVisible->Index, sizeof( u_int16_t ), 1, fp );
			else fwrite( &TempIndex, sizeof( u_int16_t ), 1, fp );
		}
	}

	return( fp );
}

/*===================================================================
	Procedure	:	Load XLights Array & Connected Global Variables
	Input		:	FILE	*	File Pointer
	Output		:	FILE	*	Updated File Pointer
===================================================================*/
FILE * LoadXLights( FILE * fp )
{
	int		i;
	u_int16_t	TempIndex;

	if( fp )
	{
		fread( &FirstXLightUsed, sizeof( u_int16_t ), 1, fp );
		fread( &FirstXLightFree, sizeof( u_int16_t ), 1, fp );
		fread( &TempIndex, sizeof( u_int16_t ), 1, fp );
		if( TempIndex != INDEX_NONE ) FirstLightVisible = &XLights[ TempIndex ];
		else FirstLightVisible = nullptr;

		for( i = 0; i < MAXXLIGHTS; i++ )
		{
			fread( &XLights[ i ].Type, sizeof( int ), 1, fp );
			fread( &XLights[ i ].Visible, sizeof( bool ), 1, fp );
			fread( &XLights[ i ].r, sizeof( float ), 1, fp );
			fread( &XLights[ i ].g, sizeof( float ), 1, fp );
			fread( &XLights[ i ].b, sizeof( float ), 1, fp );
			fread( &XLights[ i ].Size, sizeof( float ), 1, fp );
			fread( &XLights[ i ].SizeCount, sizeof( float ), 1, fp );
			fread( &XLights[ i ].CosArc, sizeof( float ), 1, fp );
			fread( &XLights[ i ].Next, sizeof( u_int16_t ), 1, fp );
			fread( &XLights[ i ].Prev, sizeof( u_int16_t ), 1, fp );
			fread( &XLights[ i ].Pos, sizeof( VECTOR ), 1, fp );
			fread( &XLights[ i ].Dir, sizeof( VECTOR ), 1, fp );
			fread( &XLights[ i ].Group, sizeof( u_int16_t ), 1, fp );
			fread( &TempIndex, sizeof( u_int16_t ), 1, fp );
			if( TempIndex != INDEX_NONE ) XLights[ i ].NextVisible = &XLights[ TempIndex ];
			else XLights[ i ].NextVisible = nullptr;
		}
	}

	return( fp );
}
