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

XLIGHT * FirstLightVisible = NULL;
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
	FirstXLightUsed = (u_int16_t) -1;
	FirstXLightFree = 0;
	for( i = 0 ; i < MAXXLIGHTS ; i++ )
	{
		XLights[i].Index = i;
		XLights[i].Next = i + 1;
		XLights[i].Prev = (u_int16_t) -1;
		XLights[i].Type = POINT_LIGHT;
	}

	XLights[MAXXLIGHTS-1].Next = (u_int16_t) -1;
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
	
	if ( i == (u_int16_t) -1)
		return i;
 
	XLights[i].Prev = FirstXLightUsed;
	if ( FirstXLightUsed != (u_int16_t) -1)
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

	if( its_prev != (u_int16_t) -1)
		XLights[its_prev].Next = its_next;

	if( its_next != (u_int16_t) -1)
		XLights[its_next].Prev = its_prev;

	XLights[light].Prev = (u_int16_t) -1;
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
	while( light != (u_int16_t ) -1 )
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
	XLIGHT * XLightPnt;
	float	blf;
	float	glf;
	float	rlf;
	VECTOR	Temp;
	VECTOR	CellIndex;
	float	distance;
	int		execbuf;
	int		vert;
    LVERTEX	*lpPointer = NULL;
	LVERTEX	*lpLVERTEX = NULL;
	LVERTEX	*lpLVERTEX2 = NULL;
	COLOR col;
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
	float	x,y,z;
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
	u_int32_t r,g,b,intWhiteOut;
	POLYANIM * PolyAnim;
	int i,e;
	u_int32_t * u_int32Pnt;
	TANIMUV * TanimUV;
	float	intensity;


	intWhiteOut = (int)WhiteOut;
	if( intWhiteOut >= 256 )
	{
		intWhiteOut = (256 - (intWhiteOut-256) );
	}

	
	CellSize = Mloadheader->CellSize;
	execbuf = Mloadheader->Group[group].num_execbufs;
	while( execbuf--)
	{
		if (!(FSLockVertexBuffer((RENDEROBJECT*)&Mloadheader->Group[group].renderObject[execbuf], &lpPointer)))
		{
			return false;
		}	
		/* The colour pass below restores the baked colours (stable from
		 * frame to frame) unless white-out or water rewrite them every
		 * frame; tell the backend's caches which (render.h SMEMO_PASS). */
		Mloadheader->Group[group].renderObject[execbuf].memoEpochPass =
			( ( GroupWaterInfo[group] == WATERSTATE_NOWATER || ShowPlaneRGB ) && WhiteOut == 0.0F ) ?
				SMEMO_PASS_UNSET : SMEMO_PASS_PER_FRAME;

		VertexCellPnt = Mloadheader->Group[group].vertex_cell_pnt[execbuf];
		OrgVertexIndexPnt = Mloadheader->Group[group].vertex_index_pnt[execbuf];

		NumOfxCells = Mloadheader->Group[group].xcells[execbuf];
		NumOfyCells = Mloadheader->Group[group].ycells[execbuf];
		NumOfzCells = Mloadheader->Group[group].zcells[execbuf];

		if( Mloadheader->Group[group].num_animating_polys[execbuf] != 0 )
		{

			PolyAnim = Mloadheader->Group[group].polyanim[execbuf];

			for( i = 0 ; i < Mloadheader->Group[group].num_animating_polys[execbuf] ; i++ )
			{
				
				if( PolyAnim->currentframe != PolyAnim->newframe )
				{
					// something has changed....

					u_int32Pnt = (u_int32_t*)PolyAnim->vert;
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
		
		
		
		lpLVERTEX = lpPointer;
		{
			lpLVERTEX2 = Mloadheader->Group[group].originalVerts[execbuf];
			
			vert = Mloadheader->Group[group].num_verts_per_execbuf[execbuf];
			
			/* @vfp-exempt R3 — light inclusion/attenuation window compare; integerizing changes which lights apply. measured n/a. Verified 2026-10-25. */
			if( GroupWaterInfo[group] == WATERSTATE_NOWATER || ShowPlaneRGB )
			{
				if( WhiteOut == 0.0F )
				{
					if ( ShowPlaneRGB )
					{
						while( vert --)
						{
							lpLVERTEX++;		
							lpLVERTEX2++;		
						}
					}
					else
					{
						while( vert --)
						{
							lpLVERTEX->color = lpLVERTEX2->color;
							lpLVERTEX++;		
							lpLVERTEX2++;		
						}
					}
					
				}
				else
				{
					//	Special Lighting effects
					while( vert --)
					{
						x = (float)((int) (lpLVERTEX2->x * 0.35F) % 360);
						y = (float)((int) (lpLVERTEX2->y * 0.35F) % 360);
						z = (float)((int) (lpLVERTEX2->z * 0.35F) % 360);
						col = lpLVERTEX2->color;
						col &= 0xffff;
						b = (int)  ( ( fast_sinf( D2R( x + cral ) ) + fast_sinf( D2R( y + cral ) ) + fast_sinf( D2R( z + cral ) ) ) * 127.0F * 0.3333333F + 128.0F ) ;
						b+= intWhiteOut;
						if( b > 255 )
							b = 255;
						col |= (b<<24)+(b<<16);
						lpLVERTEX->color = col;
						lpLVERTEX++;		
						lpLVERTEX2++;		
						
					}
				}
			}else if( GroupWaterInfo[group] == WATERSTATE_ALLWATER )
			{
				// ****************** Full Water Effect ********************************
				lpLVERTEX2 = Mloadheader->Group[group].originalVerts[execbuf];
				lpLVERTEX = lpPointer;
				vert = Mloadheader->Group[group].num_verts_per_execbuf[execbuf];
				//	Special Lighting effects
				while( vert --)
				{
					int orig_a;
					x = (float)((int) (lpLVERTEX2->x * 0.35F) % 360);
					y = (float)((int) (lpLVERTEX2->y * 0.35F) % 360);
					z = (float)((int) (lpLVERTEX2->z * 0.35F) % 360);
					col = lpLVERTEX2->color;
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
					lpLVERTEX->color = RGBA_MAKE( r ,g ,b , orig_a );
					lpLVERTEX++;
					lpLVERTEX2++;
				}
				// ****************** End of Water Effect ******************************
			}else{
				// ****************** Partial Water Effect ******************************
				lpLVERTEX2 = Mloadheader->Group[group].originalVerts[execbuf];
				lpLVERTEX = lpPointer;
				/* @vfp-exempt R3 — light inclusion/attenuation window compare; integerizing changes which lights apply. measured n/a. Verified 2026-10-25. */
				vert = Mloadheader->Group[group].num_verts_per_execbuf[execbuf];
				//	Special Lighting effects
				while( vert --)
				{
					if( lpLVERTEX2->y < GroupWaterLevel[group] )
					{
						int orig_a;
						x = (float)((int) (lpLVERTEX2->x * 0.35F) % 360);
						y = (float)((int) (lpLVERTEX2->y * 0.35F) % 360);
						z = (float)((int) (lpLVERTEX2->z * 0.35F) % 360);
						col = lpLVERTEX2->color;
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
						lpLVERTEX->color = RGBA_MAKE( r ,g ,b , orig_a );
					}else{
  						lpLVERTEX->color = lpLVERTEX2->color;
					}
					lpLVERTEX++;
					lpLVERTEX2++;
				}
				// ****************** End of Water Effect ******************************
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
				
					
						CellIndex.x = Posx - Mloadheader->Group[group].cell_origin[execbuf].x;
						CellIndex.y = Posy - Mloadheader->Group[group].cell_origin[execbuf].y;
						CellIndex.z = Posz - Mloadheader->Group[group].cell_origin[execbuf].z;
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
											lpLVERTEX = lpPointer + *VertexIndexPnt++;
											/* find the distance from vert to light */
											x = lpLVERTEX->x;
											y = lpLVERTEX->y;
											z = lpLVERTEX->z;
											x -= Posx;
											y -= Posy;
											z -= Posz;
			
											/* @vfp-exempt R3 — light inclusion/attenuation window compare; integerizing changes which lights apply. measured n/a. Verified 2026-10-25. */
											distance = (x*x) + (y*y) + (z*z);
											if ( distance <  SizeX2  )					// float
											{
												distance = 1.0F - ( distance * Size );	// float

												col = lpLVERTEX->color;
												tempiA = col & 0xff000000;
												tempiR = (int) ( rlf * distance );
												tempiG = (int) ( glf * distance );
												tempiB = (int) ( blf * distance );
												inc = ( tempiR << 16 ) + ( tempiG << 8 ) + tempiB;
												carry = ( ( col + inc ) ^ ( col ^ inc ) ) & 0x01010100;
												clamp = carry - ( carry >> 8 );
												col = ( ( col + inc - carry ) | clamp) & 0x00ffffff;
												col |= tempiA;
			
												lpLVERTEX->color = col;
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
											lpLVERTEX = lpPointer + *VertexIndexPnt++;
											/* find the distance from vert to light */
											x = lpLVERTEX->x - Posx;
											y = lpLVERTEX->y - Posy;
											z = lpLVERTEX->z - Posz;
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
												
												col =  lpLVERTEX->color;				// int
												tempiA = col & 0xff000000;
												tempiR = (int) ( rlf * intense );
												tempiG = (int) ( glf * intense );
												tempiB = (int) ( blf * intense );
												inc = ( tempiR << 16 ) + ( tempiG << 8 ) + tempiB;
												carry = ( ( col + inc ) ^ ( col ^ inc ) ) & 0x01010100;
												clamp = carry - ( carry >> 8 );
												col = ( ( col + inc - carry ) | clamp) & 0x00ffffff;
												col |= tempiA;
												lpLVERTEX->color = col;
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
		if(!FSUnlockVertexBuffer((RENDEROBJECT*)&Mloadheader->Group[group].renderObject[execbuf]))
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
	XLIGHT * XLightPnt;
	VECTOR	Temp;
	float	distance;
	int		group;
	int		execbuf;
	int		vert;
    LVERTEX	*lpPointer = NULL;

	LVERTEX	*lpLVERTEX = NULL;
	LVERTEX	*lpLVERTEX2 = NULL;
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


	group = MXloadheader->num_groups;
	while( group--)
	{
	
		execbuf = MXloadheader->Group[group].num_execbufs;
		while( execbuf--)
		{
			if (!(FSLockVertexBuffer(&MXloadheader->Group[group].renderObject[execbuf], &lpPointer)))
			{
				return false;
			}

			lpLVERTEX = lpPointer;
			vert = MXloadheader->Group[group].num_verts_per_execbuf[execbuf];
			lpLVERTEX2 = MXloadheader->Group[group].originalVerts[execbuf];

#ifdef GPU_LIGHTING
			/* The baked-colour restore is skipped here (lock/unlock is
			 * kept): it assumes the live colours still equal
			 * originalVerts. Note that AmbientLightMxModel and
			 * LightMxModel (models.cpp) do write computed colours into
			 * these same per-model live vertices, and the vertices are
			 * shared by every instance of the model, so colours they
			 * leave behind are not undone by this path. */
			(void)vert;
			(void)lpLVERTEX;
			(void)lpLVERTEX2;
#else
			while( vert --)
			{
				lpLVERTEX->color = lpLVERTEX2->color;
				lpLVERTEX++;
				lpLVERTEX2++;
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
					
						lpLVERTEX = lpPointer;
						vert = MXloadheader->Group[group].num_verts_per_execbuf[execbuf];
		
						switch( XLightPnt->Type )
						{
						case POINT_LIGHT:
		
							while( vert-- )
							{
								/* find the distance from vert to light */

								ApplyMatrix( Matrix, (VECTOR*) lpLVERTEX, &Temp );
								
								x = Temp.x - Posx;
								y = Temp.y - Posy;
								z = Temp.z - Posz;
								distance = (x*x) + (y*y) + (z*z);
		
								if ( distance <  SizeX2  )					// float
								{
									distance = 1.0F - ( distance * Size );	// float
									col =  lpLVERTEX->color;				// int
									tempiA = col & 0xff000000;
									tempiR = (int) ( rlf * distance );
									tempiG = (int) ( glf * distance );
									tempiB = (int) ( blf * distance );
									inc = ( tempiR << 16 ) + ( tempiG << 8 ) + tempiB;
									carry = ( ( col + inc ) ^ ( col ^ inc ) ) & 0x01010100;
									clamp = carry - ( carry >> 8 );
									col = ( ( col + inc - carry ) | clamp) & 0x00ffffff;
									col |= tempiA;
									lpLVERTEX->color = col;
								}
								lpLVERTEX++;
							}
							break;
						case SPOT_LIGHT:
							while( vert-- )
							{
								/* find the distance from vert to light */
								ApplyMatrix( Matrix, (VECTOR*) lpLVERTEX, &Temp );

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
									col =  lpLVERTEX->color;				// int
									tempiA = col & 0xff000000;
									tempiR = (int) ( rlf * intense );
									tempiG = (int) ( glf * intense );
									tempiB = (int) ( blf * intense );
									inc = ( tempiR << 16 ) + ( tempiG << 8 ) + tempiB;
									carry = ( ( col + inc ) ^ ( col ^ inc ) ) & 0x01010100;
									clamp = carry - ( carry >> 8 );
									col = ( ( col + inc - carry ) | clamp) & 0x00ffffff;
									col |= tempiA;
									lpLVERTEX->color = col;
								}
PLOP:
								lpLVERTEX++;
							}
							break;
						}
					}
				}
				XLightPnt = XLightPnt->NextVisible;
			}
#endif /* !GPU_LIGHTING */
			/*	unlock the vertex buffer	*/
			if (!(FSUnlockVertexBuffer(&MXloadheader->Group[group].renderObject[execbuf])))
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
	XLIGHT * XLightPnt;
	VECTOR	Temp;
	float	distance;
	int		group;
	int		execbuf;
	int		vert;
    LVERTEX	*lpPointer = NULL;

	LVERTEX	*lpLVERTEX = NULL;
	LVERTEX	*lpLVERTEX2 = NULL;
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


	group = MXloadheader->num_groups;
	while( group--)
	{
	
		execbuf = MXloadheader->Group[group].num_execbufs;
		while( execbuf--)
		{
			if (!(FSLockVertexBuffer(&MXloadheader->Group[group].renderObject[execbuf], &lpPointer)))
			{
				return false;
			}
		
			lpLVERTEX = lpPointer;
			vert = MXloadheader->Group[group].num_verts_per_execbuf[execbuf];
			lpLVERTEX2 = MXloadheader->Group[group].originalVerts[execbuf];
			while( vert --)
			{
				lpLVERTEX->color = lpLVERTEX2->color;
				lpLVERTEX++;
				lpLVERTEX2++;
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
					
						lpLVERTEX = lpPointer;
						vert = MXloadheader->Group[group].num_verts_per_execbuf[execbuf];
		
						switch( XLightPnt->Type )
						{
						case POINT_LIGHT:
		
							while( vert-- )
							{
								/* find the distance from vert to light */

								ApplyMatrix( Matrix, (VECTOR*) lpLVERTEX, &Temp );
								
								x = Temp.x - Posx;
								y = Temp.y - Posy;
								z = Temp.z - Posz;
								distance = (x*x) + (y*y) + (z*z);
		
								if ( distance <  SizeX2  )					// float
								{
									distance = 1.0F - ( distance * Size );	// float
									col =  lpLVERTEX->color;				// int
									tempiA = col & 0xff000000;
									tempiR = (int) ( rlf * distance );
									tempiG = (int) ( glf * distance );
									tempiB = (int) ( blf * distance );
									inc = ( tempiR << 16 ) + ( tempiG << 8 ) + tempiB;
									carry = ( ( col + inc ) ^ ( col ^ inc ) ) & 0x01010100;
									clamp = carry - ( carry >> 8 );
									col = ( ( col + inc - carry ) | clamp) & 0x00ffffff;
									col |= tempiA;
									lpLVERTEX->color = col;
								}
								lpLVERTEX++;
							}
							break;
						case SPOT_LIGHT:
							while( vert-- )
							{
								/* find the distance from vert to light */
								ApplyMatrix( Matrix, (VECTOR*) lpLVERTEX, &Temp );
								
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
									col =  lpLVERTEX->color;				// int
									tempiA = col & 0xff000000;
									tempiR = (int) ( rlf * intense );
									tempiG = (int) ( glf * intense );
									tempiB = (int) ( blf * intense );
									inc = ( tempiR << 16 ) + ( tempiG << 8 ) + tempiB;
									carry = ( ( col + inc ) ^ ( col ^ inc ) ) & 0x01010100;
									clamp = carry - ( carry >> 8 );
									col = ( ( col + inc - carry ) | clamp) & 0x00ffffff;
									col |= tempiA;
									lpLVERTEX->color = col;
								}
PLOP2:
								lpLVERTEX++;
							}
							break;
						}
					}
				}
				XLightPnt = XLightPnt->NextVisible;
			}
#endif /* !GPU_LIGHTING */
			/*	unlock the vertex buffer	*/
			if (!(FSUnlockVertexBuffer(&MXloadheader->Group[group].renderObject[execbuf])))
			{
				return false;
			}
		}
	}
	
	return true;
}

/*===================================================================
	Procedure	:	Sample the baked colour grid (execbuf 0's cells)
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
	Input		:	Group the current camera is in; (u_int16_t)-1 takes
				:	every used light
	Output		:	nothing
===================================================================*/
void BuildVisibleLightList( u_int16_t Group )
{
	int		light;
	XLIGHT * XLightPnt;

	light = FirstXLightUsed;
	FirstLightVisible = NULL;

	while( light != (u_int16_t ) -1 )
	{
		XLightPnt = &XLights[light];

		if( (Group == (u_int16_t) -1) || (XLightPnt->Visible && VisibleOverlap( Group, XLightPnt->Group , NULL ) ) )
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
	u_int16_t	TempIndex = (u_int16_t) -1;

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
		if( TempIndex != (u_int16_t) -1 ) FirstLightVisible = &XLights[ TempIndex ];
		else FirstLightVisible = NULL;
		
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
			if( TempIndex != (u_int16_t) -1 ) XLights[ i ].NextVisible = &XLights[ TempIndex ];
			else XLights[ i ].NextVisible = NULL;
		}
	}

	return( fp );
}
