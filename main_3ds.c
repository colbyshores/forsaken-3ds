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
/* Heap sizing — runtime-detected via __system_allocateHeaps override.
 *
 * The shipping unified CIA (assets/forsaken.rsf) requests SystemModeExt
 * (124 MB on N3DS) but deliberately omits SystemMode: 96MB (the OG HIMEM
 * grant request, which crashes some OG HOME menus before our code can
 * run). The opt-in HIMEM CIA (assets/forsaken_himem.rsf) does request
 * SystemMode for OG users who want full quality and have a working menu.
 *
 * In either case we don't know at compile time how much memory the OS
 * will hand us, so the heap sizes have to be picked at runtime in the
 * __system_allocateHeaps weak override below. Detection signal:
 * osGetMemRegionSize(MEMREGION_APPLICATION) — the actual region the OS
 * gave us:
 *   - >= 96 MB (N3DS SystemModeExt = 124 MB, or OG HIMEM = 96 MB):
 *     32+32 MB heap, 512² texture pack via render_c3d.c.
 *   - <  96 MB (OG without HIMEM grant = 64 MB):
 *     32+22 MB heap, 256² texture pack via render_c3d.c.
 *
 * The link-time defaults below are only honored if the override is
 * disabled via -DFORSAKEN_HEAP_FIXED — used for synthetic OG-tight-budget
 * tests on N3DS hardware. */
#ifndef MALLOC_HEAP_MB
#define MALLOC_HEAP_MB 32
#endif
#ifndef LINEAR_HEAP_MB
#define LINEAR_HEAP_MB 32
#endif
u32 __ctru_heap_size        = MALLOC_HEAP_MB * 1024 * 1024;
u32 __ctru_linear_heap_size = LINEAR_HEAP_MB * 1024 * 1024;
u32 __stacksize__           = 256 * 1024; /* override libctru's 32 KB default */

/* __system_allocateHeaps — weak override of libctru's default. Runs
 * before C++ ctors and main() but after svc syscalls are wired up, so
 * osGetMemRegionSize is safe to call here. Mirrors the steps in
 * libctru-2.7.0's default (svcControlMemory ×2 + mappableInit + sbrk
 * fakes); diverges only in choosing heap sizes based on region. */
#ifndef FORSAKEN_HEAP_FIXED
#include <3ds/os.h>
#include <3ds/svc.h>
#include <3ds/allocator/mappable.h>

extern u32 __ctru_heap;
extern u32 __ctru_linear_heap;
extern char *fake_heap_start, *fake_heap_end;

void __system_allocateHeaps(void)
{
	u32 tmp = 0;
	u64 region_size = osGetMemRegionSize(MEMREGION_APPLICATION);

	if (region_size >= 96 * 1024 * 1024) {
		/* N3DS SystemModeExt (124 MB) or OG HIMEM (96 MB) — full heap. */
		__ctru_heap_size        = 32 * 1024 * 1024;
		__ctru_linear_heap_size = 32 * 1024 * 1024;
	} else {
		/* OG Application (64 MB), no HIMEM — tight-budget heap.
		 * Validated on real OG hardware via YandyTheGnome's testing. */
		__ctru_heap_size        = 32 * 1024 * 1024;
		__ctru_linear_heap_size = 22 * 1024 * 1024;
	}

	__ctru_heap = 0x08000000;
	svcControlMemory(&tmp, __ctru_heap, 0, __ctru_heap_size,
	                 MEMOP_ALLOC, MEMPERM_READ | MEMPERM_WRITE);

	svcControlMemory(&__ctru_linear_heap, 0, 0, __ctru_linear_heap_size,
	                 MEMOP_ALLOC_LINEAR, MEMPERM_READ | MEMPERM_WRITE);

	/* CRITICAL: initialize the "mappable" allocator that backs
	 * mappableAlloc() — gspInit uses this to reserve the virtual
	 * address for svcMapMemoryBlock(GSP_SHARED_MEM). Skipping this
	 * step causes a NULL-deref crash deep in libctru's gspInit on
	 * the first gfxInit(). libctru 2.7.0's default __system_allocateHeaps
	 * does this call right after the two svcControlMemory calls;
	 * any override has to mirror it. */
	mappableInit(0x10000000, 0x14000000);

	fake_heap_start = (char *)__ctru_heap;
	fake_heap_end   = (char *)__ctru_heap + __ctru_heap_size;
}
#endif

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

/* Tracing: use low-level write() to bypass stdio buffering.
 *
 * Gated on __3DS_DEBUG__ — release builds (the shipping CIA) get a
 * stub that compiles to nothing, so end-user installs don't write
 * to their SD card every level load. Diagnostic builds (`make
 * DEBUG=1`, which defines __3DS_DEBUG__) get the full path, with
 * every level-load checkpoint and per-frame counter-gated trace
 * persisted to sdmc:/forsaken_trace.txt for post-crash analysis.
 *
 * Truncation policy (debug builds only): the file is truncated
 * EXACTLY ONCE per process lifetime, on the very first call to
 * either trace_enable() or trace(). Subsequent re-opens (e.g., after
 * a trace_dump() close) use O_APPEND so we don't lose what was
 * written before the close. Without this, the autotest's "DONE ->
 * trace_dump() -> chain-load" path was wiping the whole trace
 * because platform_shutdown's later trace() call would re-open with
 * O_TRUNC. */
#include <fcntl.h>

#ifdef __3DS_DEBUG__
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
#else
/* Release: no trace infrastructure at all. Call sites still resolve
 * (the function exists), but the body is empty so the optimizer
 * deletes the snprintf chain that built the trace string. */
void trace_enable(void) {}
void trace(const char *msg) { (void)msg; }
#endif /* __3DS_DEBUG__ */

/* boot_log — always-on diagnostic write for boot-time failure tracking.
 * Writes to sdmc:/forsaken_boot.log.  Unlike trace() this is NOT gated
 * on __3DS_DEBUG__; useful for diagnosing CIA installs that fail to
 * boot in the field (e.g. LOMEM bring-up).  The last line in the file
 * is the last checkpoint the boot path reached before failure.
 *
 * Truncates the file on the very first call this process, then appends.
 * Each call also flushes via fsync so a sudden process termination
 * doesn't lose recent writes. */
static int  _boot_log_fd        = -1;
static int  _boot_log_truncated = 0;

void boot_log(const char *msg)
{
	if (_boot_log_fd < 0) {
		int flags = O_WRONLY | O_CREAT |
		            (_boot_log_truncated ? O_APPEND : O_TRUNC);
		_boot_log_fd = open("sdmc:/forsaken_boot.log", flags, 0666);
		_boot_log_truncated = 1;
	}
	if (_boot_log_fd >= 0) {
		int len = (int)strlen(msg);
		write(_boot_log_fd, msg, len);
		write(_boot_log_fd, "\n", 1);
		fsync(_boot_log_fd);
	}
}

void trace_dump(void) {
#ifdef __3DS_DEBUG__
	/* Flush only — don't close. Re-opens with O_APPEND would still
	 * preserve content per the truncated-once policy, but keeping the
	 * fd open avoids the open()/close() cost on chain-load paths that
	 * call trace() again immediately afterward. */
	if (_trace_fd >= 0) fsync(_trace_fd);
#endif
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
	boot_log("[boot] platform_init: start");

	/* Verify __system_allocateHeaps set up mappableInit correctly.  Our
	 * override mirrors libctru's default init sequence (svcControlMemory
	 * ×2 + mappableInit + sbrk fakes) — if a future libctru version
	 * changes that sequence and we miss the update, gfxInit() crashes
	 * deep in gspInit (NULL deref at FAR=0x800) instead of failing
	 * loudly here.  Catch the regression at the right place: probe
	 * mappableAlloc and verify it returns an address inside the
	 * range our override passed to mappableInit. */
	{
		extern void mappableInit(u32, u32);
		extern void *mappableAlloc(size_t);
		extern void  mappableFree(void *);
		void *probe = mappableAlloc(4096);
		uintptr_t addr = (uintptr_t)probe;
		if (probe == NULL || addr < 0x10000000 || addr >= 0x14000000) {
			char _b[160];
			snprintf(_b, sizeof(_b),
			         "[boot] FATAL: mappableAlloc returned %p (expected in [0x10000000,0x14000000)). "
			         "libctru ABI changed — re-audit __system_allocateHeaps override.",
			         probe);
			boot_log(_b);
			svcBreak(USERBREAK_PANIC);
		}
		mappableFree(probe);
	}

	/* Enable New3DS 804 MHz mode when available */
	bool is_n3ds = false;
	APT_CheckNew3DS(&is_n3ds);
	if (is_n3ds)
		osSetSpeedupEnable(true);

	/* Always-on boot_log of the platform-detection result. This lands
	 * in sdmc:/forsaken_boot.log on every boot — lets us verify from
	 * the host which heap tier + texture pack ended up selected,
	 * without needing to do per-texture memory-consumption math. */
	{
		u64 region_size = osGetMemRegionSize(MEMREGION_APPLICATION);
		const char *tier = region_size >= 96ull * 1024 * 1024 ? "HIMEM (512²)" : "LOMEM (256²)";
		char _b[200];
		snprintf(_b, sizeof(_b),
		         "[platform] is_n3ds=%d region=%uMB tier=%s malloc=%uMB linear=%uMB",
		         (int)is_n3ds,
		         (unsigned)(region_size >> 20),
		         tier,
		         (unsigned)(__ctru_heap_size >> 20),
		         (unsigned)(__ctru_linear_heap_size >> 20));
		boot_log(_b);
	}

	/* Q3-style hunk arena for per-execbuf textureGroups[] arrays. The
	 * level mesh's Mloadheader.Group[].renderObject[].textureGroups[]
	 * and the model loaders' Mx(a)loadheader.Group.renderObject[]
	 * .textureGroups[] used to be inline MAX_TEXTURE_GROUPS-sized
	 * arrays in BSS; they're now small per-execbuf hunk allocations
	 * sized to numTextureGroups. Hundreds of small allocs per level
	 * is exactly what the hunk is for — bulk-freed via
	 * Hunk_FreeAll(TAG_LEVEL) on level transition. 8 MB easily covers
	 * the densest level. */
	{
		extern bool Hunk_Init(size_t);
		bool ok = Hunk_Init(8 * 1024 * 1024);
		char _b[96];
		snprintf(_b, sizeof(_b), "Hunk_Init: %s (8MB)", ok ? "OK" : "FAILED");
		trace(_b);
		if (!ok) return false;
	}

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
	boot_log("[boot] platform_init: done OK");

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
	boot_log("[boot] platform_init_video: entry");
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

	boot_log("[boot] platform_init_video: pre-pglInit");
	trace("platform_init_video: pglInit");

	/* Initialize citro3d-backed picaGL */
	pglInit();

	_video_initialized = true;
	trace("platform_init_video: pglInit done");
	boot_log("[boot] platform_init_video: pglInit done");

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

	boot_log("[boot] platform_init_video: pre-render_init");
	if (!render_init(&render_info))
	{
		DebugPrintf("platform_init_video: render_init failed\n");
		boot_log("[boot] platform_init_video: FAIL render_init returned false");
		return false;
	}

	boot_log("[boot] platform_init_video: done OK");
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
