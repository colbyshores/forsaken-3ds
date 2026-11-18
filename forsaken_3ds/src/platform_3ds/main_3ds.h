#pragma once

/* main_3ds.h -- 3DS platform layer: boot, heaps, video init, timing
 *
 * main_3ds.cpp owns process start. Its __system_allocateHeaps override
 * runs before main and sizes the malloc and linear heaps from the memory
 * region the OS granted (>= 96 MB: 32 + 32 MB, otherwise 32 + 22 MB).
 * platform_get_ticks is milliseconds since platform_init.
 * Every thread that does float math calls vfp_set_fz_dn or
 * vfp_thread_fz_dn at entry: FPSCR is per-thread and the engine assumes
 * flush-to-zero with default NaN. The c3d_* functions are defined in
 * render_c3d.cpp.
 */

#include "util/platform.h"

#include <3ds.h>
#include "rendering/render.h"

/* No letterboxing — render at the native 400x240 (5:3) resolution.
 * The Open Pandora port (800x480, also 5:3) proves this aspect ratio
 * works correctly with the engine's projection and HUD layout. */

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

/* Citro3D context/present glue (render_c3d.c). These replace the old
 * picaGL-named shim; picaGL itself is gone. */
void c3d_context_init(void);
void c3d_context_exit(void);
void c3d_present(void);
void c3d_transfer_eye(unsigned int eye);
bool c3d_hud_begin_bottom(void);
void c3d_hud_end_bottom(void);
void c3d_clear_bottom_screen(void);
