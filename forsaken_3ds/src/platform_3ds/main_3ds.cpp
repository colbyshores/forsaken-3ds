/*
 * main_3ds.c - Nintendo 3DS platform layer replacing main_sdl.c
 *
 * Initializes the 3DS hardware (gfx, HID, romfs),
 * sets up the Citro3D context on the top screen, and provides
 * the platform_*() functions consumed by main.c.
 */

#ifdef __3DS__

#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include "game/main.h"
#include "platform_3ds/main_3ds.h"
#include "util/util.h"
#include "rendering/render.h"
#include "util/hunk_alloc.h"
#include "scripting/lua_config.h"
#include "audio/sound_3ds.h"


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
 * will hand us, so the heap sizes are picked at runtime in the
 * __system_allocateHeaps weak override below. Detection signal:
 * osGetMemRegionSize(MEMREGION_APPLICATION) — the actual region the OS
 * gave us:
 *   - >= 96 MB (N3DS SystemModeExt = 124 MB, or OG HIMEM = 96 MB):
 *     32+32 MB heap, 512² texture pack via render_c3d.c.
 *   - <  96 MB (OG without HIMEM grant = 64 MB):
 *     32+22 MB heap, 256² texture pack via render_c3d.c. */
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
#include <3ds/os.h>
#include <3ds/svc.h>
#include <3ds/allocator/mappable.h>

extern u32 __ctru_heap;
extern u32 __ctru_linear_heap;
extern char *fake_heap_start, *fake_heap_end;

/* extern "C" is load-bearing: libctru calls this by its C name and links
 * its own weak default otherwise. A mangled C++ name compiles and links
 * cleanly and silently loses the override (tier detection gone). */
extern "C" void __system_allocateHeaps(void)
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
	                 MEMOP_ALLOC, (MemPerm)(MEMPERM_READ | MEMPERM_WRITE));

	svcControlMemory(&__ctru_linear_heap, 0, 0, __ctru_linear_heap_size,
	                 MEMOP_ALLOC_LINEAR, (MemPerm)(MEMPERM_READ | MEMPERM_WRITE));

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

/* ---- R1: FPSCR FZ|DN (AGENTS.md §3) ----
 *
 * Rationale (keep with the asm): decay loops can reach denormal (subnormal)
 * values, and the VFP11 handles denormals slowly; Flush-to-Zero (bit 24) and
 * Default-NaN (bit 25) are this engine's chosen numerical mode. FPSCR is
 * per-thread state, so main and every float worker thread must establish it
 * at its own entry — main's state says nothing about a worker's. Modern
 * libctru already initializes FZ|DN on supported entry paths — verified
 * 2026-10-25 by disassembling the linked libctru.a (devkitARM 15.2.0):
 * initThreadVars executes mov r1,#0x03000000; vmsr fpscr,r1 on the main and
 * threadCreate paths, matching this helper's FZ|DN exactly; these helpers are
 * the defensive pin with read-back, not a speedup claim. Never toggle FZ|DN mid-gameplay: it changes numerical
 * semantics for everything running on that thread.
 */
int vfp_set_fz_dn(void)
{
	uint32_t fpscr;

	__asm__ __volatile__("vmrs %0, fpscr" : "=r"(fpscr));
	fpscr |= (1u << 24) | (1u << 25); /* FZ | DN */
	__asm__ __volatile__("vmsr fpscr, %0" : : "r"(fpscr) : "cc");

	/* Read back: a toolchain or CPU that dropped the write must stop us
	 * before gameplay, not silently run in a different numerical mode. */
	__asm__ __volatile__("vmrs %0, fpscr" : "=r"(fpscr));
	return (fpscr & ((1u << 24) | (1u << 25))) == ((1u << 24) | (1u << 25));
}

void vfp_thread_fz_dn(void)
{
	/* Worker entry: same FZ|DN establishment as main (per-thread FPSCR,
	 * denormal stall rationale above). A worker has no clean failure path
	 * mid-audio; set and continue — main has already verified the hardware
	 * honors the write before gameplay started. */
	(void)vfp_set_fz_dn();
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

	/* Disable upstream's Debug flag. WinMain on PC sets Debug=false during
	 * startup (main.c:161), but on 3DS we don't run WinMain so the flag
	 * stays at its global initialiser of true. That routes scattered
	 * DebugPrintf("...%f...", ...) calls in level loaders (e.g. node.c
	 * NodeLoad's per-node move_len trace) through vsnprintf -> _dtoa_r.
	 * On larger levels the cost compounds and has been observed to crash
	 * inside _dtoa_r mid-load on hardware. Force the flag off here, before
	 * any loader can fire, so DebugPrintf returns immediately. */
	Debug = false;

	/* Verify __system_allocateHeaps set up mappableInit correctly.  Our
	 * override mirrors libctru's default init sequence (svcControlMemory
	 * ×2 + mappableInit + sbrk fakes) — if a future libctru version
	 * changes that sequence and we miss the update, gfxInit() crashes
	 * deep in gspInit (NULL deref at FAR=0x800) instead of failing
	 * loudly here.  Catch the regression at the right place: probe
	 * mappableAlloc and verify it returns an address inside the
	 * range our override passed to mappableInit. */
	{
		void *probe = mappableAlloc(4096);
		uintptr_t addr = (uintptr_t)probe;
		if (probe == NULL || addr < 0x10000000 || addr >= 0x14000000) {
			svcBreak(USERBREAK_PANIC);
		}
		mappableFree(probe);
	}

	/* New 3DS: enable the 804 MHz clock + 2 MB L2 cache. */
	bool is_n3ds = false;
	APT_CheckNew3DS(&is_n3ds);
	if (is_n3ds) {
		osSetSpeedupEnable(true);
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
		bool ok = Hunk_Init(8 * 1024 * 1024);
		if (!ok) return false;
	}

	/* Game data is the romfs embedded in the .3dsx / CIA. A missing romfs
	 * is reported later by missing_folders() in AppInit. */
	romfsInit();

	/* Set working directory so relative paths (Data/, Configs/) resolve */
	chdir("romfs:/");

	/* Create save game directory on SD card (romfs is read-only) */
	mkdir("sdmc:/3ds", 0777);
	mkdir("sdmc:/3ds/forsaken", 0777);
	mkdir("sdmc:/3ds/forsaken/savegame", 0777);

	return true;
}

/* ---- video / Citro3D init ---- */

/* 3DS top screen: 400x240 (5:3 landscape).
 * Same aspect ratio as the Open Pandora (800x480) which runs Forsaken
 * natively at 5:3 without any letterboxing.  Render at the full native
 * resolution for maximum screen utilisation. */
#define SCREEN_WIDTH        400
#define SCREEN_HEIGHT       240

bool platform_init_video(void)
{
	/* Initialize 3DS graphics service (GSP) - required before Citro3D.
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
		if (!sound_check_dsp_firmware_available()) {
			sound_show_missing_firmware_warning();
		}
	}

	/* Initialize the Citro3D context */
	c3d_context_init();

	_video_initialized = true;

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

	/* Standard landscape aspect ratio. The backend's projection fix
	 * applies a proper -90° rotation ([[0,1],[-1,0]], det=+1), which
	 * rotates axes without swapping the _11/_22 scales. An earlier hack
	 * inverted this to H/W assuming a reflection that swapped the scales;
	 * with the proper rotation in place that hack over-compensates and
	 * distorts vfov. */
	render_info.aspect_ratio = (float)SCREEN_WIDTH / (float)SCREEN_HEIGHT;

	DebugPrintf("platform_init_video: citro3d context created %dx%d\n",
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
}

/* ---- frame present ---- */


void platform_render_present(render_info_t *info)
{
	(void)info;
	c3d_present();
}

/* ---- shutdown ---- */

void platform_shutdown(void)
{
	if (_video_initialized)
	{
		c3d_context_exit();
		gfxExit();
		_video_initialized = false;
	}
	romfsExit();
}

#endif /* __3DS__ */
