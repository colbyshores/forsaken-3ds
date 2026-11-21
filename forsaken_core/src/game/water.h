#pragma once

/* water.h -- level water: rippling surfaces that fill and drain per group
 *
 * water.cpp owns the per-level WATEROBJECTs (PreWaterLoad reads the level
 * file, WaterLoad builds the objects later in the load) and the per-group
 * tables GroupWaterInfo (WATERSTATE_*), GroupWaterLevel (world Y of the
 * surface) and the tint colours, all private to WaterSurfaces. A surface
 * is an XVerts x YVerts grid WATER_CELLSIZE world units apart, capped at
 * MAXMESHX * MAXMESHY cells;
 * Verts and Vels are per-vertex Y offsets and velocities. offset moves
 * between MinLevel and MaxLevel at FillRate / DrainRate (file rates / 60,
 * scaled by framelag) and fires Trigger_WhenFilled / _WhenDrained (0xffff =
 * none) when it stops. InWater adds Damage * framelag to its out-parameter.
 */

#include "game/main.h"
#include "game/constants.h"

constexpr auto ROOTTWO = (1.0F / (float)sqrt(2.0F));
constexpr auto SPECIALROOTTWO = (1.0F / (4.0F+(1.0F / (float)sqrt(2.0F))*4.0F));
/* Water mesh cell budget. Original 32×32 = 1024 was sized for the
 * 1998 levels (largest is fedbankv at ~600 cells). Remaster /
 * Night-Dive levels can exceed this — tuben64's water is 37×29 =
 * 1073 cells. Bumped to 64×64 = 4096 to cover known cases plus
 * comfortable headroom; cost is ~12 KB of BSS for water.cpp's
 * Fc[MAXMESHX*MAXMESHY] scratch buffer. WaterLoad still validates
 * XVerts*YVerts against MAXMESHX*MAXMESHY, so a level with bigger
 * water will still bail (and now cleans up properly on bail). */
constexpr int MAXMESHX = 64;
constexpr int MAXMESHY = 64;
constexpr float WATER_IMPACT = (-4.0F);
constexpr auto ONEOVER255 = (1.0F/255.0F);

constexpr int WATERSTATE_NOWATER = 0;
constexpr int WATERSTATE_ALLWATER = 2;

/* A water object's level: steady, draining or filling. */
enum class WaterStatus : u_int16_t
{
	Static,
	Draining,
	Filling,
};

struct WATEROBJECT{
	u_int32_t	AnimOncePerFrame;	// used for stuff that is displayed more than once in a single frame..or maybe called for multiple cameras..
	WaterStatus	Status;
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


};




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

/* Defined in water.cpp; shared with the modules that use it. */
extern float WATER_CELLSIZE;

/* The level's water surfaces (one instance, Water): the WATEROBJECT array
 * and its count, each group's chain of surfaces and its damage rate, each
 * group's water state, surface height and tint, the water texture page, and
 * the .WAT buffer kept between PreWaterLoad and WaterLoad. Built at level
 * load and freed at release; walked each frame (fill/drain), by each visible
 * group's display, and as ships, models and bullets move. WaterFade (display
 * tint scale) and WaterNormal (the surface plane's normal) are never changed.
 * Fc is the mesh update's scratch copy of the vertex heights. The level
 * lighting's colour pass asks dry, submerged, below_surface and the tint. */
class WaterSurfaces {
	public:
	bool pre_load( char * Filename );
	bool load();
	void release();
	void add_link( WATEROBJECT * WO );
	/* The per-frame, per-group and per-move members are always_inline and
	 * defined in water.cpp, so GroupWaterProcessDisplay, WaterObjectCollide,
	 * InWater and WaterProcess keep their bodies rather than becoming tail
	 * calls. Other files call those C functions. */
	[[gnu::always_inline]] inline void process_display( u_int16_t group );
	[[gnu::always_inline]] inline bool object_collide( u_int16_t group , VECTOR *Origin, VECTOR *Offset, VECTOR *CollidePos , float Damage );
	[[gnu::always_inline]] inline bool in_water( u_int16_t group , VECTOR *OrgPos , float * Damage );
	/* Static: it uses no instance state, so each bullet's test takes its
	 * arguments in the same registers as SingleWaterObjectCollide did. */
	static bool single_collide( WATEROBJECT * Wo, VECTOR *Origin, VECTOR *Offset, VECTOR *CollidePos , float Damage );
	[[gnu::always_inline]] inline void process();
	void trigger_drain( u_int16_t * Data );
	void trigger_fill( u_int16_t * Data );

	/* Each visible group's colour pass, each frame (lights.cpp). */
	bool dry( u_int16_t group ) const { return GroupWaterInfo[group] == WATERSTATE_NOWATER; }
	bool submerged( u_int16_t group ) const { return GroupWaterInfo[group] == WATERSTATE_ALLWATER; }
	bool below_surface( u_int16_t group, float y ) const { return y < GroupWaterLevel[group]; }
	float tint_red( u_int16_t group ) const { return GroupWaterIntensity_Red[group]; }
	float tint_green( u_int16_t group ) const { return GroupWaterIntensity_Green[group]; }
	float tint_blue( u_int16_t group ) const { return GroupWaterIntensity_Blue[group]; }

	private:
	bool init_object( WATEROBJECT * WO );
	/* Out of line, as UpdateWaterMesh was: inlining it would put the whole
	 * mesh integration into each group's process_display. */
	[[gnu::noinline]] static void update_mesh( WATEROBJECT * WO );

	WATEROBJECT	* WaterObjectLink[MAXGROUPS] = {};
	WATEROBJECT	* FirstWaterObject = nullptr;
	u_int16_t	NumOfWaterObjects = 0;
	float	WaterFade = 1.0F;
	float	GroupDamageInfo[MAXGROUPS] = {};
	u_int16_t	WaterTPage = 0;
	char	* WaterBuffer = nullptr;
	char	* OrgWaterBuffer = nullptr;
	/* Static: its own symbol, as the old global was, so single_collide keeps
	 * one literal and one base register for it across the DotProduct call. */
	static VECTOR WaterNormal;
	/* Static: update_mesh's only state, so update_mesh is static too and,
	 * like UpdateWaterMesh, takes no this pointer. */
	static float	Fc[MAXMESHX*MAXMESHY];
	/* Static: each table stays its own symbol, as the old globals were, so
	 * the lighting's per-group colour pass keeps a plain indexed load from
	 * each one. */
	static u_int16_t	GroupWaterInfo[MAXGROUPS];
	static float	GroupWaterLevel[MAXGROUPS];
	static float	GroupWaterIntensity_Red[MAXGROUPS];
	static float	GroupWaterIntensity_Green[MAXGROUPS];
	static float	GroupWaterIntensity_Blue[MAXGROUPS];
};

extern WaterSurfaces Water;
