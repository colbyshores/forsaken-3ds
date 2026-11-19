/* ============================================================================
 * 11_textures.inc -- part of render_c3d.cpp (SINGLE TRANSLATION UNIT).
 *
 * Texture management: HD .t3x packs, PNG conversion and PICA200 Morton
 * tiling.
 *
 * Not a standalone header: no include guard and no includes of its own. It is
 * #included in a fixed order from render_c3d.cpp, which says why.
 * ========================================================================== */

/*===================================================================
	Texture management
===================================================================*/

void release_texture(TEXTURE_HANDLE texture)
{
	if (!texture) return;
	texture_t *texdata = (texture_t*)texture;
	if (texdata->initialized)
		C3D_TexDelete(&texdata->tex);
	free(texdata);
}

/*===================================================================
	Morton tiling for PICA200 textures
	PICA200 requires textures in 8x8-block Morton/Z-order layout.
	Input: linear RGBA8 (R,G,B,A bytes). Output: tiled ABGR8 (A,B,G,R).
===================================================================*/

static inline u32 _mortonInterleave(u32 x, u32 y)
{
	static const u32 xlut[] = {0x00,0x01,0x04,0x05,0x10,0x11,0x14,0x15};
	static const u32 ylut[] = {0x00,0x02,0x08,0x0a,0x20,0x22,0x28,0x2a};
	return xlut[x & 7] + ylut[y & 7];
}

static void tile_rgba4(const u_int8_t *src, u_int16_t *dst, int w, int h)
{
	int x, y;
	for (y = 0; y < h; y++)
	{
		int out_y = h - 1 - y;  /* flip Y for PICA200 */
		u32 coarse_y = out_y & ~7;
		for (x = 0; x < w; x++)
		{
			u32 morton = _mortonInterleave(x, out_y);
			u32 offset = morton + coarse_y * w + (x & ~7) * 8;
			int si = (y * w + x) * 4;
			/* RGBA8 → RGBA4444 packed u16 for GPU_RGBA4 */
			u_int8_t r = src[si + 0] >> 4;
			u_int8_t g = src[si + 1] >> 4;
			u_int8_t b = src[si + 2] >> 4;
			u_int8_t a = src[si + 3] >> 4;
			dst[offset] = (r << 12) | (g << 8) | (b << 4) | a;
		}
	}
}

/* Try loading an HD texture in t3x format (ETC1A4 compressed).
 * Path mapping: data\textures\foo.bmp → romfs:/hd/textures/foo.t3x
 * Uses Tex3DS_TextureImportStdio which handles format, tiling, VRAM. */
#include <tex3ds.h>

/* Pick the HD texture dir to load from, based on the actual memory
 * region size the OS gave us. Cached after the first call.
 *
 * Memory region (not hardware model) is the right signal because it
 * directly tracks whether we have room for the 512² pack:
 *   - Unified CIA on OG:        64 MB region  → 256² (hd_textures_lomem)
 *   - Unified CIA on N3DS:     124 MB region  → 512² (hd_textures)
 *   - HIMEM CIA on OG w/ grant: 96 MB region  → 512² (hd_textures)
 *   - HIMEM CIA on N3DS:       124 MB region  → 512² (hd_textures)
 *
 * Single-pack CIAs (LOMEM-only or HIMEM-only) still work — the fallback
 * dir is tried if the preferred one isn't shipped. */
static const char *s_hd_textures_dir = nullptr;
static const char *s_hd_textures_fallback = nullptr;

static void resolve_hd_textures_dirs(void)
{
	if (s_hd_textures_dir) return;
	u64 region_size = osGetMemRegionSize(MEMREGION_APPLICATION);
	if (region_size >= 96ull * 1024 * 1024) {
		s_hd_textures_dir      = "hd_textures";
		s_hd_textures_fallback = "hd_textures_lomem";
	} else {
		s_hd_textures_dir      = "hd_textures_lomem";
		s_hd_textures_fallback = "hd_textures";
	}
}

/* The part of a texture path after its "data\\" (or "data/") directory, or
 * NULL when there is none. */
static const char *hd_texture_key(const char *path)
{
	int i;

	for (i = 0; path[i]; i++)
	{
		if ((path[i] == 'd' || path[i] == 'D') &&
			(path[i+1] == 'a' || path[i+1] == 'A') &&
			(path[i+2] == 't' || path[i+2] == 'T') &&
			(path[i+3] == 'a' || path[i+3] == 'A') &&
			(path[i+4] == '\\' || path[i+4] == '/'))
			return &path[i+5];
	}
	return nullptr;
}

/* romfs:/<dir>/<key with .t3x>, lowercased with forward slashes. */
static void hd_texture_path(char *hd_path, size_t size, const char *dir,
	const char *key)
{
	char *p;

	snprintf(hd_path, size, "romfs:/%s/%s", dir, key);
	for (p = hd_path; *p; p++)
	{
		if (*p == '\\') *p = '/';
		if (*p >= 'A' && *p <= 'Z') *p += 32;
	}
	p = strrchr(hd_path, '.');
	if (p) strcpy(p, ".t3x");
	else strcat(hd_path, ".t3x");
}

/* Open the HD .t3x for a texture path: the preferred pack first, then the
 * other one. NULL when neither pack has it. */
static FILE *open_hd_texture(const char *path, char *hd_path, size_t size)
{
	const char *key;
	FILE *f;

	resolve_hd_textures_dirs();
	key = hd_texture_key(path);
	if (!key)
		return nullptr;

	hd_texture_path(hd_path, size, s_hd_textures_dir, key);
	f = fopen(hd_path, "rb");
	if (!f)
	{
		/* Preferred dir isn't shipped — try the fallback. Defensive;
		 * production CIAs always ship both packs, but this lets a
		 * future single-pack variant work without code changes. */
		hd_texture_path(hd_path, size, s_hd_textures_fallback, key);
		f = fopen(hd_path, "rb");
	}
	return f;
}

static bool try_load_hd_texture(TEXTURE_HANDLE *t, const char *path,
	u_int16_t *width, u_int16_t *height, bool *colorkey)
{
	char hd_path[512];
	FILE *f;
	texture_t *texdata;
	Tex3DS_Texture t3x;

	f = open_hd_texture(path, hd_path, sizeof(hd_path));
	if (!f)
		return false;

	/* Allocate texture_t wrapper */
	if (*t == nullptr)
	{
		texdata = (texture_t *)malloc(sizeof(texture_t));
		if (!texdata) { fclose(f); return false; }
		memset(texdata, 0, sizeof(texture_t));
	}
	else
	{
		texdata = (texture_t*)*t;
		if (texdata->initialized)
		{
			C3D_TexDelete(&texdata->tex);
			texdata->initialized = false;
		}
	}

	/* Tex3DS handles everything: format detection, decompression,
	 * tiling, and upload. Use linear memory (vram=false) since 6MB
	 * VRAM can't hold all HD textures at once. */
	t3x = Tex3DS_TextureImportStdio(f, &texdata->tex, nullptr, false);
	fclose(f);

	if (!t3x)
	{
		DebugPrintf("HD texture: Tex3DS import failed for %s\n", hd_path);
		if (*t == nullptr) free(texdata);
		return false;
	}

	/* Tex3DS imported the texture at whatever size the romfs pack
	 * shipped (512² for N3DS via hd_textures/, 256² for OG via
	 * hd_textures_lomem/ — picked at runtime by resolve_hd_textures_dirs).
	 * No further size munging needed. */

	/* Bilinear-mipmap (GPU_NEAREST for mip selection): picks ONE mip level
	 * per fragment, then bilinear-filters within it. Gives anti-moiré
	 * without the trilinear softness or the doubled per-fragment texture
	 * taps. If the t3x has no mip chain, GPU clamps to mip 0. */
	C3D_TexSetFilter(&texdata->tex, GPU_LINEAR, GPU_LINEAR);
	C3D_TexSetFilterMipmap(&texdata->tex, GPU_NEAREST);
	C3D_TexSetWrap(&texdata->tex, GPU_REPEAT, GPU_REPEAT);

	*width = texdata->tex.width;
	*height = texdata->tex.height;
	/* Match colorkey to the texture format. ETC1 has no alpha plane
	 * at all, so enabling alpha-test on it would just always pass.
	 * ETC1A4 carries real per-pixel alpha for sprites/grates. */
	*colorkey = (texdata->tex.fmt == GPU_ETC1A4);

	texdata->initialized   = true;
	texdata->material_class = classify_texture(path);
	texdata->proctex_phase  = proctex_phase_from_path(path);
	*t = (TEXTURE_HANDLE)texdata;

	Tex3DS_TextureFree(t3x);

	DebugPrintf("HD texture: loaded %s (%dx%d) class=%u\n",
		hd_path, texdata->tex.width, texdata->tex.height,
		(unsigned)texdata->material_class);
	return true;
}

static bool create_texture(TEXTURE_HANDLE *t, const char *path,
	u_int16_t *width, u_int16_t *height, int numMips, bool *colorkey)
{
	texture_image_t image;

	/* Try HD texture first (4K source → 512x512 ETC1A4 via tex3ds) */
	if (try_load_hd_texture(t, path, width, height, colorkey))
		return true;

	Change_Ext(path, image.path, ".PNG");
	if (!File_Exists((char*)image.path))
	{
		DebugPrintf("Could not find texture file: %s\n", path);
		return true;
	}

	if (load_image(&image, numMips) != 0)
		return false;

	/* PICA200 hard limit: 1024x1024 */
	while (image.w > 1024 || image.h > 1024)
	{
		int nw = image.w / 2, nh = image.h / 2;
		int x, y, c;
		char *nd = (char *)malloc(nw * nh * 4);
		if (!nd) break;
		for (y = 0; y < nh; y++)
			for (x = 0; x < nw; x++)
			{
				int si = (y * 2 * image.w + x * 2) * 4;
			for (c = 0; c < 4; c++)
				nd[(y * nw + x) * 4 + c] = (
					(u_int8_t)image.data[si + c] +
					(u_int8_t)image.data[si + 4 + c] +
					(u_int8_t)image.data[si + image.w * 4 + c] +
					(u_int8_t)image.data[si + image.w * 4 + 4 + c]
					) / 4;
		}
		free(image.data);
		image.data = nd;
		image.w = nw;
		image.h = nh;
	}

	*width = (u_int16_t)image.w;
	*height = (u_int16_t)image.h;
	*colorkey = (bool)image.colorkey;

	/* Gamma correction + colour key */
	{
		int y, x;
		int pitch = 4 * image.w;
		for (y = 0; y < image.h; y++)
			for (x = 0; x < image.w; x++)
			{
				DWORD idx = y * pitch + x * 4;
			/* Colourkey: pure black (0,0,0) → alpha=0 for transparency.
			 * Check BEFORE gamma because gamma_table clamps 0→1,
			 * turning (0,0,0) into (1,1,1) and breaking R+G+B==0. */
			bool is_colorkey = (((u_int8_t)image.data[idx] + (u_int8_t)image.data[idx+1] + (u_int8_t)image.data[idx+2]) == 0);
			image.data[idx]   = (char)gamma_table[(u_int8_t)image.data[idx]];
			image.data[idx+1] = (char)gamma_table[(u_int8_t)image.data[idx+1]];
			image.data[idx+2] = (char)gamma_table[(u_int8_t)image.data[idx+2]];
			if (is_colorkey)
				image.data[idx+3] = 0;
			else
				image.data[idx+3] = (char)gamma_table[(u_int8_t)image.data[idx+3]];
		}
	}

	{
		texture_t *texdata;
		u_int8_t *tiled;
		bool is_new = (*t == nullptr);

		if (is_new)
		{
			texdata = (texture_t *)malloc(sizeof(texture_t));
			if (!texdata) { destroy_image(&image); return false; }
			memset(texdata, 0, sizeof(texture_t));
		}
		else
		{
			texdata = (texture_t*)*t;
			if (texdata->initialized)
			{
				C3D_TexDelete(&texdata->tex);
				texdata->initialized = false;
			}
		}

		{
			char buf[128];
			snprintf(buf, sizeof(buf), "create_texture: C3D_TexInit %dx%d", image.w, image.h);
		}
		if (!C3D_TexInit(&texdata->tex, image.w, image.h, GPU_RGBA4))
		{
			texdata->initialized = false;
			*t = (TEXTURE_HANDLE)texdata;
			destroy_image(&image);
			return true; /* non-fatal — draw path falls back to vertex color */
		}

		/* Morton-tile RGBA8 → RGBA4444 into the texture's linear buffer */
		tiled = (u_int8_t *)linearAlloc(image.w * image.h * 2); /* 2 bytes per pixel for RGBA4 */
		if (tiled)
		{
			tile_rgba4((u_int8_t*)image.data, (u_int16_t*)tiled, image.w, image.h);
			C3D_TexUpload(&texdata->tex, tiled);
			C3D_TexFlush(&texdata->tex);
			linearFree(tiled);
		}

		C3D_TexSetFilter(&texdata->tex, GPU_LINEAR, GPU_LINEAR);
		C3D_TexSetWrap(&texdata->tex, GPU_REPEAT, GPU_REPEAT);

		texdata->initialized   = true;
		texdata->material_class = classify_texture(path);
		texdata->proctex_phase  = proctex_phase_from_path(path);
		*t = (TEXTURE_HANDLE)texdata;
	}

	DebugPrintf("Created texture: file=%s, width=%d, height=%d, colorkey=%s\n",
		image.path, image.w, image.h, image.colorkey ? "true" : "false");

	destroy_image(&image);
	return true;
}

/* A texture exists if create_texture could load it: an HD .t3x in either
 * pack, or the PNG it falls back to. The romfs ships no PNG for textures
 * that have a .t3x (Makefile.3ds), so tload's path choice must accept
 * either. */
bool FSTextureExists(const char *fileName)
{
	char path[256];
	char hd_path[512];
	FILE *f;

	Change_Ext(fileName, path, ".PNG");
	if (File_Exists(path))
		return true;
	f = open_hd_texture(fileName, hd_path, sizeof(hd_path));
	if (!f)
		return false;
	fclose(f);
	return true;
}

bool FSCreateTexture(TEXTURE_HANDLE *texture, const char *fileName,
	u_int16_t *width, u_int16_t *height, int numMips, bool *colourkey)
{
	return create_texture(texture, fileName, width, height, numMips, colourkey);
}

bool update_texture_from_file(TEXTURE_HANDLE dstTexture, const char *fileName,
	u_int16_t *width, u_int16_t *height, int numMips, bool *colorkey)
{
	create_texture(&dstTexture, fileName, width, height, numMips, colorkey);
	return true;
}
