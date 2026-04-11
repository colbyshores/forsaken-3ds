#ifndef MAIN_3DS_INCLUDED
#define MAIN_3DS_INCLUDED

#include <3ds.h>
#include "render.h"

bool platform_init(void);
bool platform_init_video(void);
void platform_render_present(render_info_t *info);
void platform_shutdown(void);
u_int32_t platform_get_ticks(void);
void platform_delay(u_int32_t ms);
void trace(const char *msg);

#endif
