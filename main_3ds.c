/*
 * main_3ds.c - Nintendo 3DS platform layer replacing main_sdl.c
 *
 * Initializes the 3DS hardware (gfx, HID, romfs),
 * sets up picaGL on the top screen, and provides
 * the platform_*() functions consumed by main.c.
 */

#ifdef __3DS__

#include <3ds.h>
#include <GL/picaGL.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "main.h"
#include "main_3ds.h"
#include "util.h"
#include "render.h"

extern render_info_t render_info;
extern bool render_init(render_info_t *info);

/* ---- 3DS heap configuration ---- */
/* [3DS] Heap reduced from 80 MB to 64 MB to avoid OOM during level transitions.
 *
 * The level-load path allocates two full copies of MODELHEADERS[512] and
 * MXAMODELHEADERS[512] simultaneously (old level still resident while the new
 * level is loading), plus texture upload staging buffers.  At 80 MB the
 * combined allocation exceeded available RAM on some levels.
 *
 * New 3DS has ~124 MB of application RAM.  With a 64 MB heap the remaining
 * ~60 MB covers the OS, linear (GPU-accessible) heap, and thread stacks.
 * BSS ≈ 18 MB + code ≈ 4 MB + heap 64 MB + stack ≈ 1 MB ≈ 87 MB total. */
u32 __ctru_heap_size = 64 * 1024 * 1024;

/* ---- init state tracking ---- */

static bool _video_initialized = false;

/* ---- tick counter ---- */

static u_int64_t _ticks_base = 0;

u_int32_t platform_get_ticks(void)
{
	u_int64_t now = svcGetSystemTick();
	/* 268 MHz sysclock -> ms */
	return (u_int32_t)((now - _ticks_base) * 1000ULL / SYSCLOCK_ARM11);
}

void platform_delay(u_int32_t ms)
{
	svcSleepThread((u_int64_t)ms * 1000000ULL);
}

/* ---- debug trace (writes to sdmc so Mandarine can show it) ---- */

#ifdef __3DS_DEBUG__
/* Tracing: use low-level write() to bypass stdio buffering.
   Opens once when trace_enable() is called. */
#include <fcntl.h>
static int _trace_fd = -1;
static int _trace_enabled = 0;

void trace_enable(void) {
	_trace_enabled = 1;
	if (_trace_fd < 0) {
		_trace_fd = open("sdmc:/forsaken_trace.txt", O_WRONLY|O_CREAT|O_TRUNC, 0666);
	}
}

void trace(const char *msg)
{
	svcOutputDebugString(msg, strlen(msg));
	if (_trace_enabled && _trace_fd >= 0) {
		int len = strlen(msg);
		write(_trace_fd, msg, len);
		write(_trace_fd, "\n", 1);
		fsync(_trace_fd);
	}
}

void trace_dump(void) {
	if (_trace_fd >= 0) { close(_trace_fd); _trace_fd = -1; }
}
#else
void trace_enable(void) {}
void trace(const char *msg) { (void)msg; }
void trace_dump(void) {}
#endif

/* ---- platform init ---- */

bool platform_init(void)
{
	_ticks_base = svcGetSystemTick();

	/* Ensure trace ring is dumped on any exit */
	atexit(trace_dump);

	trace("platform_init: start");

	/* Enable New3DS 804 MHz mode when available */
	bool is_n3ds = false;
	APT_CheckNew3DS(&is_n3ds);
	if (is_n3ds)
		osSetSpeedupEnable(true);

	trace("platform_init: romfsInit");

	/* Initialize romfs - game data is embedded in the 3DSX */
	romfsInit();

	/* Set working directory so relative paths (Data/, Configs/) resolve */
	chdir("romfs:/");

	/* Create save game directory on SD card (romfs is read-only) */
	mkdir("sdmc:/3ds", 0777);
	mkdir("sdmc:/3ds/forsaken", 0777);
	mkdir("sdmc:/3ds/forsaken/savegame", 0777);

	trace("platform_init: done");

	return true;
}

/* ---- video / picaGL init ---- */

/* 3DS top screen: 400x240 (5:3 landscape).
 * Same aspect ratio as the Open Pandora (800x480) which runs Forsaken
 * natively at 5:3 without any letterboxing.  Render at the full native
 * resolution for maximum screen utilisation. */
#define SCREEN_WIDTH        400
#define SCREEN_HEIGHT       240

bool platform_init_video(void)
{
	trace("platform_init_video: gfxInitDefault");

	/* Initialize 3DS graphics service (GSP) - required before picaGL */
	gfxInitDefault();
	/* [3DS] Hardware stereoscopic 3D starts disabled; the 3D slider logic in
	 * MainGameRender / DisplayTitle calls gfxSet3D(true/false) each frame based
	 * on osGet3DSliderState().  Initialise to false so the first frame is always
	 * mono regardless of slider position at boot. */
	gfxSet3D(false);

	trace("platform_init_video: pglInit");

	/* Initialize citro3d-backed picaGL */
	pglInit();

	_video_initialized = true;
	trace("platform_init_video: pglInit done");

	render_info.ThisMode.w = SCREEN_WIDTH;
	render_info.ThisMode.h = SCREEN_HEIGHT;
	render_info.window_size.cx = SCREEN_WIDTH;
	render_info.window_size.cy = SCREEN_HEIGHT;
	render_info.WindowsDisplay.w = SCREEN_WIDTH;
	render_info.WindowsDisplay.h = SCREEN_HEIGHT;
	render_info.NumModes = 1;
	render_info.CurrMode = 0;
	render_info.fullscreen = true;

	/* Standard landscape aspect ratio. picaGL's matrix4x4_fix_projection
	 * applies a proper -90° rotation ([[0,1],[-1,0]], det=+1) to the
	 * projection, which rotates axes without swapping the _11/_22 scales.
	 * An earlier hack inverted this to H/W assuming picaGL was doing a
	 * reflection that swapped the scales; with the proper rotation in
	 * place that hack over-compensates and distorts vfov. */
	render_info.aspect_ratio = (float)SCREEN_WIDTH / (float)SCREEN_HEIGHT;

	DebugPrintf("platform_init_video: picaGL context created %dx%d\n",
		SCREEN_WIDTH, SCREEN_HEIGHT);

	if (!render_init(&render_info))
	{
		DebugPrintf("platform_init_video: render_init failed\n");
		return false;
	}

	return true;
}

/* ---- 3D slider with emulator override ---- */

float platform_get_3d_slider(void)
{
#ifndef RENDERER_C3D
	/* picaGL: stereo not supported (no display list replay = half framerate).
	 * Always return 0 to keep stereo disabled. */
	return 0.0f;
#else
	extern float config_get_float(const char *opt, float _default);
	/* Check for a test override first (useful when testing in Mandarine/Citra
	 * which may return 0 from osGet3DSliderState).
	 * Set   stereo_test_slider = 0.5   in Configs/debug.txt.
	 * A negative value means "use the real hardware slider". */
	float override = config_get_float("stereo_test_slider", -1.0f);
	if (override >= 0.0f)
		return override > 1.0f ? 1.0f : override;
	float v = osGet3DSliderState();
	if (v < 0.0f) v = 0.0f;
	if (v > 1.0f) v = 1.0f;
	return v;
#endif
}

/* ---- frame present ---- */

void platform_render_present(render_info_t *info)
{
	(void)info;
	pglSwapBuffers();
}

/* ---- shutdown ---- */

void platform_shutdown(void)
{
	trace("platform_shutdown: dumping trace ring");
	trace_dump();
	if (_video_initialized)
	{
		pglExit();
		gfxExit();
		_video_initialized = false;
	}
	romfsExit();
}

#endif /* __3DS__ */
