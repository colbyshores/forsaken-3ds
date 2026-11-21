#pragma once

/* mload.h -- level mesh (.mxv) loader: MLOADHEADER with its groups, portals
 * and visibility trees, the .mxv file records, start positions and texture
 * animation; the file layout and loader are described in mload.cpp.
 */

#include "game/main.h"
#include "data/file.h"
#include "rendering/render.h"
#include "math/new3d.h"
#include "data/tload.h"
#include "rendering/transexe.h"
#include "game/constants.h"

/*
 * defines
 */
constexpr int MAXTPAGESPERMLOAD = 8;
constexpr int MAXMESHESPERGROUP = 16;
constexpr int MAXPORTALSPERGROUP = 64;
constexpr int MAXVERTSPERPORTAL = 32;
constexpr int MAXPOLYSPERPORTAL = 32;
constexpr int MAXVERTSPERPORTALPOLY = 4;
constexpr auto MAXSTARTPOSITIONS = MAXGROUPS;
constexpr auto HASTRANSPARENCIES = 0x001;
constexpr auto HASENVIROMENTMAP = 0x002;


constexpr int MAXANIMSEQS = 128;
constexpr int MAXANIMLOOPSTACK = 8;
constexpr int TEXTUREANIM_Stop = 0;
constexpr int TEXTUREANIM_Go = 1;


constexpr auto TEXANIM_COMMAND = (0x8000); // (top bit set if command otherwise frame number)
constexpr auto TEXANIM_DURATION = (0x8001); // default frame duration
constexpr auto TEXANIM_DURATION1 = (0x8002); // next frame duration only
constexpr auto TEXANIM_LOOPINIT = (0x8003); // loop var, number of repeats
constexpr auto TEXANIM_LOOPEND = (0x8004); // loop var, start of loop
constexpr auto TEXANIM_END = (0xFFFF); // (end of animation)

constexpr auto LEVEL_SPECIFIC = true;
constexpr auto NOT_LEVEL_SPECIFIC = false;

/*
 * structures
 */

struct PROJPOINT {
	float u, v;
};



struct MCFACE {
	u_int32_t type;
	float nx;
	float ny;
	float nz;
	float D;
	PROJPOINT v[ 4 ];
};
static_assert( sizeof( MCFACE ) == 52, "MCFACE is a file record" );


struct VERTEXCELL{
	u_int16_t	num_verts_in_cell;
	u_int16_t	start_vert_in_cell;
};


struct STARTPOS{
	VECTOR	Pos;
	VECTOR	Dir;
	VECTOR	Up;
	u_int16_t	Group;
};
static_assert( sizeof( STARTPOS ) == 40, "STARTPOS is a file record (2 trailing pad bytes)" );

struct GAMESTARTPOS{
	VECTOR	Pos;
	VECTOR	Dir;
	VECTOR	Up;
	u_int16_t	Group;
	u_int16_t	NextInGroup;
};

struct PORTALPOLY
{
	u_int16_t num_vertices_in_poly;
	VERT   Verts[MAXVERTSPERPORTALPOLY];
};


struct VISTREE;

struct PORTAL;


struct VISTREE
{
	PORTAL *portal;
	u_int16_t group;
	u_int16_t num_visible;
	VISTREE *visible;
};



struct PORTAL{
	u_int16_t num_vertices_in_portal;
	VERT   Verts[MAXVERTSPERPORTAL];
	u_int16_t num_polys_in_portal;
	MCFACE Poly[MAXPOLYSPERPORTAL];
	VISTREE visible;
	VERT centroid;
};


struct TANIMUV {
	float u, v;
};


struct POLYANIM {
	u_int16_t	State;
	u_int16_t	animation;
	float	CurrentTime;
	float	MasterTime;
	u_int16_t	CurrentOffset;
	u_int16_t	currentframe;	// what we are displaying now...
	u_int16_t	newframe;		// what we want ti display..
	u_int16_t	frames;			// number of frames
	u_int16_t	vertices;		// number of verts...
	u_int16_t	loopstack[MAXANIMLOOPSTACK];
	int	*	vert;
	TANIMUV	* UVs;
	char	Tag[32];
	int16_t	WhenStoppedTriggerMod;
};

struct TEXTUREANIMINFOINDEX{
	u_int16_t	Group;
	u_int16_t	Mesh;
	u_int16_t	PolyAnim;
	u_int16_t	InitState;
};


struct ANIMDATA {
	u_int16_t	num_animating_polys;
	TEXTUREANIMINFOINDEX * TAnimInfoIndex;
	u_int16_t	num_animations;
	u_int16_t * AnimSeq[MAXANIMSEQS];
};


struct LVLGROUP{
	u_int16_t	num_meshes;
	u_int16_t	mesh_type[MAXMESHESPERGROUP];
	u_int16_t	num_verts_per_mesh[MAXMESHESPERGROUP];
	LEVELRENDEROBJECT			renderObject[MAXMESHESPERGROUP];
	LVERTEX *originalVerts[MAXMESHESPERGROUP];
	char	name[32];
	VERT	center;
	VERT	half_size;
	VECTOR	up;
	u_int16_t	num_portals;
	PORTAL  * Portal;
	int16_t	BGClear_Flag;
	float	BGClear_Red;
	float	BGClear_Green;
	float	BGClear_Blue;

	VECTOR	cell_origin[MAXMESHESPERGROUP];
	u_int16_t	xcells[MAXMESHESPERGROUP];
	u_int16_t	ycells[MAXMESHESPERGROUP];
	u_int16_t	zcells[MAXMESHESPERGROUP];
	u_int16_t	numofcells[MAXMESHESPERGROUP];
	u_int16_t	num_vertex_indices[MAXMESHESPERGROUP];
	u_int16_t * vertex_index_pnt[MAXMESHESPERGROUP];
	VERTEXCELL * vertex_cell_pnt[MAXMESHESPERGROUP];
	COLOR * colour_cell_pnt[MAXMESHESPERGROUP];

	u_int16_t	num_animating_polys[MAXMESHESPERGROUP];

	POLYANIM *polyanim[MAXMESHESPERGROUP];
	u_int16_t	StartPosInThisGroup;
	u_int32_t				AnimOncePerFrame;					// used for stuff that is displayed more than once in a single frame..
	u_int8_t		color_clean;						// the meshes hold their baked colours: XLight1Group can skip restoring them
	u_int8_t		color_clean_pad[3];
};


struct MLOADHEADER{
	int					state;								// false if not loaded properly...
	char		*		OrgAddr;							// the .mxv file, from PreMload until Mload has copied it out
	const char	*		Buffer;								// where Mload starts reading OrgAddr
	u_int16_t				num_texture_files;
	u_int16_t				num_groups;
	LVLGROUP				Group[MAXGROUPS];
	int16_t				TloadIndex[MAXTPAGESPERMLOAD];		// which texture in the Tloadheader....
	char                ImageFile[MAXTPAGESPERMLOAD][32];	// Texture Files....
	float				CellSize;
	ANIMDATA			AnimData;
};



struct MVERT {
	float x;
	float y;
	float z;
	float nx;
	float ny;
	float nz;
	float tu;
	float tv;
};

struct MFACE {
	u_int16_t v1;
	u_int16_t v2;
	u_int16_t v3;
	u_int16_t pad;
	float nx;
	float ny;
	float nz;
};
static_assert( sizeof( MFACE ) == 20, "MFACE is a file record" );


/*
 * fn prototypes
 */
bool PreMload( char * Filename, MLOADHEADER * Mloadheader  );
bool Mload( char * Filename, MLOADHEADER * Mloadheader );
bool ExecuteMloadHeader( MLOADHEADER * Mloadheader  );
void ReleaseMloadheader( MLOADHEADER * Mloadheader );

bool ExecuteSingleGroupMloadHeader( MLOADHEADER * Mloadheader, u_int16_t group  );
void BackGroundTextureAnimation( MLOADHEADER * Mloadheader , u_int16_t group );
u_int16_t * HandleAnimCommands( POLYANIM * PolyAnim , u_int16_t * AnimData , u_int16_t * OrgAnimAdr);

void TriggerBackgroundAnimationGo( u_int16_t * Data );
void TriggerBackgroundAnimationStop( u_int16_t * Data );

extern FILE *LoadTextureAnimations( FILE *f );
extern FILE *SaveTextureAnimations( FILE *f );

/* Defined in mload.cpp; shared with the modules that use it. */
extern float SoundInfo[ MAXGROUPS ][ MAXGROUPS ];
void FixUV( TRIANGLE *Tri, LVERTEX *Vert, u_int16_t Tpage, LVERTEX *Orig_Vert );
void FixUV_Anim( POLYANIM *PolyAnim, LVERTEX *Vert, LVERTEX *Orig_Vert );
void InitSoundInfo( MLOADHEADER * Mloadheader );
extern u_int16_t num_start_positions;
extern GAMESTARTPOS StartPositions[ MAXSTARTPOSITIONS ];
extern u_int16_t last_start_position;
extern u_int16_t FirstStartPositionInGroup[ MAXGROUPS ];
extern u_int16_t GroupTris[ MAXGROUPS ];
