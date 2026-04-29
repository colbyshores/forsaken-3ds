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
/* ---- heap sizing ----
 *
 * OG 3DS in HIMEM mode has 96 MB app RAM (forsaken.rsf:SystemMode=96MB,
 * SpecialMemoryArrange=true; suspends Miiverse/Browser to free ~32 MB
 * over the default 64 MB). New has 124 MB. Both allocations must fit
 * inside the smaller budget for a single binary to work on both.
 *
 * Rough layout on OG (HIMEM):
 *   code            ~4 MB
 *   BSS             ~32 MB   (level data, model data, static arrays;
 *                             dominated by ModelHeaders[608] +
 *                             MxaModelHeaders[608] at ~9.6 KB/entry each)
 *   malloc heap     24 MB    (this variable)
 *   linear heap     24 MB    (GPU-accessible; textures, scratch, cmd buf)
 *   stack           ~1 MB
 *   OS reserve      ~8 MB    (services, IPC, filesystem buffers)
 *   ---
 *   total           ~93 MB   (fits 96 MB with ~3 MB margin)
 *
 * Rationale for 24/24 vs the old 64/default-32:
 *   * 64 MB malloc was wildly over what the engine needs (Lua + small
 *     runtime allocations); it starved the linear heap.
 *   * 24 MB linear needs to hold: post-strip HD texture set
 *     (~30 walls × ~45 KB = ~1.4 MB), 4 MB GPU scratch, 1 MB command
 *     buffer, PNG fallback textures (up to ~10 MB worst case for
 *     several big bigexp*.png at 1024² RGBA4), audio buffers (~1 MB),
 *     misc linear allocations. Total ~18 MB peak, 6 MB margin.
 *
 * On New 3DS this leaves ~30 MB unused overhead but that's fine —
 * we're not memory-optimizing for New. */
u32 __ctru_heap_size        = 24 * 1024 * 1024;
u32 __ctru_linear_heap_size = 24 * 1024 * 1024;

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

/* Tracing: use low-level write() to bypass stdio buffering. Compiles
 * unconditionally — the production EDITION=remaster build doesn't
 * define __3DS_DEBUG__ but still needs the post-crash trace to survive
 * on SD for diagnosis. Cost when nothing's broken is ~200 bytes of
 * writes per level load (one open, a handful of writes + one fsync per
 * checkpoint). Negligible.
 *
 * Truncation policy: the file is truncated EXACTLY ONCE per process
 * lifetime, on the very first call to either trace_enable() or trace().
 * Subsequent re-opens (e.g., after a trace_dump() close) use O_APPEND so
 * we don't lose what was written before the close. Without this, the
 * autotest's "DONE -> trace_dump() -> chain-load" path was wiping the
 * whole trace because platform_shutdown's later trace() call would
 * re-open with O_TRUNC. */
#include <fcntl.h>
static int  _trace_fd = -1;
static int  _trace_enabled = 0;
static int  _trace_truncated_once = 0;

static void _trace_open(void) {
	int flags = O_WRONLY | O_CREAT |
	            (_trace_truncated_once ? O_APPEND : O_TRUNC);
	_trace_fd = open("sdmc:/forsaken_trace.txt", flags, 0666);
	_trace_truncated_once = 1;
}

void trace_enable(void) {
	_trace_enabled = 1;
	if (_trace_fd < 0) _trace_open();
}

void trace(const char *msg)
{
	/* Auto-open the trace file on first call even if trace_enable()
	 * hasn't been called yet, so unsolicited trace points (e.g. a
	 * checkpoint deep inside the loader) still survive a crash. */
	if (_trace_fd < 0) {
		_trace_open();
		_trace_enabled = 1;
	}
	svcOutputDebugString(msg, strlen(msg));
	if (_trace_enabled && _trace_fd >= 0) {
		int len = strlen(msg);
		write(_trace_fd, msg, len);
		write(_trace_fd, "\n", 1);
		fsync(_trace_fd);
	}
}

void trace_dump(void) {
	/* Flush only — don't close. Re-opens with O_APPEND would still
	 * preserve content per the truncated-once policy, but keeping the
	 * fd open avoids the open()/close() cost on chain-load paths that
	 * call trace() again immediately afterward. */
	if (_trace_fd >= 0) fsync(_trace_fd);
}

/* Runtime flag — when true, DrawSimplePanel prints raw 3D-slider value,
 * stereo_eye_sep, and the on/off state so we can diagnose binary-seeming
 * slider behavior. Config key: ShowStereoDebug. Default off. */
bool g_show_stereo_debug = false;

/* ---- unaligned memcpy for ARM -----------------------------------------
 *
 * GCC compiles a constant-size `memcpy(a, b, N)` into `ldm/stm` when it
 * infers either pointer is 4-byte aligned from its declared type. On ARM11
 * the 3DS enables unaligned access for single ldr/ldrh, but `ldm` *always*
 * requires 4-byte alignment — a misaligned `ldm` produces a Data Abort
 * (Alignment). For load-time parsers that read struct data out of a raw
 * file buffer at byte granularity, we need a copy that GCC cannot unroll
 * into ldm. `noinline` stops GCC in its tracks. Cost: one function call
 * per copy, paid once at level load. */
__attribute__((noinline))
void memcpy_unaligned(void *dst, const void *src, size_t n)
{
	unsigned char *d = (unsigned char *)dst;
	const unsigned char *s = (const unsigned char *)src;
	while (n--) *d++ = *s++;
}

/* ---- platform init ---- */

bool platform_init(void)
{
	_ticks_base = svcGetSystemTick();

	/* Ensure trace ring is dumped on any exit */
	atexit(trace_dump);

	/* Disable upstream's Debug flag. WinMain on PC sets Debug=false during
	 * startup (main.c:161), but on 3DS we don't run WinMain so the flag
	 * stays at its global initialiser of true. That routes scattered
	 * DebugPrintf("...%f...", ...) calls in level loaders (e.g. node.c
	 * NodeLoad's per-node move_len trace) through vsnprintf -> _dtoa_r.
	 * On larger levels the cost compounds and has been observed to crash
	 * inside _dtoa_r mid-load on hardware. Force the flag off here, before
	 * any loader can fire, so DebugPrintf returns immediately. */
	{
		extern bool Debug;
		Debug = false;
	}

	trace("platform_init: start");

	/* Enable New3DS 804 MHz mode when available */
	bool is_n3ds = false;
	APT_CheckNew3DS(&is_n3ds);
	if (is_n3ds)
		osSetSpeedupEnable(true);

	trace("platform_init: romfsInit");

	/* romfs precedence: try the .3dsx's embedded romfs FIRST. If that
	 * fails (slim build with no embedded section), fall back to
	 * sdmc:/3ds/forsaken/forsaken.romfs.
	 *
	 * Why this order: the embedded romfs ships in lock-step with the
	 * code, so a FULL build always boots a self-consistent dataset.
	 * The SD file is a long-lived blob from a previous SLIM iteration
	 * and can drift behind the code (e.g., a level-list update only
	 * landed in the last build's romfs). Letting it win silently led
	 * to autotest cycling 1998 SP slots even with an EDITION=remaster
	 * code build.
	 *
	 * Production CIA installs never set up the SD file, so they hit
	 * the embedded path on the first try — behaviour unchanged. */
	{
		Result rc = romfsInit();
		if (R_SUCCEEDED(rc)) {
			trace("platform_init: using embedded romfs");
		} else {
			trace("platform_init: no embedded romfs, trying sdmc fallback");
			static const u16 ext_path[] = u"/3ds/forsaken/forsaken.romfs";
			FS_Path apath = { PATH_EMPTY, 1, (u8*)"" };
			FS_Path fpath = { PATH_UTF16, sizeof(ext_path), (u8*)ext_path };
			Handle fh = 0;
			rc = FSUSER_OpenFileDirectly(&fh, ARCHIVE_SDMC, apath, fpath, FS_OPEN_READ, 0);
			if (R_SUCCEEDED(rc)) {
				rc = romfsMountFromFile(fh, 0, "romfs");
				if (R_SUCCEEDED(rc)) {
					trace("platform_init: mounted external sdmc romfs");
				} else {
					FSFILE_Close(fh);
					trace("platform_init: ERROR — sdmc romfs mount failed");
				}
			} else {
				trace("platform_init: ERROR — no romfs available");
			}
		}
	}

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
	trace("platform_init_video: BOOT_TAG_v3_idempotent_renderer");
	trace("platform_init_video: gfxInitDefault");

	/* Initialize 3DS graphics service (GSP) - required before picaGL.
	 *
	 * Use VRAM for framebuffers (third arg `true`).  gfxInitDefault()
	 * places framebuffers at the START of the linear heap (top buffer at
	 * 0x30000000, ~280 KB each, double-buffered) without marking those
	 * pages as occupied in libctru's linearAlloc bookkeeping.  Subsequent
	 * linearAlloc() calls — including every BSP vertex buffer Mload
	 * allocates — happily return overlapping addresses.  Each frame, the
	 * GPU's display transfer (VRAM render target → libctru framebuffer)
	 * writes the rendered top-screen image into those linear-heap pages,
	 * clobbering whatever buffer happens to share the address space.
	 * Where the screen pixels are black (clear / skybox / sky portal),
	 * the corresponding bytes in the overlapping linearAlloc buffer
	 * become 0 — vertex floats turn to 0.0f, triangles collapse to the
	 * origin, and that BSP geometry vanishes.
	 *
	 * Tloloc Temple was hit hardest because its BSP layout placed many
	 * groups in the framebuffer-overlapping address range.  Hardware-only
	 * — Mandarine/Citra emulators don't model the linearAlloc/framebuffer
	 * overlap so the bug never surfaces in emulation.
	 *
	 * Putting the framebuffers in VRAM (0x1F000000+) takes them entirely
	 * off the linear heap and removes the overlap.  VRAM has 6 MB on OG
	 * 3DS — easily room for the framebuffers (~2 MB total) plus citro3d
	 * render targets (~2 MB) plus headroom; HD textures stay in linear
	 * heap (vram=false on Tex3DS_TextureImportStdio). */
	gfxInit(GSP_BGR8_OES, GSP_BGR8_OES, true);

	/* TODO: VRAM framebuffers come up holding whatever was last written to
	 * those addresses (GPU debris, a previous .3dsx's render target, or
	 * just uninitialised noise).  Manifests as ~2 s of corruption on the
	 * top screen during the title-screen load and persistent garbage on
	 * the bottom screen until the gameplay HUD first draws over it.
	 *
	 * A naive `memset(gfxGetFramebuffer(...), 0, w*h*3)` here crashes with
	 * a data abort writing to VRAM (FAR=0x1f300000) — the framebuffer
	 * mapping isn't fully writable from CPU at this point in init.  The
	 * proper fix is GPU-side: clear via C3D_RenderTargetClear after
	 * c3d_renderer_init has created the targets.  Punted for now;
	 * the initial corruption is cosmetic and short-lived. */

	/* [3DS] Hardware stereoscopic 3D starts disabled; the 3D slider logic in
	 * MainGameRender / DisplayTitle calls gfxSet3D(true/false) each frame based
	 * on osGet3DSliderState().  Initialise to false so the first frame is always
	 * mono regardless of slider position at boot. */
	gfxSet3D(false);

	/* CIA installs don't bundle the Nintendo DSP firmware (legal blob), so
	 * users who haven't run dsp1 separately will boot with no audio and no
	 * explanation. Check now — while we still have a plain framebuffer that
	 * the console library can render to — and show a friendly warning with
	 * dsp1 instructions if the blob is missing. Dismissing the screen hands
	 * the top screen back to citro3d as usual. */
	{
		extern bool sound_check_dsp_firmware_available(void);
		extern void sound_show_missing_firmware_warning(void);
		if (!sound_check_dsp_firmware_available())
			sound_show_missing_firmware_warning();
	}

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
	/* Mode is a dynamic-mode list on PC/SDL (populated by render_mode_select).
	 * On 3DS we have exactly one fixed mode — point Mode at the inline
	 * ThisMode so code like `render_info.Mode[CurrMode].w` (SetGamePrefs,
	 * shutdown path) doesn't dereference NULL and data-abort. */
	render_info.Mode = &render_info.ThisMode;
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

#ifdef VERBOSE_TRACE
/* Counter armed by autotest_tick before a hot-jump. Decremented on each
 * frame present so we get enter/exit traces around the first ~8 swaps
 * after the transition — enough to see if the GPU hangs in pglSwapBuffers
 * or if we stop entering present at all. */
int _vt_flip_remaining = 0;
#endif

void platform_render_present(render_info_t *info)
{
	(void)info;
#ifdef VERBOSE_TRACE
	if (_vt_flip_remaining > 0) {
		char _b[48];
		snprintf(_b, sizeof(_b), "FLIP: enter (%d left)", _vt_flip_remaining);
		trace(_b);
	}
	pglSwapBuffers();
	if (_vt_flip_remaining > 0) {
		char _b[48];
		snprintf(_b, sizeof(_b), "FLIP: exit (%d left)", _vt_flip_remaining);
		trace(_b);
		_vt_flip_remaining--;
	}
#else
	pglSwapBuffers();
#endif
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
