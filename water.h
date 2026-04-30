
#ifndef WATER_H
#define WATER_H

#define	ROOTTWO  (1.0F / (float)sqrt(2.0F))
#define	SPECIALROOTTWO (1.0F / (4.0F+(1.0F / (float)sqrt(2.0F))*4.0F))
/* Water mesh cell budget. Original 32×32 = 1024 was sized for the
 * 1998 levels (largest is fedbankv at ~600 cells). Remaster /
 * Night-Dive levels can exceed this — tuben64's water is 37×29 =
 * 1073 cells. Bumped to 64×64 = 4096 to cover known cases plus
 * comfortable headroom; cost is ~12 KB of BSS for the global
 * Fc[MAXMESHX*MAXMESHY] scratch buffer. WaterLoad still validates
 * XVerts*YVerts against MAXMESHX*MAXMESHY, so a level with bigger
 * water will still bail (and now cleans up properly on bail). */
#define	MAXMESHX 64
#define	MAXMESHY 64
#define WATER_IMPACT	(-4.0F)
#define	ONEOVER255	(1.0F/255.0F)

#define	WATERSTATE_NOWATER 0
#define	WATERSTATE_SOMEWATER 1
#define	WATERSTATE_ALLWATER 2

#define	WATERSTATUS_STATIC		0
#define	WATERSTATUS_DRAINING	1
#define	WATERSTATUS_FILLING		2

typedef struct WATEROBJECT{
	u_int32_t	AnimOncePerFrame;	// used for stuff that is displayed more than once in a single frame..or maybe called for multiple cameras..
	u_int16_t	Status;
	u_int16_t	Group;
	VECTOR	Pos;
	float	offset;
	float	XSize;
	float	YSize;
	float	uTL,vTL;			// uv coords Top Left
	float	uBR,vBR;			// uv coords Bottom Right
	float	uRange;
	float	vRange;
	int		YVerts;
	int		XVerts;
	int		num_of_verts;
	float *	Verts;				// Y positions...		
	float *	Vels;				// Y Velocities..

	float	Red;
	float	Green;
	float	Blue;

//	LPDIRECT3DEXECUTEBUFFER	lpExBuf;
//	LPVERTEXBUFFER lpVertexBuffer;
	RENDEROBJECT renderObject;

	struct WATEROBJECT * GroupLink;

	float	MaxLevel;
	float	MinLevel;
	float	FillRate;
	float	DrainRate;
	float	Density;
	float	MaxWaveSize;
	u_int16_t	Trigger_WhenFilled;
	u_int16_t	Trigger_WhenDrained;
	float	Damage;


}WATEROBJECT;




//--------------------------------------------------------------------------
// Prototypes
//--------------------------------------------------------------------------
void GroupWaterProcessDisplay( u_int16_t group );
bool PreWaterLoad( char * Filename );
bool WaterLoad( void );
bool PreWaterLoad( char * Filename );
void WaterRelease( void );
void AddWaterLink(WATEROBJECT * WO);
void WaterProcess( void );
void TriggerWaterDrain( u_int16_t * Data );
void TriggerWaterFill( u_int16_t * Data );
bool SingleWaterObjectCollide( WATEROBJECT * Wo, VECTOR *Origin, VECTOR *Offset, VECTOR *CollidePos , float Damage);
bool InWater( u_int16_t group , VECTOR *OrgPos , float * Damage);
bool WaterObjectCollide( u_int16_t group , VECTOR *Origin, VECTOR *Offset, VECTOR *CollidePos , float Damage );
void GetWaterColour( u_int16_t Group, u_int8_t * Red, u_int8_t * Green, u_int8_t * Blue );

#endif // WATER_H
