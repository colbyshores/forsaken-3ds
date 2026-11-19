#pragma once

/* mxload.h -- static model (.mx) loader: MXLOADHEADER with its groups, fire
 * points and spot effects; the file layout and loader are described in
 * mxload.cpp.
 */

#include "game/main.h"
#include "data/file.h"
#include "rendering/render.h"
#include "math/new3d.h"
#include "data/tload.h"
#include "data/mload.h"
#include "rendering/transexe.h"

/*
 * defines
 */
constexpr int MAXMXGROUPS = 1;
constexpr int MAXMXATEXTUREGROUPSPER = 8;
constexpr int MAXTPAGESPERMXLOAD = 8;

enum {

	PV_FirePoint,		// 0
	PV_SpotFX,			// 1

};

 /*
 * structures
 */

struct PVFIREPOINT{

	u_int16_t		ID;
	VECTOR		Pos;
	VECTOR		Dir;
	VECTOR		Up;

};

struct PVSPOTFX {

	u_int16_t		Type;
	int8_t		Primary;
	int8_t		Secondary;
	u_int8_t		Red;
	u_int8_t		Green;
	u_int8_t		Blue;
	VECTOR		Pos;
	VECTOR		DirVector;
	VECTOR		UpVector;
	float		StartDelay;
	float		ActiveDelay;
	float		InactiveDelay;
	int16_t		SoundFX;
	float		SoundFXVolume;
	float		SoundFXSpeed;

};


struct MXGROUP{
	u_int16_t		num_meshes;
	u_int16_t		mesh_type[MAXMESHESPERGROUP];
	u_int16_t		num_verts_per_mesh[MAXMESHESPERGROUP];
	u_int16_t	num_polys_per_mesh[MAXMESHESPERGROUP];
	u_int16_t	num_texture_groups[MAXMXATEXTUREGROUPSPER];
	RENDEROBJECT renderObject[MAXMXATEXTUREGROUPSPER];
	LVERTEX *originalVerts[MAXMESHESPERGROUP];

	TRIANGLE *poly_ptr	[MAXMESHESPERGROUP];
	char		name[16];
	u_int16_t		num_animating_polys[MAXMESHESPERGROUP];
	POLYANIM *	polyanim[MAXMESHESPERGROUP];

};


struct MXLOADHEADER{
	int					state;								// false if not loaded properly...
	u_int32_t				AnimOncePerFrame;					// used for stuff that is displayed more than once in a single frame..
	char		*		OrgAddr;							// the .mx file, from PreMxload until Mxload has copied it out
	const char	*		Buffer;								// where Mxload starts reading OrgAddr
	u_int16_t				num_texture_files;
	u_int16_t				num_groups;
	MXGROUP				Group[MAXMXGROUPS];
	int16_t				TloadIndex[MAXTPAGESPERMXLOAD];		// which texture in the Tloadheader....
	char                ImageFile[MAXTPAGESPERMXLOAD][32];	// Texture Files....
	ANIMDATA			AnimData;
	u_int16_t				NumFirePoints;						// Number of FirePoints
	PVFIREPOINT	*		FirePoints;							// FirePoint Stuff
	u_int16_t				NumSpotFX;							// Number of SpotFX
	PVSPOTFX	*		SpotFX;								// SpotFX Stuff
	int					LOD;
	VECTOR				Center;								// Center Pos
	VECTOR				Sizes;								// X,Y,Z Sizes
};

/*
 * fn prototypes
 */
bool PreMxload( char * Filename, MXLOADHEADER * Mxloadheader , bool Panel, bool LevelSpecific );
bool Mxload( char * Filename, MXLOADHEADER * Mxloadheader , bool Panel, bool StoreTriangles );
bool ExecuteMxloadHeader( MXLOADHEADER * Mxloadheader, u_int16_t Model );

void ReleaseMxloadheader( MXLOADHEADER * Mxloadheader );

bool ReallyExecuteMxloadHeader( MXLOADHEADER * Mxloadheader, u_int16_t Model );

void ModelTextureAnimation( MXLOADHEADER * Mxloadheader  );
bool RestoreColourMxloadHeader( MXLOADHEADER * Mxloadheader1 );
