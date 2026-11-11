/* framedump_core.h — portable core declarations. Plain C, no platform
 * headers, so the host unit test and the 3DS backend share it. */
#ifndef FRAMEDUMP_CORE_INCLUDED
#define FRAMEDUMP_CORE_INCLUDED

#include <stdint.h>

/* FNV-1a over raw bytes. */
uint32_t framedump_fnv1a(const unsigned char *data, unsigned long len);

/* Trigger-line parser: 1 = DUMP (sanitized tag out), 2 = PING, 0 = no. */
int framedump_parse_cmd(const char *line, char *tag_out, unsigned long tag_cap);

/* 24-bit BMP writer. px is packed RGB8 (bpp 3) or RGBA8 (bpp 4),
 * row-major, top row first. Returns 0 on success, -1 on I/O error. */
int framedump_write_bmp(const char *path, const unsigned char *px,
                        unsigned w, unsigned h, int bpp);

#endif /* FRAMEDUMP_CORE_INCLUDED */
