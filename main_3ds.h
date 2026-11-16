#ifndef MAIN_3DS_INCLUDED
#define MAIN_3DS_INCLUDED

#include <3ds.h>
#include "render.h"

/* No letterboxing — render at the native 400x240 (5:3) resolution.
 * The Open Pandora port (800x480, also 5:3) proves this aspect ratio
 * works correctly with the engine's projection and HUD layout. */
#define FS3DS_LETTERBOX_OFFSET_X 0

/* R1 (AGENTS.md §3): establish FPSCR Flush-to-Zero (FZ) + Default-NaN (DN)
 * on every thread that executes float math, at thread entry, before any hot
 * path runs. Denormal (subnormal) results stall the VFP11; FZ|DN are this
 * engine's chosen numerical mode. FPSCR is per-thread state, so main and
 * every worker establish it themselves — modern libctru initializes it on
 * supported entry paths and these helpers are the defensive pin with
 * read-back, not a speedup claim. vfp_set_fz_dn returns 1 when the bits are
 * verified set; main must refuse to run on failure rather than silently run
 * in a different numerical mode. */
int  vfp_set_fz_dn(void);
void vfp_thread_fz_dn(void);

bool platform_init(void);
bool platform_init_video(void);
void platform_render_present(render_info_t *info);
void platform_shutdown(void);
u_int32_t platform_get_ticks(void);
void platform_delay(u_int32_t ms);
void trace(const char *msg);

/* Byte-wise memcpy that GCC cannot inline to `ldm/stm` (which require
 * 4-byte alignment on ARM and fault on misaligned file-buffer sources).
 * Use when copying structs out of a raw byte-parsed buffer. */
void memcpy_unaligned(void *dst, const void *src, size_t n);

/* Returns the 3D slider state (0.0–1.0).
 * On real hardware this is osGet3DSliderState().
 * In emulators that always return 0 you can override it by setting
 * stereo_test_slider = <value> in Configs/debug.txt (3DS-only config key). */
float platform_get_3d_slider(void);

/* Citro3D context/present glue (render_c3d.c). These replace the old
 * picaGL-named shim; picaGL itself is gone. */
void c3d_context_init(void);
void c3d_context_exit(void);
void c3d_present(void);
void c3d_transfer_eye(unsigned int eye);
bool c3d_hud_begin_bottom(void);
void c3d_hud_end_bottom(void);
void c3d_clear_bottom_screen(void);

#endif
