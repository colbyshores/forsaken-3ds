#ifndef DEMO_INCLUDED
#define DEMO_INCLUDED
#include "main.h"
#ifdef __cplusplus
extern "C" {
#endif

#define MAX_DEMONAME_LENGTH		(60)
#define MAX_DEMOFILENAME_LENGTH	(80) // must be at least MAX_DEMONAME_LENGTH + strlen( DEMOFOLDER ) + strlen( DEMOFILE_EXTENSION ) + 1

#define DEMOFOLDER				"Demos"
#define DEMOFILE_EXTENSION		".DMO"
#define DEMOFILE_SEARCHPATH		DEMOFOLDER"\\*"DEMOFILE_EXTENSION

char *DemoFileName( char *demoname );
char *DemoName( char *demofilename );

void StartDemoCleaning( MENUITEM * Item );
void StartDemoPlayback( MENUITEM * Item );

#ifdef __cplusplus
}
#endif
#endif
