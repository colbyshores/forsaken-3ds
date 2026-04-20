#ifndef MAIN_3DS_INCLUDED
#define MAIN_3DS_INCLUDED

#include <3ds.h>
#include "render.h"

/* No letterboxing — render at the native 400x240 (5:3) resolution.
 * The Open Pandora port (800x480, also 5:3) proves this aspect ratio
 * works correctly with the engine's projection and HUD layout. */
#define FS3DS_LETTERBOX_OFFSET_X 0

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

#endif
