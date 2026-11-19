#pragma once

/* sound.h -- low-level sound backend seam: buffers, sources, listener
 *
 * Implemented by sound_3ds.cpp (ndsp, SFX capped at 8 voices with the
 * oldest evicted) and sound_openal.cpp. sfx.cpp is the only game-side
 * caller. Units keep the 1998 DirectSound conventions: volume in millibels (0 = full,
 * negative attenuates, sound_minimum_volume is the floor and is owned by
 * sfx.cpp), pan in -10000..10000, pitch as a multiplier with 0 meaning
 * as recorded. Sources are 2D: sfx.cpp pans them with sound_pan.
 */

#include "game/main.h"
#ifndef __3DS__
#include <SDL.h>
#endif

//
// Globals
//

// the game appears to need this probably wont in openAL
extern int sound_minimum_volume;

//
// Generic Functions
//

bool sound_init( void );
void sound_destroy( void );

//
// Listener
//

bool sound_listener_position( float x, float y, float z );
bool sound_listener_velocity( float x, float y, float z );
bool sound_listener_orientation(
	float fx, float fy, float fz, // forward vector
	float ux, float uy, float uz  // up vector
);

//
// Buffers
//

typedef struct sound_source_t sound_source_t;
typedef struct sound_buffer_t sound_buffer_t;

sound_buffer_t * sound_load(char* file);
sound_source_t * sound_source( sound_buffer_t * buffer );

void sound_play( sound_source_t * s );
void sound_play_looping( sound_source_t * s );
bool sound_is_playing( sound_source_t * s );
void sound_release_source( sound_source_t * s );
void sound_release_buffer( sound_buffer_t * s );
void sound_set_pitch( sound_source_t * s, float freq );
void sound_volume( sound_source_t * s, long decibels );
void sound_pan( sound_source_t * s, long pan );
