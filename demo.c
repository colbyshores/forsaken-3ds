
#include <stdio.h>

#include "main.h"
#include "net.h"
#include "render.h"
#include "title.h"

#include "new3d.h"
#include "quat.h"
#include "compobjects.h"
#include "bgobjects.h"
#include "object.h"
#include "networking.h"
#include "multiplayer.h"
#include "mxload.h"
#include "mxaload.h"
#include <stdio.h>
#include "local.h"
#include "text.h"
#include "ships.h"
#include "loadsave.h"
#include "pickups.h"
#include "primary.h"
#include "controls.h"
#include "util.h"
#include "demo.h"
#include "file.h"


extern BYTE TeamNumber[MAX_PLAYERS];
extern LIST DemoList;
extern bool TeamGame;
extern bool CTF;
extern bool CaptureTheFlag;
extern bool BountyHunt;
extern u_int16_t RandomStartPosModify;
extern int CameraStatus;  
extern bool CountDownOn;
extern bool DemoShipInit[];
extern u_int16_t CopyOfSeed1;
extern u_int16_t CopyOfSeed2;
extern bool RandomPickups;
extern bool PlayDemo;
extern void DebugLastError( void );
extern bool ChangeLevel( void );

FILE	*	DemoFp = NULL;
FILE	*	DemoFpClean = NULL;

char *DemoFileName( char *demoname )
{
	static char filename[ MAX_DEMOFILENAME_LENGTH  ];
	char *dmo;

	folder_exists( DEMOFOLDER );
	strcpy( filename, DEMOFOLDER );
	strncat( filename, "\\", MAX_DEMONAME_LENGTH );
	strncat( filename, demoname, MAX_DEMONAME_LENGTH );
	filename[ strlen( DEMOFOLDER ) + 1 + MAX_DEMONAME_LENGTH ] = 0;
	strtoupper( filename );
	dmo = strstr( filename, DEMOFILE_EXTENSION );
	if ( !dmo )
		strcat( filename, DEMOFILE_EXTENSION );

	return filename;
}

char *DemoName( char *demofilename )
{
	static char demoname[ MAX_DEMONAME_LENGTH + 4 ];
	char *dmo;
	int offset;

	offset = strstr( demofilename, DEMOFOLDER ) ? strlen( DEMOFOLDER ) + 1 : 0;
	strncpy( demoname, demofilename + offset, sizeof( demoname ) - 1 );
	demoname[ sizeof( demoname ) - 1 ] = 0;
	strtoupper( demoname );
	dmo = strstr( demoname, DEMOFILE_EXTENSION );
	if ( dmo )
		*dmo = 0;

	return demoname;
}

void StartDemoCleaning( MENUITEM * Item )
{
}

void StartDemoPlayback( MENUITEM * Item )
{
}
