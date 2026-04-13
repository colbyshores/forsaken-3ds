#ifndef MAIN_3DS_INCLUDED
#define MAIN_3DS_INCLUDED

#include <3ds.h>
#include "render.h"

/* Letterbox: render a 320x240 (4:3) logical screen into the center of
 * the 400x240 physical top screen. Shared with FSSetViewPort so it
 * knows to offset glViewport by 40 pixels. */
#define FS3DS_LETTERBOX_OFFSET_X 40

bool platform_init(void);
bool platform_init_video(void);
void platform_render_present(render_info_t *info);
void platform_shutdown(void);
u_int32_t platform_get_ticks(void);
void platform_delay(u_int32_t ms);
void trace(const char *msg);

/* Returns the 3D slider state (0.0–1.0).
 * On real hardware this is osGet3DSliderState().
 * In emulators that always return 0 you can override it by setting
 * stereo_test_slider = <value> in Configs/debug.txt (3DS-only config key). */
float platform_get_3d_slider(void);

#endif
