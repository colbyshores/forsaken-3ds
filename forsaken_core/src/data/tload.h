/*==========================================================================
 *
 *
 *  File:       tload.h
 *  Content:    Tload include file
 *
 *
 ***************************************************************************/

#pragma once

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
	int		state;
	u_int16_t num_texture_files;
	u_int16_t				Xsize[MAXTPAGESPERTLOAD];		// the Xsize now
	u_int16_t				Ysize[MAXTPAGESPERTLOAD];		// the Ysize now
	bool				ColourKey[MAXTPAGESPERTLOAD];	// 0 not colour keyed
    char                ImageFile[MAXTPAGESPERTLOAD][256]; /* files */
    LPTEXTURE			lpTexture[MAXTPAGESPERTLOAD]; /* texture objs */
	bool				MipMap[MAXTPAGESPERTLOAD];
	bool				PlaceHolder[MAXTPAGESPERTLOAD];	// is the texture a placeholder ( for subsequent dynamic loading of textures? )
	char				*PlaceHolderFile[MAXTPAGESPERTLOAD];	// current full path of file occupying placeholder ( needed to restore surface )
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
