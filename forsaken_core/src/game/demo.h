#pragma once

/* demo.h -- demo file naming and the recording handle
 *
 * A demo is a recorded multiplayer session: multiplayer.cpp opens DemoFp
 * and networking.cpp appends timestamped network messages to it.
 * DemoFileName maps a demo name to Demos/<NAME>.DMO in a static buffer
 * that the next call overwrites; MAX_DEMOFILENAME_LENGTH must cover the
 * folder, separator, name and extension.
 */

#include "game/main.h"

constexpr int MAX_DEMONAME_LENGTH = (60);
constexpr int MAX_DEMOFILENAME_LENGTH = (80); // must be at least MAX_DEMONAME_LENGTH + strlen( DEMOFOLDER ) + strlen( DEMOFILE_EXTENSION ) + 1

#define DEMOFOLDER				"Demos"
#define DEMOFILE_EXTENSION		".DMO"
#define DEMOFILE_SEARCHPATH		DEMOFOLDER "\\*" DEMOFILE_EXTENSION


/* Defined in demo.cpp; shared with the modules that use it. */
extern FILE * DemoFp;
