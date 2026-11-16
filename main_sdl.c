#include "main.h"
#include "util.h"
#include "version.h"
#include "main_sdl.h"
#include <SDL.h>

extern render_info_t render_info;
extern bool render_init( render_info_t * info );

bool sdl_init( void )
{
	SDL_version ver;

	SDL_VERSION(&ver);
	DebugPrintf("SDL compile-time version: %u.%u.%u\n", ver.major, ver.minor, ver.patch);

	SDL_GetVersion(&ver);
	DebugPrintf("SDL runtime version: %u.%u.%u\n", ver.major, ver.minor, ver.patch);

	/* Shared two-tag hunk allocator (model textureGroups etc).
	 * The 3DS platform layer initialises this in platform_init;
	 * Linux needs the same call or every Hunk_Alloc returns NULL
	 * and model loading fails. Same 8 MB size as 3DS. */
	{
		extern bool Hunk_Init(size_t);
		if (!Hunk_Init(8 * 1024 * 1024))
		{
			Msg("Failed to initialise hunk allocator\n");
			return false;
		}
	}

	if( SDL_Init( SDL_INIT_VIDEO | SDL_INIT_JOYSTICK ) < 0 )
	{
		Msg("Failed to initialize sdl: %s\n",SDL_GetError());
		return false;
	}

	return true;
}

/////////////////////////
//
// Video Initialization
//
/////////////////////////

/*
	You can't trust ListModes.
	Modes are standard and a list is updated at:
		http://en.wikipedia.org/wiki/Display_resolution.
	Default mode should match the current desktop.
	BPP should be left alone to match whatever the desktop is at.
*/

#define NUMBER_MODES 19
render_display_mode_t video_modes[NUMBER_MODES] = {
{0,0}, // current video mode of the desktop
{640,480},
{800,600}, // default window mode
{1024,768},
{1152,864},
{1280,600},
{1280,720},
{1280,768},
{1280,800},
{1280,854},
{1280,960},
{1280,1024},
{1366,768},
{1440,900},
{1600,900},
{1600,1200},
{1680,1050},
{1920,1080},
{1920,1200}
};

static void init_video_modes( u_int32_t window_flags )
{
	int i;
	render_info.Mode               = video_modes;
	render_info.NumModes           = NUMBER_MODES;

	// in full screen then default to desktop resolution
	if( window_flags & SDL_WINDOW_FULLSCREEN ) render_info.CurrMode = 0;

	// in window mode default to 800x600
	else render_info.CurrMode = 2;

	// try to find the users preferred mode
	for( i = 1; i < render_info.NumModes; i++ ) // skip mode 0
	{
		if( render_info.Mode[i].w == render_info.default_mode.w &&
		    render_info.Mode[i].h == render_info.default_mode.h )
		{
			render_info.CurrMode = i;
			DebugPrintf("init_video_modes: found requested resolution of %dx%d\n",
				render_info.default_mode.w,
				render_info.default_mode.h);
			break;
		}
	}

	render_info.ThisMode           = render_info.Mode[ render_info.CurrMode ];
	render_info.window_size.cx     = render_info.ThisMode.w;
	render_info.window_size.cy     = render_info.ThisMode.h;
	render_info.WindowsDisplay     = render_info.Mode[ render_info.CurrMode ];
	render_info.WindowsDisplay.w   = render_info.ThisMode.w;
	render_info.WindowsDisplay.h   = render_info.ThisMode.h;
}

static void set_window_icon( void )
{
	SDL_Surface* icon = SDL_LoadBMP("Data/ProjectX-32x32.bmp");
	if(icon)
	{
		SDL_SetWindowIcon(render_info.window, icon);
		SDL_FreeSurface(icon);
	}
}

static void print_info( void )
{
	DebugPrintf("main_sdl: video driver name: %s\n", SDL_GetCurrentVideoDriver());
}

static u_int32_t create_window_flags( void )
{
	u_int32_t window_flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;

	if(render_info.fullscreen)
		window_flags |= SDL_WINDOW_FULLSCREEN;

	window_flags |= SDL_WINDOW_VULKAN;

	return window_flags;
}

static bool create_video_surface( u_int32_t window_flags )
{
#ifndef RENDER_DISABLED
	render_info.window = SDL_CreateWindow(
		"ProjectX",
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		render_info.ThisMode.w,
		render_info.ThisMode.h,
		window_flags
		);
	if(!render_info.window)
	{
		Msg("main_sdl: failed to create window: %s\n",SDL_GetError());
		return false;
	}
	/* Vulkan surface/device creation lives in render_vulkan.c
	 * (render_init), called by sdl_init_video below. */

	print_info();
#endif

	return true;
}

static void set_window_title( void )
{
	if (render_info.window)
		SDL_SetWindowTitle(render_info.window, PXVersion);
}

// TODO - we need a ui control for this
static void set_aspect_ratio( void )
{
	render_info.aspect_ratio = (float) render_info.ThisMode.w / (float) render_info.ThisMode.h;
	DebugPrintf("aspect ratio: %d:%d\n",render_info.ThisMode.w,render_info.ThisMode.h);
}

bool sdl_init_video( void )
{
	u_int32_t window_flags   = create_window_flags();

	init_video_modes( window_flags );

	set_aspect_ratio();

	if(!create_video_surface( window_flags ))
		return false;

	set_window_icon();
	set_window_title();

	if (!render_init( &render_info ))
	{
		Msg("render_init() returned false");
		return false;
	}

	return true;
}
