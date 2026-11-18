#pragma once

/* tload.h -- the texture page table shared by the level, models and sprites
 *
 * Tloadheader is the one table of texture pages for the current title screen
 * or level. Loaders register pages with AddTexture (which returns the index
 * they keep, e.g. MLOADHEADER::TloadIndex, and returns the existing index if
 * a page of the same file name is already there), then Tload creates them
 * all through the backend's FSCreateTexture, which fills Xsize / Ysize /
 * ColourKey from the image actually loaded (on the 3DS an HD .t3x when one
 * exists, otherwise the PNG). Indices are only meaningful in Tloadheader:
 * code holding a loader's local texture number must map it through that
 * loader's TloadIndex first.
 *
 * A placeholder page (AddTexture with an empty name) is a slot whose image
 * is chosen at run time: the title screen writes a file name into
 * PlaceHolderFile and calls TloadReloadPlaceHolder (level preview pictures,
 * saved-game pictures). GetLevelTexturePath picks the file for a level
 * texture: the level's own textures folder, then data\textures\original
 * when UseOriginalTextures is set, then data\textures, then default.png.
 */

#include "data/file.h"
#include "rendering/render.h"
#include "math/new3d.h"

/*
 * defines
 */
constexpr int MAXTPAGESPERTLOAD = 50;

constexpr int DONTLOAD_TPAGES = 0;
constexpr int LOAD_TPAGES = 1;
constexpr int LOAD_TPAGES_PLACEHOLDER = 4;
constexpr int LOAD_TPAGES_FINISH = 8;

/*
 * structures
 */

typedef struct TLOADHEADER{
	int		state;						// true once Tload has created every page
	u_int16_t num_texture_files;
	u_int16_t				Xsize[MAXTPAGESPERTLOAD];		// width and height of the image loaded (for a
	u_int16_t				Ysize[MAXTPAGESPERTLOAD];		// placeholder, the size requested until filled)
	bool				ColourKey[MAXTPAGESPERTLOAD];	// black texels are transparent
    char                ImageFile[MAXTPAGESPERTLOAD][256]; // path as registered (empty for a placeholder)
    TEXTURE_HANDLE			TexHandle[MAXTPAGESPERTLOAD];	// the backend texture
	bool				MipMap[MAXTPAGESPERTLOAD];		// create a mip chain
	bool				PlaceHolder[MAXTPAGESPERTLOAD];	// image chosen at run time (see above)
	char				*PlaceHolderFile[MAXTPAGESPERTLOAD];	// 256-byte buffer: the file now in the placeholder
}TLOADHEADER;

/*
 * fn prototypes
 */
bool InitTload( TLOADHEADER * Tloadheader  );
bool Tload( TLOADHEADER * Tloadheader );
void TloadReleaseTexture(TLOADHEADER * Tloadheader, int n);
void ReleaseTloadheader( TLOADHEADER * Tloadheader );

bool TloadAllTextures(TLOADHEADER * Tloadheader);

int16_t	FindTexture( TLOADHEADER * Tloadheader , const char * Name );

void GetLevelTexturePath( char * destination, char * file, char * level );
int16_t	AddTexture( TLOADHEADER * Tloadheader , const char * Name , u_int16_t ColourKey , bool MipMap, int16_t xsize, int16_t ysize );
int16_t	FindTexture( TLOADHEADER * Tloadheader , const char * Name );

bool TloadReloadPlaceHolder( TLOADHEADER *Tloadheader, int16_t n );

extern bool use_original_textures;

/* Defined in tload.cpp; shared with the modules that use it. */
extern TLOADHEADER Tloadheader;
extern double Gamma;
