#pragma once

/* platform.h -- SDL timing names and the 3DS services core code calls
 *
 * On PC this only includes <SDL.h>. On 3DS it includes <3ds.h>, maps
 * SDL_GetTicks / SDL_Delay onto platform_get_ticks / platform_delay
 * (main_3ds.cpp: milliseconds from the ARM11 system tick, svcSleepThread),
 * defines SDL_VERSION_ATLEAST as 0 (unless input.h already has) so SDL 2
 * branches compile out, and
 * declares the 3DS services that core code calls directly: the 3D slider
 * (main_3ds.cpp), the button-remap overlay (controls_3ds.cpp) and the
 * SDL_EnableUNICODE stub (stubs_3ds.cpp). Those declarations exist only
 * under __3DS__, so callers in core must be guarded the same way.
 */


#ifdef __3DS__

#include <3ds.h>

extern u_int32_t platform_get_ticks(void);
extern void platform_delay(u_int32_t ms);

/* 3DS platform services used by the game (main_3ds.cpp, controls_3ds.cpp, stubs_3ds.cpp). */
float platform_get_3d_slider(void);		/* 0..1, 0 = mono */
void controls_3ds_remap_menu(void);		/* open the button-remap overlay */
bool controls_3ds_menu_is_open(void);
void controls_3ds_render_overlay(void);	/* draw the remap overlay (bottom screen) */
int SDL_EnableUNICODE(int enable);		/* SDL 1.x call, stubbed on 3DS */

#define SDL_GetTicks()     platform_get_ticks()
#define SDL_Delay(ms)      platform_delay(ms)

/* SDL version check - always false on 3DS */
#ifndef SDL_VERSION_ATLEAST
#define SDL_VERSION_ATLEAST(x,y,z) 0
#endif

#else

#include <SDL.h>

#endif /* __3DS__ */
