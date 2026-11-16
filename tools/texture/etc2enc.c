/*
 * etc2enc - offline ETC2 encoder for the PC texture pack.
 *
 * Reads a PNG master, writes a KTX2 file containing ETC2
 * (VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK) mip levels. Only opaque textures
 * are encoded; if the PNG carries meaningful alpha the tool exits
 * with status 2 and writes nothing, so the runtime keeps that texture
 * on the uncompressed RGBA path (the designated RGBA exception class:
 * sprites, decals, fonts - exactly the textures with alpha).
 *
 * Encoding quality: per 4x4 block it searches ETC2 individual and
 * differential modes (both sub-block flips, all 8 modifier tables)
 * and keeps the least-squares winner. T/H/planar modes are not tried;
 * the visual bar is parity with the 3DS ETC1A4 pack, which uses the
 * same mode family.
 *
 * Build: see tools/texture/Makefile. Run by the PC build (Makefile
 * target textures-etc2); outputs sit next to the PNG masters and are
 * gitignored.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <png.h>

typedef unsigned char u8;
typedef unsigned int u32;

/* ETC modifier tables as (a, b) pairs; pixel index maps
 * 0 -> -b, 1 -> -a, 2 -> +a, 3 -> +b. */
static const int kMod[8][2] = {
    {2, 8}, {5, 17}, {9, 29}, {13, 42},
    {18, 60}, {24, 80}, {33, 106}, {47, 183}
};

static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

static int extend4(int v) { return (v << 4) | v; }
static int extend5(int v) { return (v << 3) | (v >> 2); }

/* Evaluate one sub-block (8 pixels) for a candidate base colour and
 * modifier table; returns squared error and fills indices. */
static long eval_sub(const u8 *px, const int *pixidx, int n,
                     int br, int bg, int bb, int table, int *out_idx)
{
    long err = 0;
    int a = kMod[table][0], b = kMod[table][1];
    int modv[4] = {-b, -a, a, b};
    for (int i = 0; i < n; i++) {
        const u8 *p = px + pixidx[i] * 4;
        long best = 1L << 60; int bi = 0;
        for (int m = 0; m < 4; m++) {
            int dr = (int)p[0] - clampi(br + modv[m], 0, 255);
            int dg = (int)p[1] - clampi(bg + modv[m], 0, 255);
            int db = (int)p[2] - clampi(bb + modv[m], 0, 255);
            long e = (long)dr * dr + (long)dg * dg + (long)db * db;
            if (e < best) { best = e; bi = m; }
        }
        out_idx[pixidx[i]] = bi;
        err += best;
    }
    return err;
}

/* Average colour of a sub-block, per channel. */
static void avg_sub(const u8 *px, const int *pixidx, int n, int *r, int *g, int *b)
{
    long sr = 0, sg = 0, sb = 0;
    for (int i = 0; i < n; i++) {
        const u8 *p = px + pixidx[i] * 4;
        sr += p[0]; sg += p[1]; sb += p[2];
    }
    *r = (int)(sr / n); *g = (int)(sg / n); *b = (int)(sb / n);
}

/* Encode one 4x4 RGBA block (alpha ignored) into 8 ETC2 bytes. */
static void encode_block(const u8 *px, u8 out[8])
{
    /* Sub-block pixel sets for both flips. Pixel index = y*4+x. */
    int horiz[2][8], vert[2][8];
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++) {
            int p = y * 4 + x;
            horiz[y < 2 ? 0 : 1][(y % 2) * 4 + x] = p;
            vert[x < 2 ? 0 : 1][(x % 2) + y * 2] = p;
        }

    long best_err = -1;
    u8 best[8];

    for (int mode = 0; mode < 2; mode++) {       /* 0=individual, 1=differential */
        for (int flip = 0; flip < 2; flip++) {
            const int (*subs)[8] = flip ? vert : horiz;
            int idx[16];
            long err = 0;
            u8 blk[8] = {0};

            if (mode == 0) {
                int base[2][3], ext[2][3];
                for (int s = 0; s < 2; s++) {
                    avg_sub(px, subs[s], 8, &base[s][0], &base[s][1], &base[s][2]);
                    for (int c = 0; c < 3; c++) ext[s][c] = extend4(base[s][c] >> 4);
                }
                int tables[2] = {0, 0};
                for (int s = 0; s < 2; s++) {
                    long be = -1;
                    for (int t = 0; t < 8; t++) {
                        int tix[16];
                        long e = eval_sub(px, subs[s], 8, ext[s][0], ext[s][1], ext[s][2], t, tix);
                        if (be < 0 || e < be) { be = e; tables[s] = t; memcpy(idx, tix, sizeof idx); }
                    }
                    err += be;
                }
                blk[0] = (u8)((base[0][0] >> 4 << 4) | (base[1][0] >> 4));
                blk[1] = (u8)((base[0][1] >> 4 << 4) | (base[1][1] >> 4));
                blk[2] = (u8)((base[0][2] >> 4 << 4) | (base[1][2] >> 4));
                blk[3] = (u8)((tables[0] << 5) | (tables[1] << 2) | (flip << 1) | 0);
            } else {
                int avg[2][3];
                for (int s = 0; s < 2; s++)
                    avg_sub(px, subs[s], 8, &avg[s][0], &avg[s][1], &avg[s][2]);
                int b0[3], d[3], ext0[3], ext1[3];
                int ok = 1;
                for (int c = 0; c < 3; c++) {
                    b0[c] = avg[0][c] >> 3;
                    d[c] = (avg[1][c] >> 3) - b0[c];
                    if (d[c] < -4 || d[c] > 3) { ok = 0; break; }
                    ext0[c] = extend5(b0[c]);
                    ext1[c] = extend5(clampi(b0[c] + d[c], 0, 31));
                }
                if (!ok) continue;
                int tables[2] = {0, 0}, exts[2][3];
                memcpy(exts[0], ext0, sizeof ext0);
                memcpy(exts[1], ext1, sizeof ext1);
                for (int s = 0; s < 2; s++) {
                    long be = -1;
                    for (int t = 0; t < 8; t++) {
                        int tix[16];
                        long e = eval_sub(px, subs[s], 8, exts[s][0], exts[s][1], exts[s][2], t, tix);
                        if (be < 0 || e < be) { be = e; tables[s] = t; memcpy(idx, tix, sizeof idx); }
                    }
                    err += be;
                }
                blk[0] = (u8)((b0[0] << 3) | (d[0] & 7));
                blk[1] = (u8)((b0[1] << 3) | (d[1] & 7));
                blk[2] = (u8)((b0[2] << 3) | (d[2] & 7));
                blk[3] = (u8)((tables[0] << 5) | (tables[1] << 2) | (flip << 1) | 1);
            }

            /* Pixel index planes: pixel k = 4*x + y (column-major),
             * MSB plane first, MSB-first bit order. */
            for (int i = 0; i < 16; i++) {
                int x = i / 4, y = i % 4;
                int v = idx[y * 4 + x];
                if (v & 2) blk[4 + i / 8] |= (u8)(0x80 >> (i % 8));
                if (v & 1) blk[6 + i / 8] |= (u8)(0x80 >> (i % 8));
            }

            if (best_err < 0 || err < best_err) { best_err = err; memcpy(best, blk, 8); }
        }
    }
    memcpy(out, best, 8);
}

static u8 *load_png_rgba(const char *path, int *w, int *h, int *has_alpha)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info = png_create_info_struct(png);
    if (setjmp(png_jmpbuf(png))) { fclose(f); return NULL; }
    png_init_io(png, f);
    png_read_info(png, info);
    {
        png_uint_32 uw, uh;
        png_get_IHDR(png, info, &uw, &uh, NULL, NULL, NULL, NULL, NULL);
        *w = (int)uw; *h = (int)uh;
    }
    png_set_expand(png);
    png_set_strip_16(png);
    png_set_add_alpha(png, 0xff, PNG_FILLER_AFTER);
    png_read_update_info(png, info);
    u8 *data = malloc((size_t)*w * *h * 4);
    png_bytep *rows = malloc(sizeof(png_bytep) * *h);
    for (int y = 0; y < *h; y++) rows[y] = data + (size_t)y * *w * 4;
    png_read_image(png, rows);
    free(rows);
    fclose(f);
    *has_alpha = 0;
    for (long i = 3; i < (long)*w * *h * 4; i += 4)
        if (data[i] < 250) { *has_alpha = 1; break; }
    return data;
}

/* One mip level: box-filter downsample by 2 (edge clamp). */
static u8 *downsample(const u8 *src, int sw, int sh, int *dw, int *dh)
{
    *dw = sw > 1 ? sw / 2 : 1;
    *dh = sh > 1 ? sh / 2 : 1;
    u8 *dst = malloc((size_t)*dw * *dh * 4);
    for (int y = 0; y < *dh; y++)
        for (int x = 0; x < *dw; x++) {
            long acc[4] = {0, 0, 0, 0}; int n = 0;
            for (int dy = 0; dy < 2; dy++)
                for (int dx = 0; dx < 2; dx++) {
                    int sx = 2 * x + dx < sw ? 2 * x + dx : sw - 1;
                    int sy = 2 * y + dy < sh ? 2 * y + dy : sh - 1;
                    const u8 *p = src + ((size_t)sy * sw + sx) * 4;
                    for (int c = 0; c < 4; c++) acc[c] += p[c];
                    n++;
                }
            u8 *q = dst + ((size_t)y * *dw + x) * 4;
            for (int c = 0; c < 4; c++) q[c] = (u8)(acc[c] / n);
        }
    return dst;
}

static void put32(FILE *f, u32 v)
{
    u8 b[4] = {(u8)v, (u8)(v >> 8), (u8)(v >> 16), (u8)(v >> 24)};
    fwrite(b, 1, 4, f);
}
static void put64(FILE *f, unsigned long long v)
{
    for (int i = 0; i < 8; i++) fputc((int)(v >> (8 * i)) & 0xff, f);
}

int main(int argc, char **argv)
{
    if (argc != 3) { fprintf(stderr, "usage: etc2enc in.png out.ktx2\n"); return 1; }
    int w, h, alpha;
    u8 *img = load_png_rgba(argv[1], &w, &h, &alpha);
    if (!img) { fprintf(stderr, "etc2enc: cannot read %s\n", argv[1]); return 1; }
    if (alpha) { free(img); return 2; }  /* RGBA exception class */

    /* Build mip chain in memory. */
    int levels = 0;
    for (int t = w > h ? w : h; t >= 1; t /= 2) levels++;
    u8 **level_data = calloc(levels, sizeof(u8 *));
    u32 *level_size = calloc(levels, sizeof(u32));
    const u8 *cur = img; int cw = w, ch = h;
    u8 *owned = NULL;
    for (int l = 0; l < levels; l++) {
        int bw = (cw + 3) / 4, bh = (ch + 3) / 4;
        level_size[l] = (u32)bw * bh * 8;
        level_data[l] = malloc(level_size[l]);
        for (int by = 0; by < bh; by++)
            for (int bx = 0; bx < bw; bx++) {
                u8 block[64];
                for (int y = 0; y < 4; y++)
                    for (int x = 0; x < 4; x++) {
                        int sx = bx * 4 + x < cw ? bx * 4 + x : cw - 1;
                        int sy = by * 4 + y < ch ? by * 4 + y : ch - 1;
                        memcpy(block + (y * 4 + x) * 4, cur + ((size_t)sy * cw + sx) * 4, 4);
                    }
                encode_block(block, level_data[l] + ((size_t)by * bw + bx) * 8);
            }
        if (l + 1 < levels) {
            int nw, nh;
            owned = downsample(cur, cw, ch, &nw, &nh);
            if (cur != img) free((void *)cur);
            cur = owned; cw = nw; ch = nh;
        }
    }

    FILE *f = fopen(argv[2], "wb");
    if (!f) { fprintf(stderr, "etc2enc: cannot write %s\n", argv[2]); return 1; }
    static const u8 ktx2id[12] = {0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32,
                                  0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A};
    fwrite(ktx2id, 1, 12, f);
    put32(f, 144);            /* VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK */
    put32(f, 1);              /* typeSize */
    put32(f, (u32)w); put32(f, (u32)h);
    put32(f, 0);              /* pixelDepth */
    put32(f, 0);              /* layerCount */
    put32(f, 1);              /* faceCount */
    put32(f, (u32)levels);
    put32(f, 0);              /* supercompressionScheme: none */

    /* DFD: minimal block (vendor 0, type 0, version 2, 40 bytes base
     * + no samples beyond the mandatory header words). Our loader
     * ignores DFD content; fields keep the file spec-shaped. */
    u32 dfd_total = 40;
    long header_bytes = 12 + 9 * 4 + 40;   /* id + 9 fields + index (u64/u32 pairs) */
    long dfd_off = header_bytes + (long)levels * 24;
    put64(f, (unsigned long long)dfd_off); put32(f, dfd_total);
    put64(f, 0); put32(f, 0);                /* kvd */
    put64(f, 0); put64(f, 0);                /* sgd */
    long offset = dfd_off + dfd_total;
    for (int l = 0; l < levels; l++) {
        put64(f, (unsigned long long)offset);
        put64(f, level_size[l]);
        put64(f, level_size[l]);
        offset += level_size[l];
    }
    /* DFD block */
    put32(f, dfd_total); put32(f, 0); put32(f, 0); put32(f, 2 | (40 << 16));
    for (int i = 0; i < 6; i++) put32(f, 0);
    for (int l = 0; l < levels; l++) fwrite(level_data[l], 1, level_size[l], f);
    fclose(f);

    for (int l = 0; l < levels; l++) free(level_data[l]);
    free(level_data); free(level_size);
    if (cur != img) free((void *)cur);
    free(img);
    return 0;
}
