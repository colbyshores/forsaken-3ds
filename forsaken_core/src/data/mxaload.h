#pragma once

/* mxaload.h -- animated model (.mxa) loader: MXALOADHEADER, the MXAVERT
 * frame record and InterpFrames; the file layout and loader are described in
 * mxaload.cpp.
 */

/*===================================================================
	Includes
===================================================================*/

#include "game/main.h"
#include "data/file.h"
#include "rendering/render.h"
#include "math/new3d.h"
#include "data/tload.h"
#include "data/mload.h"
#include "rendering/transexe.h"
#include "game/constants.h"
#include "data/mxload.h"

/*===================================================================
	Defines
===================================================================*/
constexpr int MAX_MXAGROUPS = 1;
constexpr int MAXTPAGESPERMXALOAD = 8;
#ifdef __3DS__
/* Must be == MAXMODELHEADERS — model indices are shared between MX and MXA
 * headers, so the smaller of the two limits the system. See models.h for
 * the sizing rationale. */
constexpr int MAXMXAMODELHEADERS = 608;
#else
constexpr int MAXMXAMODELHEADERS = 1024;
#endif


constexpr auto MXA_ANIM_POS = (1 << 0);
constexpr auto MXA_ANIM_RGB = (1 << 1);
constexpr auto MXA_ANIM_UV = (1 << 2);

/*===================================================================
	Structures
===================================================================*/
typedef struct MXAVERT {
	float x;
	float y;
	float z;
	COLOR color;
	COLOR specular;
	float tu;
	float tv;
	int32_t flags;
} MXAVERT;
static_assert( sizeof( MXAVERT ) == 32, "MXAVERT is a file record" );


typedef struct COLOR_RGBA {
	u_int8_t b;
	u_int8_t g;
	u_int8_t r;
	u_int8_t a;
} COLOR_RGBA;

typedef struct MXAGROUP{
	u_int16_t	num_meshes;
	u_int16_t	mesh_type[MAXMESHESPERGROUP];
	u_int16_t	num_verts_per_mesh[MAXMESHESPERGROUP];
	u_int16_t	num_polys_per_mesh[MAXMESHESPERGROUP];
	u_int16_t	num_texture_groups[MAXMXATEXTUREGROUPSPER];
	RENDEROBJECT renderObject[MAXMESHESPERGROUP];
	LVERTEX *originalVerts[MAXMESHESPERGROUP];
	TRIANGLE *poly_ptr	[MAXMESHESPERGROUP];
	u_int32_t	texture_group_vert_off[MAXMESHESPERGROUP][MAXMXATEXTUREGROUPSPER];
	char	name[16];
}MXAGROUP;


typedef u_int16_t MXAVERTCOUNT[MAX_MXAGROUPS][MAXMESHESPERGROUP][MAXMXATEXTUREGROUPSPER];		// points to the begining of each set of verts...
typedef	MXAVERT	*MXAVERTFRAME[MAX_MXAGROUPS][MAXMESHESPERGROUP][MAXMXATEXTUREGROUPSPER];		// points to the begining of each set of verts...

typedef struct MXALOADHEADER{

	int					state;								// false if not loaded properly...
	int					CurrentFrame;
	int					WantedFrame;
	float				Interp;								// how close to the wanted frame....
	float				Time;
	char		*		OrgAddr;							// the .mxa file, from PreMxaload until Mxaload has copied it out
	const char	*		Buffer;								// where Mxaload starts reading OrgAddr
	u_int16_t				num_texture_files;
	u_int16_t				num_groups;
	MXAGROUP			Group[MAX_MXAGROUPS];
	int16_t				TloadIndex[MAXTPAGESPERMXALOAD];		// which texture in the Tloadheader....
	char                ImageFile[MAXTPAGESPERMXALOAD][32];	// Texture Files....
	u_int16_t				num_frames;
	MXAVERTCOUNT *		num_anim_vertices;// points to the begining of each set of verts...
	MXAVERTFRAME *		frame_pnts;		  // points to the begining of each set of verts...
	MXAVERT		*		anim_verts;		  // every frame's MXAVERTs; frame_pnts point into this
	int					AllocateTPage;
	u_int16_t				NumFirePoints;							// Number of FirePoints
	PVFIREPOINT	*		FirePoints;								// FirePoints
	u_int16_t				NumSpotFX;								// Number of SpotFX
	PVSPOTFX	*		SpotFX;									// SpotFX
	VECTOR				Center;									// Center Pos
	VECTOR				Sizes;									// X,Y,Z Sizes

}MXALOADHEADER;

/*===================================================================
	Prototypes
===================================================================*/
bool PreMxaload( char * Filename, MXALOADHEADER * Mxaloadheaders, int header_num, bool LevelSpecific );
bool Mxaload( char * Filename, MXALOADHEADER * Mxaloadheader, bool StoreTriangles  );
bool ExecuteMxaloadHeader( MXALOADHEADER * Mxaloadheader, u_int16_t group );
void ReleaseMxaloadheader( MXALOADHEADER * Mxaloadheader );
bool InterpFrames( MXALOADHEADER * Mxaloadheader , int FromFrame, int ToFrame , float Interp );

/* Defined in mxaload.cpp; shared with the modules that use it. */
extern MXALOADHEADER * MxaModelHeaders;
