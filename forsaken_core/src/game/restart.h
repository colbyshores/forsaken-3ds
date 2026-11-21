#pragma once

/* restart.h -- level start points
 *
 * LoadStartPoints reads the level's .STP file into StartPositions[];
 * DisplayStartPoints draws their bounding boxes when ShowStartPoints is
 * set; Save/LoadStartRestartPoints carry last_start_position in saved
 * games. (The animated restart-point objects this module once managed
 * were disabled in the original source and have been removed.)
 */

/*===================================================================
	Prototypes
===================================================================*/
bool LoadStartPoints( void );
FILE * SaveStartRestartPoints( FILE * fp );
FILE * LoadStartRestartPoints( FILE * fp );
void DisplayStartPoints( void );

/* Defined in restart.cpp; shared with the modules that use it. */
