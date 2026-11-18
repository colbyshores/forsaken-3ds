

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
#include "net/networking.h"
#include "data/mload.h"
#include "game/camera.h"

#include "util/util.h"
#include "util/file_reader.h"

constexpr int CAM_VERSION_NUMBER = 1;

/*===================================================================
		Externals ...
===================================================================*/

/*===================================================================
		Globals ...
===================================================================*/

CAMERA	CurrentCamera;

CAMERA	MainCamera;			// the main viewing screen...



int32_t	NumOfCams = 0;
REMOTECAMERA * ActiveRemoteCamera = NULL;
REMOTECAMERA * RemoteCameras = NULL;


/*===================================================================
	Procedure	:		Load .Cam File
	Input		:		char	*	Filename 
	Output		:		Nothing
===================================================================*/
bool Cameraload( char * Filename )
{
	char		*	Buffer;
	char		*	OrgBuffer;
	int			e;
	long			File_Size;
	long			Read_Size;
	REMOTECAMERA	*	CamPnt;
	u_int32_t		MagicNumber;
	u_int32_t		VersionNumber;
	VECTOR	Tempv;

	ActiveRemoteCamera = NULL;
	RemoteCameras = NULL;


	File_Size = Get_File_Size( Filename );	

	if( !File_Size )
	{
		// Doesnt Matter.....
		return true;
	}
	Buffer = (char *)calloc( 1, File_Size );
	OrgBuffer = Buffer;
	if( !Buffer )
	{
		Msg( "Camload failed to Allocate buffer for %s failed\n", Filename );
		return false;
	}
	Read_Size = Read_File( Filename, Buffer, File_Size );
	if( Read_Size != File_Size )
	{
		Msg( "Camload Error reading file %s\n", Filename );
		return false;
	}
	FileReader in( Buffer );
	MagicNumber = in.u32();
	VersionNumber = in.u32();

	if( ( MagicNumber != MAGIC_NUMBER ) || ( VersionNumber != CAM_VERSION_NUMBER  ) )
	{
		Msg( "CamLoad() Incompatible .CAM file %s", Filename );
		return( false );
	}

	NumOfCams = in.i32();
 	RemoteCameras = (REMOTECAMERA*) calloc( NumOfCams, sizeof(REMOTECAMERA) );
 	CamPnt = RemoteCameras;
	if( !CamPnt )
	{
		Msg( "Camload failed allocate Cam Pointer in %s \n", Filename );
		return false;
	}

	for( e = 0 ; e < NumOfCams ; e++ )
	{
		CamPnt->enable = false;

		CamPnt->Group = in.i16();
		CamPnt->Pos = in.vec();
		CamPnt->Dir = in.vec();
		CamPnt->Up = in.vec();
		Tempv.x = CamPnt->Pos.x + CamPnt->Dir.x;
		Tempv.y = CamPnt->Pos.y + CamPnt->Dir.y;
		Tempv.z = CamPnt->Pos.z + CamPnt->Dir.z;
		MakeViewMatrix( &CamPnt->Pos, &Tempv, &CamPnt->Up, &CamPnt->Mat);
		MatrixTranspose( &CamPnt->Mat, &CamPnt->InvMat );
		CamPnt++;
	}
	// All Cameras Networks have been loaded...
	free(OrgBuffer);
	return true;
}

/*===================================================================
	Procedure	:		Release NodeHeader..
	Input		:		NOTHING
	Output		:		Nothing
===================================================================*/
void CameraRelease( void)
{
	if( RemoteCameras )
	{
		free( RemoteCameras );
		ActiveRemoteCamera = NULL;
		RemoteCameras = NULL;
	}
	NumOfCams = 0;
}

/*===================================================================
 	Procedure	:		Enable Remote Camera
	Input		:		u_int16_t	* Data
	Output		:		Nothing
===================================================================*/
void EnableRemoteCamera( u_int16_t * Data )
{
	REMOTECAMERA * CamPnt;

	CamPnt = RemoteCameras;
	if( !CamPnt )
		return;
	CamPnt += *Data;
	CamPnt->enable = true;
	ActiveRemoteCamera = CamPnt;
}
/*===================================================================
 	Procedure	:		Disable Remote Camera
	Input		:		u_int16_t	* Data
	Output		:		Nothing
===================================================================*/
void DisableRemoteCamera( u_int16_t * Data )
{
	REMOTECAMERA * CamPnt;

	CamPnt = RemoteCameras;
	if( !CamPnt )
		return;
	CamPnt += *Data;
	CamPnt->enable = false;
	if( ActiveRemoteCamera == CamPnt )
		ActiveRemoteCamera = NULL;
}

/*===================================================================
	Procedure	:	Save RemoteCameras arrays & Connected Global Variables
	Input		:	FILE	*	File Pointer
	Output		:	FILE	*	Updated File Pointer
===================================================================*/
FILE * SaveRemoteCameras( FILE * fp )
{
	u_int16_t	TempIndex;

	if( fp )
	{
		if( ActiveRemoteCamera ) TempIndex = (u_int16_t) ( ActiveRemoteCamera - RemoteCameras );
		else TempIndex = (u_int16_t) -1;
		fwrite( &TempIndex, sizeof( u_int16_t ), 1, fp );
	}

	return( fp );
}

/*===================================================================
	Procedure	:	Load RemoteCameras Arrays & Connected Global Variables
	Input		:	FILE	*	File Pointer
	Output		:	FILE	*	Updated File Pointer
===================================================================*/
FILE * LoadRemoteCameras( FILE * fp )
{
	u_int16_t	i;
	u_int16_t	TempIndex;
	REMOTECAMERA	*	CamPnt;

 	CamPnt = RemoteCameras;

	for( i = 0; i < NumOfCams ; i++ )
	{
		CamPnt->enable = false;
		CamPnt++;
	}

	if( fp )
	{
		fread( &TempIndex, sizeof( u_int16_t ), 1, fp );
		if( TempIndex != (u_int16_t) -1 )
		{
			ActiveRemoteCamera = ( RemoteCameras + TempIndex );
			ActiveRemoteCamera->enable = true;
		}
		else
		{
			ActiveRemoteCamera = NULL;
		}
	}

	return( fp );
}


