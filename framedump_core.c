/* framedump_core.c — portable half of the framedump library.
 *
 * No platform headers: plain C89 + stdio. The 3DS backend
 * (framedump_3ds.c) and the host test (tests/test_framedump.c) both
 * link this file. Pixel input is a packed RGB8 or RGBA8 buffer,
 * row-major, top row first — exactly what gfxGetFramebuffer returns.
 * BMP output is 24-bit uncompressed, bottom-up rows, 4-byte stride.
 * Byte order inside a pixel is stored as-is; the FNV-1a hash beside
 * each file is the render oracle (identical input bytes hash
 * identically, whatever the channel order), the BMP is for eyeballs.
 */
/* Compiles to nothing unless -DFRAMEDUMP is set (same pattern as
 * fidelity_3ds.c), so default builds are unaffected. FRAMEDUMP_HOSTTEST
 * enables it for the Linux unit test. */
#if defined(FRAMEDUMP) || defined(FRAMEDUMP_HOSTTEST)

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "framedump_core.h"

uint32_t framedump_fnv1a(const unsigned char *data, unsigned long len)
{
	uint32_t h = 2166136261u;
	unsigned long i;
	for (i = 0; i < len; i++) {
		h ^= data[i];
		h *= 16777619u;
	}
	return h;
}

/* Parse one trigger line. Returns 1 for DUMP (tag copied out, sanitized
 * to [A-Za-z0-9_-], truncated to fit), 2 for PING, 0 for anything else. */
int framedump_parse_cmd(const char *line, char *tag_out, unsigned long tag_cap)
{
	unsigned long n = 0;
	const char *p;
	if (tag_cap == 0) return 0;
	tag_out[0] = '\0';
	if (strncmp(line, "PING", 4) == 0 &&
	    (line[4] == '\0' || line[4] == '\n' || line[4] == '\r' ||
	     line[4] == ' ' || line[4] == '\t'))
		return 2;
	if (strncmp(line, "DUMP", 4) != 0) return 0;
	p = line + 4;
	while (*p == ' ' || *p == '\t') p++;
	while (*p && *p != '\n' && *p != '\r' && n + 1 < tag_cap) {
		char c = *p++;
		if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
		    (c >= '0' && c <= '9') || c == '_' || c == '-')
			tag_out[n++] = c;
		else
			tag_out[n++] = '_';
	}
	tag_out[n] = '\0';
	if (n == 0) {
		tag_out[0] = 'x';
		tag_out[1] = '\0';
	}
	return 1;
}

static void put_u16(unsigned char *b, unsigned v)
{
	b[0] = (unsigned char)(v & 0xff);
	b[1] = (unsigned char)((v >> 8) & 0xff);
}

static void put_u32(unsigned char *b, unsigned long v)
{
	b[0] = (unsigned char)(v & 0xff);
	b[1] = (unsigned char)((v >> 8) & 0xff);
	b[2] = (unsigned char)((v >> 16) & 0xff);
	b[3] = (unsigned char)((v >> 24) & 0xff);
}

/* Write a 24-bit BMP. Returns 0 on success, -1 on I/O error. */
int framedump_write_bmp(const char *path, const unsigned char *px,
                        unsigned w, unsigned h, int bpp)
{
	FILE *f;
	unsigned stride, y, x;
	unsigned char hdr[54];
	unsigned char pad[3] = { 0, 0, 0 };
	unsigned pix_bytes = (bpp == 4) ? 4u : 3u;

	if (w == 0 || h == 0 || (bpp != 3 && bpp != 4)) return -1;
	f = fopen(path, "wb");
	if (!f) return -1;
	stride = (w * 3u + 3u) & ~3u;
	memset(hdr, 0, sizeof(hdr));
	hdr[0] = 'B';
	hdr[1] = 'M';
	put_u32(hdr + 2, 54ul + (unsigned long)stride * h);
	put_u32(hdr + 10, 54ul);
	put_u32(hdr + 14, 40ul);
	put_u32(hdr + 18, w);
	put_u32(hdr + 22, h);
	put_u16(hdr + 26, 1u);
	put_u16(hdr + 28, 24u);
	if (fwrite(hdr, 1, sizeof(hdr), f) != sizeof(hdr)) {
		fclose(f);
		return -1;
	}
	/* BMP rows are bottom-up; input rows are top-down. */
	for (y = 0; y < h; y++) {
		const unsigned char *row = px + (unsigned long)(h - 1u - y) * w * pix_bytes;
		for (x = 0; x < w; x++) {
			if (fwrite(row + (unsigned long)x * pix_bytes, 1, 3u, f) != 3u) {
				fclose(f);
				return -1;
			}
		}
		if (fwrite(pad, 1, stride - w * 3u, f) != stride - w * 3u) {
			fclose(f);
			return -1;
		}
	}
	fclose(f);
	return 0;
}

#endif /* FRAMEDUMP */
