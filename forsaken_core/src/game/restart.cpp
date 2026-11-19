/*===================================================================
	Includes
===================================================================*/
#include "game/main.h"
#include <stdio.h>

#include "math/new3d.h"
#include "math/quat.h"
#include "game/compobjects.h"
#include "game/bgobjects.h"
#include "game/object.h"
#include "net/networking.h"
#include "util/util.h"
#include <math.h>
#include "data/mload.h"
#include "game/models.h"

#include "rendering/2dpolys.h"
#include "game/primary.h"
#include "game/restart.h"
#include "game/spotfx.h"
#include "game/oct2.h"
#include "rendering/2dtextures.h"

/*===================================================================
	Defines
===================================================================*/
constexpr int RESTART_VERSION_NUMBER = 1;

/*===================================================================
	Global Variables
===================================================================*/
bool		ShowStartPoints = false;

/*===================================================================
	Procedure	:	Load StartPoints
	Input		:	Nothing
	Output		:	bool	True/False
===================================================================*/
bool LoadStartPoints( void )
{
	FILE	*	fp;
	int16_t		Count;
	const char	*	NewExt = ".STP";
	char		NewFilename[ 256 ];
	u_int32_t		MagicNumber;
	u_int32_t		VersionNumber;
	int16_t		OllyPad;

	Change_Ext( &LevelNames[ LevelNum ][ 0 ], &NewFilename[ 0 ], NewExt );


	fp = file_open( &NewFilename[ 0 ], "rb" );

	if( fp != nullptr )
	{
		fread( &MagicNumber, sizeof( u_int32_t ), 1, fp );
		fread( &VersionNumber, sizeof( u_int32_t ), 1, fp );

		if( ( MagicNumber != MAGIC_NUMBER ) || ( VersionNumber != RESTART_VERSION_NUMBER  ) )
		{
			fclose( fp );
			Msg( "LoadStartPoints() Incompatible start point file %s", &NewFilename[ 0 ] );
			return( false );
		}

		for( Count = 0 ; Count < MAXGROUPS ; Count++ )
		{
			FirstStartPositionInGroup[Count] = INDEX_NONE;
		}


		fread( &num_start_positions, sizeof( int16_t ), 1, fp );

		for( Count = 0; Count < num_start_positions; Count++ )
		{
			fread( &StartPositions[ Count ].Pos.x, sizeof( float ), 1, fp );
			fread( &StartPositions[ Count ].Pos.y, sizeof( float ), 1, fp );
			fread( &StartPositions[ Count ].Pos.z, sizeof( float ), 1, fp );
			fread( &StartPositions[ Count ].Dir.x, sizeof( float ), 1, fp );
			fread( &StartPositions[ Count ].Dir.y, sizeof( float ), 1, fp );
			fread( &StartPositions[ Count ].Dir.z, sizeof( float ), 1, fp );
			fread( &StartPositions[ Count ].Up.x, sizeof( float ), 1, fp );
			fread( &StartPositions[ Count ].Up.y, sizeof( float ), 1, fp );
			fread( &StartPositions[ Count ].Up.z, sizeof( float ), 1, fp );
			fread( &StartPositions[ Count ].Group, sizeof( u_int16_t ), 1, fp );
			fread( &OllyPad, sizeof( int16_t ), 1, fp );
			Mloadheader.Group[ StartPositions[ Count ].Group ].StartPosInThisGroup = Count;

			if( FirstStartPositionInGroup[StartPositions[Count].Group] == INDEX_NONE )
			{
				StartPositions[Count].NextInGroup = INDEX_NONE;
				FirstStartPositionInGroup[StartPositions[Count].Group] = Count;
			}else{
				StartPositions[Count].NextInGroup = FirstStartPositionInGroup[StartPositions[Count].Group];
				FirstStartPositionInGroup[StartPositions[Count].Group] = Count;
			}

		}

		fclose( fp );
	}

	return( true );
}

/*===================================================================
	Procedure	:	Save RestartPoint arrays & Connected Global Variables
	Input		:	FILE	*	File Pointer
	Output		:	FILE	*	Updated File Pointer
===================================================================*/
FILE * SaveStartRestartPoints( FILE * fp )
{
	if( fp )
	{
		fwrite( &last_start_position, sizeof( u_int16_t ), 1, fp );
	}

	return( fp );
}

/*===================================================================
	Procedure	:	Load RestartPoint Arrays & Connected Global Variables
	Input		:	FILE	*	File Pointer
	Output		:	FILE	*	Updated File Pointer
===================================================================*/
FILE * LoadStartRestartPoints( FILE * fp )
{
	if( fp )
	{
		fread( &last_start_position, sizeof( u_int16_t ), 1, fp );
	}

	return( fp );
}

/*===================================================================
	Procedure	:	Display StartPoints
	Input		:	u_int16_t Group
	Output		:	Nothing
===================================================================*/
void DisplayStartPoints( void )
{
	int16_t	Count;
	VECTOR	TopLeft;
	VECTOR	BottomRight;

	for( Count = 0; Count < num_start_positions; Count++ )
	{
		TopLeft.x = ( StartPositions[ Count ].Pos.x - ( SHIP_RADIUS ) );
		TopLeft.y = ( StartPositions[ Count ].Pos.y - ( SHIP_RADIUS ) );
		TopLeft.z = ( StartPositions[ Count ].Pos.z - ( SHIP_RADIUS ) );
		BottomRight.x = ( StartPositions[ Count ].Pos.x + ( SHIP_RADIUS ) );
		BottomRight.y = ( StartPositions[ Count ].Pos.y + ( SHIP_RADIUS ) );
		BottomRight.z = ( StartPositions[ Count ].Pos.z + ( SHIP_RADIUS ) );
		CreateBoundingBox( &TopLeft, &BottomRight, StartPositions[ Count ].Group );
	}
}
