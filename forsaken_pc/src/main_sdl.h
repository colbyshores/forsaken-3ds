#pragma once

/* main_sdl.h -- SDL start-up for the Linux build
 *
 * sdl_init initialises the hunk allocator and SDL video/joystick; sdl_init_video
 * creates the window and calls render_init. Both are called once from main.cpp.
 */

#include <SDL.h>
#include "rendering/render.h"

bool sdl_init_video( void );
bool sdl_init( void );
