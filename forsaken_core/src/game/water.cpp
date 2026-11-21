
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
#include "game/triggers.h"
#include "data/file.h"
#include "rendering/render.h"
#include "math/new3d.h"
#include "data/tload.h"
#include "data/mload.h"
#include "rendering/transexe.h"
#include "game/water.h"
#include "util/util.h"
#include "game/oct2.h"
#include "game/ships.h"
#include "util/hunk_alloc.h"
#include "util/file_reader.h"

/*===================================================================
		Externs...
===================================================================*/

/*===================================================================
		Defines
===================================================================*/
constexpr int WAT_VERSION_NUMBER = 1;

/*===================================================================
		Globals...
===================================================================*/

void DisplayWaterObject(WATEROBJECT * WO);

VECTOR WaterSurfaces::WaterNormal = { 0.0F, 1.0F, 0.0F };
float	WaterSurfaces::Fc[MAXMESHX*MAXMESHY] = {};
u_int16_t	WaterSurfaces::GroupWaterInfo[MAXGROUPS] = {};
float	WaterSurfaces::GroupWaterLevel[MAXGROUPS] = {};
float	WaterSurfaces::GroupWaterIntensity_Red[MAXGROUPS] = {};
float	WaterSurfaces::GroupWaterIntensity_Green[MAXGROUPS] = {};
float	WaterSurfaces::GroupWaterIntensity_Blue[MAXGROUPS] = {};

/*===================================================================
	Procedure	:	Pre Water Load..
	Input		:	char * filename....
	Output		:	bool
===================================================================*/
bool WaterSurfaces::pre_load( char * Filename )
{
	long			File_Size;
	long			Read_Size;
	char		*	Buffer;
	char	*	FileNamePnt;
	char buf[256];
	int i;
	u_int32_t	MagicNumber;
	u_int32_t	VersionNumber;
	int8_t		TempFilename[ 256 ];

	for( i = 0 ; i < MAXGROUPS ; i++ )
	{
		GroupDamageInfo[i] = 0.0F;
		GroupWaterInfo[i] = WATERSTATE_NOWATER;
		WaterObjectLink[i] = nullptr;
	}

	NumOfWaterObjects = 0;
	FirstWaterObject = nullptr;
	WaterBuffer = nullptr;

	File_Size = Get_File_Size( Filename );

	if( !File_Size )
	{
		// dont need water...
		return true;
	}
	Buffer = (char*)malloc( File_Size );
	OrgWaterBuffer = Buffer;

	if( !Buffer )
	{
		Msg( "Pre Water Load : Unable to allocate file buffer\n", Filename );
		return( false );
	}
	Read_Size = Read_File( Filename, Buffer, File_Size );
	if( Read_Size != File_Size )
	{
		Msg( "Pre Water Load Error reading %s\n", Filename );
		return( false );
	}

	FileReader in( Buffer );
	MagicNumber = in.u32();
	VersionNumber = in.u32();

	if( ( MagicNumber != MAGIC_NUMBER ) || ( VersionNumber != WAT_VERSION_NUMBER  ) )
	{
		Msg( "PreWaterLoad() Incompatible water( .WAT ) file %s", Filename );
		return( false );
	}

	NumOfWaterObjects = in.u16();
	Buffer = (char *) in.pos();
	// get the Tpage sorted out...
	FileNamePnt = (char *) &buf[0];
	while ( ( *FileNamePnt++ = *Buffer++ ) != 0 );

	GetLevelTexturePath( (char*)&TempFilename[ 0 ], sizeof( TempFilename ), &buf[0], &ShortLevelNames[ LevelNum ][ 0 ] );

	DebugPrintf("loading water texture: %s\n",TempFilename);

	WaterTPage = AddTexture( &Tloadheader , (char*)&TempFilename[ 0 ], true , true, 0, 0 );		// dont colourkey
	WaterBuffer = Buffer;
	return true;
}

/*===================================================================
	Procedure	:	Water Load..
	Input		:	char * filename....
	Output		:	bool
===================================================================*/
bool WaterSurfaces::load()
{
	char		*	Buffer;
	char		*	OrgBuffer;
	int			i;
	WATEROBJECT	* WO;
	u_int16_t NumOfGroups = 0;
	COLOR col;

	FirstWaterObject = nullptr;
	Buffer = WaterBuffer;
	OrgBuffer = OrgWaterBuffer;

	if( !Buffer )
		return true;

	FileReader in( Buffer );
	NumOfGroups = in.u16();

	for( i = 0 ; i < NumOfGroups ; i++ )
	{
		GroupWaterInfo[i] = in.u16();
		col = in.u32();

		GroupWaterIntensity_Red[i] = RGBA_GETRED(col) * ONEOVER255;
		GroupWaterIntensity_Green[i] = RGBA_GETGREEN(col) * ONEOVER255;
		GroupWaterIntensity_Blue[i] = RGBA_GETBLUE(col) * ONEOVER255;
	}


	FirstWaterObject = (WATEROBJECT*) malloc( NumOfWaterObjects * sizeof(WATEROBJECT) );
	if( !FirstWaterObject )
	{
		Msg( "Water Load : Unable to allocate buffer" );
		return( false );
	}

	/* Bug fix: sizeof(FirstWaterObject) is sizeof(pointer) = 4 bytes,
	 * not sizeof(WATEROBJECT). The original line zeroed only ~N×4 bytes
	 * of the N×sizeof(WATEROBJECT) buffer, leaving each element's
	 * embedded RENDEROBJECT (normalBuffer / indexBuffer /
	 * numTextureGroups / textureGroups[]) full of malloc residue.
	 * FSReleaseRenderObject then linearFree'd those garbage pointers
	 * and iterated `for (i=0; i<numTextureGroups; i++)` with a garbage
	 * count, hanging WaterRelease somewhere around the 20th level
	 * transition (when the heap had accumulated enough residue for the
	 * fields to read non-zero). */
	memset(FirstWaterObject, 0, NumOfWaterObjects * sizeof(WATEROBJECT));

	WO = FirstWaterObject;
	WO->Group = 0;
	WO->AnimOncePerFrame = 0;
	for( i = 0 ; i < NumOfWaterObjects ; i++ )
	{
		WO->Status = WaterStatus::Static;

		/* Each water object uses exactly one texture group at draw time
		 * (see line ~496 below). Post-Q3-refactor, RENDEROBJECT's
		 * textureGroups is a pointer rather than an inline array, so
		 * we allocate one slot per water object from the TAG_LEVEL
		 * hunk. Bulk-freed via Hunk_FreeAll(TAG_LEVEL) at level
		 * transition. */
		{
			WO->renderObject.textureGroups =
				(TEXTUREGROUP *) Hunk_Alloc(TAG_LEVEL, sizeof(TEXTUREGROUP));
			if ( !WO->renderObject.textureGroups )
			{
				Msg( "WaterLoad: hunk-alloc failed for WO[%d].textureGroups", i );
				goto load_fail;
			}
		}

		WO->Group = in.u16();
		WO->Pos = in.vec();
		WO->offset = 0.0F;
		GroupWaterLevel[WO->Group] = WO->Pos.y + VideoOpts.WATER_CELLSIZE;
		WO->XSize = in.f32();
		WO->YSize = in.f32();
		WO->uTL = in.f32();
		WO->vTL = in.f32();
		WO->uBR = in.f32();
		WO->vBR = in.f32();

		WO->uRange = WO->uBR - WO->uTL;
		WO->vRange = WO->vBR - WO->vTL;

		WO->XVerts = ((int) (WO->XSize / VideoOpts.WATER_CELLSIZE))+1;
		WO->YVerts = ((int) (WO->YSize / VideoOpts.WATER_CELLSIZE))+1;

		if ( WO->XVerts < 1 || WO->YVerts < 1 )
		{
			DebugPrintf("Not loading water, either xVerts or yVerts was less than 1\n");
			DebugPrintf("xVerts=%d, yVerts=%d, xSize=%d, ySize=%d, cellSize=%d\n",
				WO->XVerts,WO->YVerts,WO->XSize,WO->YSize,VideoOpts.WATER_CELLSIZE);
			goto load_fail;
		}

		if( ( WO->XVerts * WO->YVerts ) > MAXMESHX * MAXMESHY )
		{
			Msg( "Water Load : Water is to big\n" );
			goto load_fail;
		}
		if( !init_object( WO ) )
		{
			Msg( "Water Load : Unable to init water object\n" );
			goto load_fail;

		}
		add_link(WO);

		col = in.u32();
		WO->Red = RGBA_GETRED(col) * ONEOVER255;
		WO->Green = RGBA_GETGREEN(col) * ONEOVER255;
		WO->Blue = RGBA_GETBLUE(col) * ONEOVER255;

		WO->MaxLevel = in.f32();
		WO->MinLevel = in.f32();
		WO->FillRate = in.f32(); WO->FillRate/=60.0f;
		WO->DrainRate = in.f32(); WO->DrainRate/=60.0f;
		WO->Density = in.f32();
		WO->MaxWaveSize = in.f32();

		WO->Trigger_WhenFilled = in.u16();
		WO->Trigger_WhenDrained = in.u16();
		WO->Damage = (((float) in.i16()) / 60.0F);

		WO++;
	}
	free( OrgBuffer );
	return true;

	load_fail:
	/* Bail-out cleanup. Without this, FirstWaterObject is left malloc'd
	 * with partially populated WATEROBJECTs (Group, XVerts, YVerts set
	 * from the file but Verts/Vels/renderObject still NULL because
	 * InitWaterObject hasn't run). The caller in oct2.cpp ignores
	 * WaterLoad's return value, so the engine continues into gameplay
	 * with that half-loaded state. The first frame's
	 * GroupWaterProcessDisplay then matches WO->Group and calls
	 * UpdateWaterMesh, which dereferences NULL Verts/Vels via the
	 * x/y mesh-update loops — data abort.
	 *
	 * Tuben64 hits the size cap (37×29 = 1073 > MAXMESHX*MAXMESHY) and
	 * was the first level in the autotest sweep to expose this gap. */
	if (FirstWaterObject) {
		free(FirstWaterObject);
		FirstWaterObject = nullptr;
	}
	NumOfWaterObjects = 0;
	free(OrgBuffer);
	return false;
}

/*===================================================================
	Procedure	:	Water Release..
	Input		:	NOTHING
	Output		:	NOTHING
===================================================================*/
void WaterSurfaces::release()
{
	int			i;
	WATEROBJECT	* WO;

	WO = FirstWaterObject;
	if( !WO )
		return;
	for( i = 0 ; i < NumOfWaterObjects ; i++ )
	{
		free(WO->Verts);
		free(WO->Vels);

		FSReleaseRenderObject(&WO->renderObject);

		WO++;
	}

	if( FirstWaterObject )
	{
		free(FirstWaterObject);
		FirstWaterObject = nullptr;
	}
	NumOfWaterObjects = 0;
}
/*===================================================================
	Procedure	:		Add a Water to the group link list..
	Input		:		WATEROBJECT * WO
	Output		:		Nothing
===================================================================*/
void WaterSurfaces::add_link(WATEROBJECT * WO)
{
	if( !WaterObjectLink[WO->Group] )
	{
		WaterObjectLink[WO->Group] = WO;
		WO->GroupLink = nullptr;
	}else{
		WO->GroupLink = WaterObjectLink[WO->Group];
		WaterObjectLink[WO->Group] = WO;
	}
}

/*===================================================================
	Procedure	:		Init Everything to do with a Water Mesh...
	Input		:		Nothing
	Output		:		bool true/false
===================================================================*/
bool WaterSurfaces::init_object(WATEROBJECT * WO)
{
	int x,y;
	TRIANGLE	*FacePnt = nullptr;
	LVERTEX	*LVert = nullptr;
	WORD			*Indices = nullptr;
	int			i;
	int ntris = 0;
	int vertsCount = 0;

	WO->num_of_verts = WO->XVerts * WO->YVerts;
	WO->Verts = (float*) calloc(  WO->num_of_verts , sizeof(float) );
	WO->Vels = (float*) calloc( WO->num_of_verts , sizeof(float) );

	if (!FSCreateVertexBuffer(&WO->renderObject, WO->num_of_verts))
	{
		DebugPrintf("water FSCreateVertexBuffer failed\n");
		return false;
	}
	WO->renderObject.vertOwner = VERT_OWNER_WATER;

	// pre-count the number of tris so we know how big to make the index buffer
	// this loop structure was coppied from the way the triangles are assigned further bellow
	for( x = 0 ; x < WO->XVerts-1 ; x++ )
		for( y = 0 ; y < WO->YVerts-1 ; y++ )
			ntris += 2;

	if (!FSCreateIndexBuffer(&WO->renderObject, ntris*3 )) // 3 vertexes in a triangle
	{
		DebugPrintf("water FSCreateIndexBuffer failed\n");
		return false;
	}

	if (!FSLockVertexBuffer(&WO->renderObject, &LVert))
	{
		DebugPrintf("water FSLockVertexBuffer failed\n");
		return false;
	}

	if (!(FSLockIndexBuffer(&WO->renderObject, &Indices)))
	{
		DebugPrintf("water FSLockIndexBuffer failed\n");
		return false;
	}

	FacePnt =  (TRIANGLE *) Indices;

	for( x = 0 ; x < WO->XVerts ; x++ )
	{
		for( y = 0 ; y < WO->YVerts ; y++ )
		{
			LVert->x = -( ((WO->XVerts-1) * VideoOpts.WATER_CELLSIZE ) * 0.5F ) + (x*VideoOpts.WATER_CELLSIZE);
			LVert->z = -( ((WO->YVerts-1) * VideoOpts.WATER_CELLSIZE ) * 0.5F ) + (y*VideoOpts.WATER_CELLSIZE);
			LVert->y = 0.0F;
			LVert->tu = 0.0F;
			LVert->tv = 0.0F;
			LVert->color = RGBA_MAKE(128,128,128,128);

			LVert++;

			vertsCount++;
		}
	}

	/*	copy the triangles into the index buffer	*/

	for( x = 0 ; x < WO->XVerts-1 ; x++ )
	{
		for( y = 0 ; y < WO->YVerts-1 ; y++ )
		{
			i = x * WO->YVerts + y;
			FacePnt->v1 = i+WO->YVerts+1;
			FacePnt->v2 = i+WO->YVerts;
			FacePnt->v3 = i;
			FacePnt++;
			FacePnt->v1 = i+1;
			FacePnt->v2 = i+WO->YVerts+1;
			FacePnt->v3 = i;
			FacePnt++;
		}
	}

	if (!(FSUnlockVertexBuffer(&WO->renderObject)))
	{
		DebugPrintf( "water FSUnlockVertexBuffer failed\n");
		return false;
	}

	if (!(FSUnlockIndexBuffer(&WO->renderObject)))
	{
		DebugPrintf( "water FSUnlockIndexBuffer failed\n");
		return false ;
	}

	/*	set up the render object	*/
	WO->renderObject.numTextureGroups = 1;
	WO->renderObject.textureGroups[0].numTriangles = ntris;
	WO->renderObject.textureGroups[0].numVerts = WO->num_of_verts;
	WO->renderObject.textureGroups[0].startIndex = 0;
	WO->renderObject.textureGroups[0].startVert = 0;
	WO->renderObject.textureGroups[0].texture = Tloadheader.TexHandle[WaterTPage];
	WO->renderObject.textureGroups[0].colourkey = Tloadheader.ColourKey[WaterTPage];

	return true;
}

/*===================================================================
	Procedure	:	Process Water Based on Group..
	Input		:	u_int16_t group
	Output		:	NOTHING
===================================================================*/
void WaterSurfaces::process_display( u_int16_t group )
{
	WATEROBJECT	* WO;
	u_int16_t i;
	float r, g, b;

	WO = FirstWaterObject;

	for( i = 0 ; i < NumOfWaterObjects ; i++ )
	{
		if( (WO->Group==group) && (WO->AnimOncePerFrame != AnimOncePerFrame) )
		{
			WO->AnimOncePerFrame = AnimOncePerFrame;

			// store orig water rgb
			r = WO->Red; g = WO->Green; b = WO->Blue;
			// apply fade value ( should be 0 to 1 )
			WO->Red *= WaterFade; WO->Green *= WaterFade; WO->Blue *= WaterFade;

			update_mesh( WO );

			// restore orig water rgb
			WO->Red = r; WO->Green = g; WO->Blue = b;
		}
		if( WO->Group==group )
		{
			// Display Water...
			DisplayWaterObject( WO );
		}
		WO++;
	}

}


/*===================================================================
	Procedure	:	Update a Water Mesh...
	Input		:	WATEROBJECT * WO
	Output		:	NOTHING
===================================================================*/
void WaterSurfaces::update_mesh( WATEROBJECT * WO )
{
	int x,y;
	LVERTEX	*LVert;
	int col;
	float dx, dy;
	float u, v;
	float * floatpnt;
	float	Tick,Damp;
	float temp;
	float temp2;
	float * VelsPnt;
	float * VertPnt;
	float	org;

	Damp = (float)powf( 1.0 - 0.01, framelag * framelag);
	Tick = 0.04F * framelag;


	memmove( &Fc[0] , WO->Verts , WO->num_of_verts * sizeof(float) );//memcpy

	VelsPnt = WO->Vels;
	VelsPnt += WO->YVerts + 1;
	VertPnt = WO->Verts;
	VertPnt += WO->YVerts + 1;

	for (x = 1; x< WO->XVerts-1; x++)
	{
		for (y=1; y<WO->YVerts-1; y++)
		{
			floatpnt = &Fc[0];
			floatpnt += ((x-1) * WO->YVerts ) + (y-1);
			temp2 = *floatpnt++;
			temp = *floatpnt++;
			temp2 += *floatpnt;
			floatpnt += WO->YVerts-2;

			temp += *floatpnt++;
			org = *floatpnt++;
			temp += *floatpnt;

			floatpnt += WO->YVerts-2;
			temp2 += *floatpnt++;
			temp += *floatpnt++;
			temp2 += *floatpnt;

			temp += temp2 * ROOTTWO;
			temp *= SPECIALROOTTWO;

			*VelsPnt += Tick * (temp - org);  // Accel
			*VelsPnt *= Damp;						// Damping
			*VertPnt++ += *VelsPnt++ * framelag;
		}
		VelsPnt += 2;
		VertPnt += 2;
	}

	/*	lock the vertex buffer	*/

	if (!(FSLockVertexBuffer(/*WO->vertexBuffer*/&WO->renderObject, &LVert)))
	{
		return;
	}

	VertPnt = WO->Verts;

	for( x = 0 ; x < WO->XVerts ; x++ )
	{
		for( y = 0 ; y < WO->YVerts ; y++ )
		{
			LVert->y = *VertPnt;
			if( x > 0 && x < WO->XVerts-1 && y > 0 && y < WO->YVerts-1 )
			{
				dx = *(VertPnt+WO->YVerts) - *(VertPnt-WO->YVerts) + 0.25F
				* ( *(VertPnt+WO->YVerts-1) + *(VertPnt+WO->YVerts+1) - *(VertPnt-WO->YVerts-1) - *(VertPnt-WO->YVerts+1) );
				dy = *(VertPnt+1) - *(VertPnt-1) + 0.25F
				* ( *(VertPnt+WO->YVerts+1) + *(VertPnt-WO->YVerts+1) - *(VertPnt+WO->YVerts-1) - *(VertPnt-WO->YVerts-1) );
			}else{
				dx = 0.0F;
				dy = 0.0F;
			}
			u = (float) ( x + dx * 0.025F) / WO->XVerts;
			v = 1.0F - (float) ( y + dy * 0.025F) / WO->YVerts;
			LVert->tu = (u * WO->uRange) + WO->uTL;
			LVert->tv = (v * WO->vRange) + WO->vTL;
			col = 192 + (int) ( dy * 8 );
			if( col > 255 )
				col = 255;
			if( col < 0 )
				col = 0;
			LVert->color = RGBA_MAKE( (int)(col * WO->Red) , (int)(col * WO->Green) , (int)(col * WO->Blue) , 128);
			LVert++;
			VertPnt++;
		}
	}
	/*	unlock the vertex buffer	*/
	if (!(FSUnlockVertexBuffer(/*WO->vertexBuffer*/&WO->renderObject)))
	{
		return;
	}
	smemo_invalidate_frame(); /* water verts just integrated (never in the persistent cache) */
}

/*===================================================================
	Procedure	:		Display the Water Stuff...
	Input		:		Nothing
	Output		:		Nothing
===================================================================*/
void DisplayWaterObject(WATEROBJECT * Wo)
{
	TempWorld = identity;
	TempWorld._41 = Wo->Pos.x;
	TempWorld._42 = Wo->Pos.y + Wo->offset;
	TempWorld._43 = Wo->Pos.z;

	if (!FSSetWorld(&TempWorld))
		return;

	cull_none();

	if (!draw_object(&Wo->renderObject))
		return;

	reset_cull();

	if (!FSSetWorld(&identity))
		return;
}

/*===================================================================
	Procedure	:		Loop around and see if there are any object in this group...
	Input		:		Nothing
	Output		:		Nothing
===================================================================*/

bool WaterSurfaces::object_collide( u_int16_t group , VECTOR *Origin, VECTOR *Offset, VECTOR *CollidePos , float Damage )
{
	WATEROBJECT * Wo;

	Wo = WaterObjectLink[group];

	while( Wo )
	{
		if( single_collide( Wo, Origin, Offset, CollidePos ,  Damage) )
		{
			return true;
		}
		Wo = Wo->GroupLink;
	}
	return false;
}
/*===================================================================
	Procedure	:		Am I In The Water..
	Input		:		Nothing
	Output		:		Nothing
===================================================================*/

bool WaterSurfaces::in_water( u_int16_t group , VECTOR *OrgPos , float * Damage)
{
	WATEROBJECT * Wo;

	if( GroupWaterInfo[group] == WATERSTATE_ALLWATER )
	{
		*Damage += GroupDamageInfo[group] * framelag;
		return true;
	}

	Wo = WaterObjectLink[group];

	while( Wo )
	{
		if( OrgPos->y < ( Wo->Pos.y + Wo->offset ) )
		{
			*Damage += Wo->Damage * framelag;
			return true;
		}

		Wo = Wo->GroupLink;
	}
	return false;
}

/*===================================================================
	Procedure	:		Collide the Water Object Stuff...
	Input		:		Nothing
	Output		:		Nothing
===================================================================*/
bool WaterSurfaces::single_collide( WATEROBJECT * Wo, VECTOR *Origin, VECTOR *Offset, VECTOR *CollidePos , float Damage)
{
	float	WaterOffset;
	float	Div;
	float	Num;
	float	t;
	VECTOR	Delta;
	float	WaterX;
	float	WaterY;
	int		IWaterX;
	int		IWaterY;
	float	dx;
	float	dy;
	float	Impact;
	float	NewDamage;
	float * Vels;
	float * Verts;
	VECTOR Pos;

	Pos = Wo->Pos;
	Pos.y += Wo->offset;



	WaterOffset = -DotProduct( &WaterNormal, &Pos );
	/*===================================================================
		Calculate T
	===================================================================*/
	Div = ( Offset->x * WaterNormal.x) +
		  ( Offset->y * WaterNormal.y) +
		  ( Offset->z * WaterNormal.z);
	if( Div == 0.0F ) return false;		/* Reject, Parallel */
	Num = ( ( Origin->x * WaterNormal.x ) +
			( Origin->y * WaterNormal.y ) +
			( Origin->z * WaterNormal.z ) ) + WaterOffset ;
	t = -( Num / Div );

	/*===================================================================
		Do Polygon collision
	===================================================================*/

	if( t < 0.0F ) return false;		/* Intersection behind origin */
	if( t > 1.0F ) return false;		/* Intersection Greater then ray length */

	CollidePos->x = ( Origin->x + ( Offset->x * t ) );
	CollidePos->y = ( Origin->y + ( Offset->y * t ) );
	CollidePos->z = ( Origin->z + ( Offset->z * t ) );

	Delta.x = CollidePos->x - Pos.x;
	Delta.y = CollidePos->y - Pos.y;
	Delta.z = CollidePos->z - Pos.z;

	// ideally, now we'd use the WaterNormal (and other parameters, such as water orientation in the plane)
	// to find out the correct WaterX and Y positions.  for now, though... [assuming WaterNormal is (0,1,0)]
	WaterX = ( Delta.x + ( (Wo->XVerts * VideoOpts.WATER_CELLSIZE ) * 0.5F ) ) / VideoOpts.WATER_CELLSIZE;
	WaterY = ( Delta.z + ( (Wo->YVerts * VideoOpts.WATER_CELLSIZE ) * 0.5F ) ) / VideoOpts.WATER_CELLSIZE;


	IWaterX = (int) floorf( WaterX );
	IWaterY = (int) floorf( WaterY );
	if ( IWaterX < 0 || IWaterX >= Wo->XVerts || IWaterY < 0 || IWaterY >= Wo->YVerts )
		return false;

	if( Damage == 0.0F )
	{
		return true;
	}
	else
	{
		NewDamage = Damage / 32.0F;
		if( NewDamage < 0.75F )
			NewDamage = 0.75F;
	}

	Impact = WATER_IMPACT * NewDamage;
	if( CollidePos->y > Origin->y )
		// shooting from below.....
		Impact *= -0.25F;

	dx = 1.0F - (float) ( WaterX - floorf( WaterX ) );
	dy = 1.0F - (float) ( WaterY - floorf( WaterY ) );

	Vels = Wo->Vels;

	Verts = Wo->Verts;

	Verts += (IWaterX * Wo->YVerts ) + IWaterY;
	Vels += (IWaterX * Wo->YVerts ) + IWaterY;
	if( *Verts < BLOCKSIZE &&
		*Verts > -BLOCKSIZE )
	{
		*Vels = Impact * ( dx + dy );
	}
	Verts++;
	Vels++;
	if ( IWaterY + 1 < Wo->YVerts)
	{
		if( *Verts < BLOCKSIZE &&
			*Verts  > -BLOCKSIZE )
		{
			*Vels = Impact * ( dx + ( 1 - dy ) );
		}
	}
	if ( IWaterX + 1 < Wo->XVerts )
	{
		Verts += Wo->YVerts-1;
		Vels += Wo->YVerts-1;

		if( *Verts < BLOCKSIZE &&
			*Verts > -BLOCKSIZE )
		{
			*Vels = Impact * ( ( 1 - dx ) + dy );
		}
		if ( IWaterY + 1 < Wo->YVerts )
		{
			Verts++;
			Vels++;
			if( *Verts < BLOCKSIZE &&
				*Verts > -BLOCKSIZE )
			{
				*Vels = Impact * ( ( 1 - dx ) + ( 1 - dy ) );
			}
		}
	}

	return true;
}


/*===================================================================
	Procedure	:	Water Process...
	Input		:	NOTHING
	Output		:	NOTHING
===================================================================*/
void WaterSurfaces::process()
{
	int			i;
	WATEROBJECT	* WO;

	WO = FirstWaterObject;
	if( !WO )
		return;
	for( i = 0 ; i < NumOfWaterObjects ; i++ )
	{
		if( WO->Status == WaterStatus::Draining )
		{
			WO->offset -= WO->DrainRate * framelag;
			if( WO->offset <= WO->MinLevel )
			{
				WO->offset = WO->MinLevel;
				WO->Status = WaterStatus::Static;
				if( WO->Trigger_WhenDrained != 0xffff )
					ApplyTriggerMod( &TrigMods[WO->Trigger_WhenDrained] );
			}
			GroupWaterLevel[WO->Group] = WO->Pos.y + WO->offset + VideoOpts.WATER_CELLSIZE;
		}else if( WO->Status == WaterStatus::Filling )
		{

			WO->offset += WO->FillRate * framelag;
			if( WO->offset >= WO->MaxLevel )
			{
				WO->offset = WO->MaxLevel;
				WO->Status = WaterStatus::Static;
				if( WO->Trigger_WhenFilled != 0xffff )
					ApplyTriggerMod( &TrigMods[WO->Trigger_WhenFilled] );
			}
			GroupWaterLevel[WO->Group] = WO->Pos.y + WO->offset + VideoOpts.WATER_CELLSIZE;
		}

		WO++;
	}
}

/*===================================================================
	Procedure	:	Trigger water to drain...
	Input		:	NOTHING
	Output		:	NOTHING
===================================================================*/
void WaterSurfaces::trigger_drain( u_int16_t * Data )
{
	WATEROBJECT	* WO;

	WO = FirstWaterObject;
	if( !WO )
		return;
	WO += *Data;
	WO->Status = WaterStatus::Draining;
}
/*===================================================================
	Procedure	:	Trigger water to fill...
	Input		:	NOTHING
	Output		:	NOTHING
===================================================================*/
void WaterSurfaces::trigger_fill( u_int16_t * Data )
{
	WATEROBJECT	* WO;

	WO = FirstWaterObject;
	if( !WO )
		return;
	WO += *Data;
	WO->Status = WaterStatus::Filling;
}


/*===================================================================
	Procedure	:	Get Water Colour in group
	Input		:	u_int16_t		Group
				:	u_int8_t	*	Red;
				:	u_int8_t	*	Green;
				:	u_int8_t	*	Blue;
	Output		:	Nothing
===================================================================*/
void GetWaterColour( u_int16_t Group, u_int8_t * Red, u_int8_t * Green, u_int8_t * Blue )
{
	*Red	= (u_int8_t) ( Water.tint_red( Group ) * 255.0F );
	*Green	= (u_int8_t) ( Water.tint_green( Group ) * 255.0F );
	*Blue	= (u_int8_t) ( Water.tint_blue( Group ) * 255.0F );
}

WaterSurfaces Water;

bool PreWaterLoad( char * Filename ) { return Water.pre_load( Filename ); }
bool WaterLoad( void ) { return Water.load(); }
void WaterRelease( void ) { Water.release(); }
void AddWaterLink( WATEROBJECT * WO ) { Water.add_link( WO ); }
void GroupWaterProcessDisplay( u_int16_t group ) { Water.process_display( group ); }
bool WaterObjectCollide( u_int16_t group , VECTOR *Origin, VECTOR *Offset, VECTOR *CollidePos , float Damage ) { return Water.object_collide( group, Origin, Offset, CollidePos, Damage ); }
bool InWater( u_int16_t group , VECTOR *OrgPos , float * Damage ) { return Water.in_water( group, OrgPos, Damage ); }
bool SingleWaterObjectCollide( WATEROBJECT * Wo, VECTOR *Origin, VECTOR *Offset, VECTOR *CollidePos , float Damage ) { return Water.single_collide( Wo, Origin, Offset, CollidePos, Damage ); }
void WaterProcess( void ) { Water.process(); }
void TriggerWaterDrain( u_int16_t * Data ) { Water.trigger_drain( Data ); }
void TriggerWaterFill( u_int16_t * Data ) { Water.trigger_fill( Data ); }
