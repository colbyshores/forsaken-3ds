
#include <stdio.h>

#include "game/main.h"
#include "net/net.h"
#include "rendering/render.h"
#include "ui/title.h"

#include "math/new3d.h"
#include "math/quat.h"
#include "game/compobjects.h"
#include "game/bgobjects.h"
#include "game/object.h"
#include "net/networking.h"
#include "net/multiplayer.h"
#include "data/mxload.h"
#include "data/mxaload.h"
#include <stdio.h>
#include "ui/local.h"
#include "rendering/text.h"
#include "game/ships.h"
#include "data/loadsave.h"
#include "game/pickups.h"
#include "game/primary.h"
#include "input/controls.h"
#include "util/util.h"
#include "game/demo.h"
#include "data/file.h"
#include "game/oct2.h"
#include "rendering/screenpolys.h"

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
