/* tload.cpp -- builds and releases the shared texture page table
 * (Tloadheader); see tload.h. */

#include "ui/title.h"
#include "util/platform.h"
#include "rendering/text.h"

/*===================================================================
		Include File...	
===================================================================*/

#include "game/main.h"
#include "data/tload.h"
#include "util/util.h"
#include "stdio.h"


/*===================================================================
		Globals...	
===================================================================*/
TLOADHEADER	Tloadheader;
double	Gamma = 1.0;

/*===================================================================
	Procedure	:		Init a Tloadheader
	Input		:		TLOADHEADER *
	Output		:		bool false/true
===================================================================*/
bool InitTload( TLOADHEADER * Tloadheader  )
{
	int i;
	memset( Tloadheader, 0, sizeof(TLOADHEADER) );
	Tloadheader->num_texture_files = 0;
	for( i = 0 ; i < MAXTPAGESPERTLOAD ; i++ )
	{
		Tloadheader->lpTexture[i]     = NULL;

		Tloadheader->MipMap[i]		  = false;
		Tloadheader->PlaceHolder[i]	  = false;

		Tloadheader->Xsize[ i ] = 0;		
		Tloadheader->Ysize[ i ] = 0;
		
		Tloadheader->PlaceHolderFile[ i ] = NULL;
	}
	return	true;
}


/*===================================================================
	Procedure	:		Load All textures associated with a level
	Input		:		TLOADHEADER *
	Output		:		bool false/true
===================================================================*/
bool Tload( TLOADHEADER * Tloadheader  )
{
	int	i;

	build_gamma_table( Gamma );

	// Tloadheader is not valid until everything has been done..
	Tloadheader->state = false;

	// allocate space for placeholder file names
	for( i = 0 ; i < Tloadheader->num_texture_files ; i ++ )
	{
		if ( Tloadheader->PlaceHolder[ i ] )
		{
			if(!Tloadheader->PlaceHolderFile[ i ])
				Tloadheader->PlaceHolderFile[ i ] = ( char * )malloc( sizeof( char ) * 256 );
			DebugPrintf( "Tload: Placeholder ( texture %2d )\n", i );
		}
	}

	if ( Tloadheader->num_texture_files != 0 )
	{
		//	load in and convert all textures
		if ( TloadAllTextures( Tloadheader ) != true)
		{
			Msg( "TLoadAllTextures() Failed\n" );
			return false;
		}
	}

	// Tloadheader is valid
	Tloadheader->state = true;

	return( true );
}
	
/* Create page n from its file (a placeholder is left empty until
   TloadReloadPlaceHolder fills it). numMips 0 asks the backend for a full
   mip chain, 1 for the base level only. */
bool TloadTextureSurf( TLOADHEADER * Tloadheader , int n)
{
	LPTEXTURE lpSrcTexture = NULL;
	release_texture(Tloadheader->lpTexture[n]);
	Tloadheader->lpTexture[n] = NULL;
	if ( !Tloadheader->PlaceHolder[ n ] )
	{
		if( Tloadheader->MipMap[n] )
		{
			FSCreateTexture(
				&lpSrcTexture, &Tloadheader->ImageFile[n][0],
				&Tloadheader->Xsize[n], &Tloadheader->Ysize[n],
				0, &Tloadheader->ColourKey[n]);
		}
		else
		{
			FSCreateTexture(
				&lpSrcTexture, &Tloadheader->ImageFile[n][0],
				&Tloadheader->Xsize[n], &Tloadheader->Ysize[n],
				1, &Tloadheader->ColourKey[n]);
		}
	}
	Tloadheader->lpTexture[n] = lpSrcTexture;
	lpSrcTexture = NULL;

	return true;
}

/* Load the file named in PlaceHolderFile[n] into placeholder page n:
   create the texture the first time, afterwards reload it in place so
   everything that holds lpTexture[n] sees the new image. */
bool TloadReloadPlaceHolder( TLOADHEADER *Tloadheader, int16_t n )
{
	// create only one mipmap level (original texture only)
	int numMips = 1;

	// we only work on place holders
	if( !Tloadheader->PlaceHolderFile[n] || !Tloadheader->PlaceHolderFile[n][0] )
		return false;

	// 0 = let the backend build the mip chain
	if(Tloadheader->MipMap[n])
		numMips = 0;

	// the first load creates the texture; update_texture_from_file only replaces an existing one
	if(!Tloadheader->lpTexture[n])
		FSCreateTexture(
			&Tloadheader->lpTexture[n], &Tloadheader->PlaceHolderFile[n][0],
			&Tloadheader->Xsize[n], &Tloadheader->Ysize[n],
			numMips, &Tloadheader->ColourKey[n]);
	else
		update_texture_from_file(
			Tloadheader->lpTexture[n], &Tloadheader->PlaceHolderFile[n][0],
			&Tloadheader->Xsize[n], &Tloadheader->Ysize[n],
			numMips, &Tloadheader->ColourKey[n]);

	return true;
}

/* Release page n's texture, and a placeholder's file-name buffer. */
void
TloadReleaseTexture(TLOADHEADER * Tloadheader, int n)
{
	release_texture(Tloadheader->lpTexture[n]);
	Tloadheader->lpTexture[n] = NULL;
	if ( Tloadheader->PlaceHolder[ n ] )
	{
		free( Tloadheader->PlaceHolderFile[ n ] );
		Tloadheader->PlaceHolderFile[ n ] = NULL;
	}
}

/* Release every page and reset the table to empty. */
void
ReleaseTloadheader( TLOADHEADER * Tloadheader )
{
    int i;
    for (i = 0; i < Tloadheader->num_texture_files; i++)
    {
        TloadReleaseTexture( Tloadheader , i);
    }
    InitTload( Tloadheader );
}

bool TloadAllTextures(TLOADHEADER * Tloadheader)
{
    int i;

    for (i = 0; i < Tloadheader->num_texture_files; i++)
	{
        if(!TloadTextureSurf( Tloadheader, i ))
		{
			goto exit_with_error;
		}
	}

    return true;

exit_with_error:
    for ( ; i < Tloadheader->num_texture_files; i++) {
        TloadReleaseTexture( Tloadheader , i);
    }
    return false;
}

/*===================================================================
	Procedure	:		Find a texture from a tloadheader Ignores the Path
	Input		:		TLOADHEADER * , char * Name
	Output		:		-1 if no match found otherwise number of texture
===================================================================*/

int16_t	FindTexture( TLOADHEADER * Tloadheader , const char * Name )
{
	int	i;
	const char * pnt1;
	char * pnt2;

	pnt1 = strrchr( Name, '\\' );
	if( pnt1 )
	{
		pnt1++;
	}else{
		pnt1 = Name;
	}

	for( i = 0 ; i < Tloadheader->num_texture_files ; i++ )
	{
		pnt2 = strrchr( &Tloadheader->ImageFile[i][0] , '\\' );
		if( pnt2 )
		{
			pnt2++;
		}else{
			pnt2 = &Tloadheader->ImageFile[i][0];
		}

		if( strcasecmp( pnt1, pnt2 ) == 0 )
			return i;

	}
	return -1;
}
/*===================================================================
	Procedure	:		Add a texture to a tloadheader
	Input		:		TLOADHEADER * , char * Name , ColourKey
	Output		:		-1 if too many tpages
===================================================================*/

int16_t	AddTexture( TLOADHEADER * Tloadheader , const char * Name , u_int16_t ColourKey  , bool MipMap, int16_t xsize, int16_t ysize )
{
	int16_t i;
	char * NamePnt;

	// are we just loading placeholder?
	if ( !Name[ 0 ] )
	{
	 	i = Tloadheader->num_texture_files;
		Tloadheader->ColourKey[i] = ColourKey;
		Tloadheader->MipMap[i] = MipMap;
		Tloadheader->ImageFile[i][0] = 0;
		Tloadheader->PlaceHolder[i] = true;
		Tloadheader->num_texture_files++;

		Tloadheader->Xsize[ i ] = xsize;		
		Tloadheader->Ysize[ i ] = ysize;		

		return i;
	}
			
	DebugPrintf("Added Texture: %s\n",Name);

	// has the Texture allready been Added ??
	if( ( i = FindTexture( Tloadheader , Name ) ) != -1 )
		return i;										// found file of same name....

	if( Tloadheader->num_texture_files >= MAXTPAGESPERTLOAD )
	{
		return( -1 );
	}

	i = Tloadheader->num_texture_files;
	Tloadheader->ColourKey[i] = ColourKey;
	Tloadheader->MipMap[i] = MipMap;
	NamePnt = (char *) &Tloadheader->ImageFile[i];
	while( ( *NamePnt++ = *Name++ ) != 0 )
		;
	Tloadheader->PlaceHolder[i] = false;
	Tloadheader->num_texture_files++;
	return i;
}

bool use_original_textures = false;	// UseOriginalTextures in the config

/* Choose the file for a level texture (destination: 256 bytes). The level's
   own textures folder wins, then data\textures\original when
   UseOriginalTextures is set, then the shared data\textures, then
   default.png. The name's extension becomes .PNG; pkups.bmp / pkupsa.bmp
   map to titana.png, as in the original game. */
void GetLevelTexturePath( char * destination, char * file, char * level )
{
	char path[256];
	char new_file[256];

	// pkups pkup[sa] is really titana (old game hard coded it that way)
	if( !strcasecmp( "pkups.bmp", file ) || !strcasecmp( "pkupsa.bmp", file ) )
	{
		strncpy(destination,"data\\textures\\titana.png",256);
		return;
	}

	// convert file extension to new standard
	Change_Ext( file, new_file, ".PNG" );

	// check for level folder override
	snprintf(path,sizeof(path),"data\\levels\\%s\\textures\\%s",level,new_file);
	if( File_Exists( path ) )
	{
		strncpy(destination,path,256);
		return;
	}

	// use old textures if requested and available
	snprintf(path,sizeof(path),"data\\textures\\original\\%s",new_file);
	if( use_original_textures && File_Exists( path ) )
	{
		strncpy(destination,path,256);
		return;
	}

	// default to data/textures
	snprintf(path,sizeof(path),"data\\textures\\%s",new_file);
	if( File_Exists( path ) )
	{
		strncpy(destination,path,256);
		return;
	}

	// we can't find it so set it to default for safety
	strncpy(destination,"data\\textures\\default.png",256);
	DebugPrintf("ERROR: GetTexturePath() could not find %s\n",new_file);
}
