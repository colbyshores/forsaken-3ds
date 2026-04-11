/*
 * platform.h - Cross-platform compatibility layer
 *
 * Provides unified macros for functions that differ
 * between SDL (desktop) and 3DS platforms.
 */

#ifndef PLATFORM_INCLUDED
#define PLATFORM_INCLUDED

#ifdef __3DS__

#include <3ds.h>

extern u_int32_t platform_get_ticks(void);
extern void platform_delay(u_int32_t ms);

#define SDL_GetTicks()     platform_get_ticks()
#define SDL_Delay(ms)      platform_delay(ms)

/* SDL version check - always false on 3DS */
#ifndef SDL_VERSION_ATLEAST
#define SDL_VERSION_ATLEAST(x,y,z) 0
#endif

#else

#include <SDL.h>

#endif /* __3DS__ */

#endif /* PLATFORM_INCLUDED */
