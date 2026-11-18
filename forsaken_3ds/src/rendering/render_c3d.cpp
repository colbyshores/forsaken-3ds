/* render_c3d.cpp -- render.h for the 3DS, on citro3d (PICA200)
 *
 * The 3DS backend of the FS* / draw_* seam (render_vulkan.cpp is the PC one),
 * plus the c3d_* calls render.h declares under __3DS__. c3d_context_init runs
 * C3D_Init once, with a 16x (4 MB) command buffer. c3d_renderer_init, called
 * from render_init and idempotent, loads the vertex shader and creates the
 * render targets (left and right top-screen eyes, and the bottom screen,
 * where the HUD is drawn), the 4 MB linear vertex scratch, the ProcTex
 * wall-grain tables and the fragment-light setup for object shine.
 * FSBeginScene opens the frame on the left target and sets the GPU state
 * draws depend on; c3d_present ends it, and C3D_FrameEnd's display transfer
 * puts it on screen. c3d_transfer_eye moves the second stereo pass to the
 * right target.
 *
 * Engine vertex, index and normal buffers are linearAlloc memory holding
 * engine-format vertices. draw_render_object converts each texture group's
 * LVERTEX / TLVERTEX (packed colour) to gpu_vertex_t (36 bytes, float
 * colour) in the scratch and draws from there. Two caches skip that
 * conversion: the stereo memo lets the right eye resubmit what the left eye
 * converted (keyed by batch, so no draw is ever skipped), and in mono frames
 * level walls stay converted in retained linear buffers until a texture
 * animation step or c3d_release_level_cache.
 *
 * upload_matrices puts world_matrix in the shader's modelView uniform and
 * view x projection (with the PICA depth remap and the 90-degree screen
 * rotation) in projection, so the shader has world-space positions for its
 * per-vertex lighting: ambient plus up to GPU_MAX_LIGHTS point lights from
 * c3d_upload_xlights. Textures load as .t3x through Tex3DS from the HD pack
 * the memory tier selects, else from PNG converted to RGBA4444 and Morton
 * tiled. TexEnv modes: DIFFUSE (texture x vertex colour), AUX (walls, plus
 * ProcTex grain: "Wall detail"), OBJECT (models, plus fragment-light Phong
 * specular: "Object shine"). For full-screen cameras visi.cpp asks for
 * scissor mode: FSSetViewPort keeps the full viewport and scissors to the
 * group's rect, so adjacent groups share one projection.
 *
 * Hard-won rules: initialise GPU state (AttrInfo, TexEnv, depth, cull,
 * uniforms) explicitly, since unset registers draw nothing without
 * crashing; and every draw needs its own C3D_GetBufInfo (bind_vertex_buffer).
 */

#ifdef __3DS__

#include <3ds.h>
#include <citro3d.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include "game/main.h"
#include "math/new3d.h"
#include "math/quat.h"
#include "game/compobjects.h"
#include "game/bgobjects.h"
#include "game/object.h"
#include "net/networking.h"
#include "rendering/render.h"
#include "rendering/stereomemo.h"
#include "rendering/texture.h"
#include "data/file.h"
#include "platform_3ds/main_3ds.h"
#include "rendering/lights.h"
#include "util/util.h"
#include "util/platform.h"
#include "game/oct2.h"

/* Defined further down; used above their definitions. */
void c3d_renderer_cleanup(void);
void GetRealLightAmbientWorldSpace(VECTOR *Pos, float *R, float *G, float *B, float *A);
#include "util/platform.h"
#include "game/camera.h"

/* ---- shader binary (compiled from shaders/render_c3d.v.pica) ---- */
#include "rendering/render_c3d_shbin.h"

/* ---- uniform register locations (must match .v.pica) ---- */
constexpr int UNIFORM_PROJECTION = 0; /* fvec4[4] — view × proj, PICA-adjusted */
constexpr int UNIFORM_MODELVIEW = 4; /* fvec4[4] — world_matrix (model→world) */
constexpr int UNIFORM_AMBIENT = 8; /* fvec4    — .xyz=ambient, .w=unused */
constexpr int UNIFORM_LIGHT_COUNT = 9; /* fvec4    — .x=count(0..GPU_MAX_LIGHTS) */
#define UNIFORM_LIGHTS      10   /* fvec4[24]— GPU_MAX_LIGHTS × 3 fvec4 each
                                  *   light[3i+0] = (Pos.xyz, invSizeSq)
                                  *   light[3i+1] = (r, g, b, type)
                                  *   light[3i+2] = (Dir.xyz, CosArc)  */
constexpr int GPU_MAX_LIGHTS = 8; /* must match .v.pica `lights[24]` (= 8*3) */

/* ---- citro3d state ---- */

static DVLB_s          *s_shaderDVLB    = NULL;
static shaderProgram_s  s_shaderProgram;
static bool             s_shaderReady   = false;

/* PICA200 ProcTex — procedural texture unit, used as a per-class
 * detail-noise generator on wall draws. Free per-pixel modulation,
 * zero VRAM cost. Three preconfigured classes capture the dominant
 * material types in Forsaken's atlases:
 *
 *   MAT_METAL    — fine grit + brushed-metal scratches (default)
 *   MAT_ROCK     — coarser pitted noise for thermal / lava chambers
 *   MAT_ORGANIC  — softer grain for biological surfaces
 *
 * Each class has its own C3D_ProcTex (noise coefs + filter + clamp)
 * and C3D_ProcTexColorLut (tonal palette). Per-draw apply_texenv binds
 * the matching pair. Shared LUTs (noise smoothstep, RGBmap) bound
 * once at init.
 *
 * Replaces the per-texture _d.t3x detail-map pipeline — Forsaken's
 * painted source art has no high-frequency content to extract, so
 * derived detail maps were imperceptible. Synthesized noise is more
 * visible, identical RAM footprint on OG and N3DS, no asset pipeline. */
typedef enum {
	MAT_METAL    = 0,
	MAT_ROCK,
	MAT_ORGANIC,
	MAT_COUNT
} material_class_t;

static C3D_ProcTex          s_wallProcTex[MAT_COUNT];
static C3D_ProcTexColorLut  s_wallProcColor[MAT_COUNT];
static C3D_ProcTexLut       s_wallProcNoise;   /* shared smoothstep */
static C3D_ProcTexLut       s_wallProcMap;     /* shared RGB map */
static bool                 s_procTexReady = false;
static bool                 s_objProcTexOn = false;  /* per-draw toggle */

/* PICA200 fragment-lighting for object Phong shading. Vshader outputs
 * a pseudo-normal-derived quaternion (outnq) per vertex; the hardware
 * uses it + the bound light env to compute FRAGMENT_PRIMARY_COLOR
 * (diffuse) and FRAGMENT_SECONDARY_COLOR (specular). The OBJECT
 * TexEnv mode reads SECONDARY for the moving shine highlight. */
static C3D_LightEnv  s_objLightEnv;
static C3D_Light     s_objLight;
static C3D_LightLut  s_objLightLut;
static bool          s_objLightReady = false;

/* User-toggleable graphics features (persisted via title.cpp's config_*).
 * apply_texenv_for_texture reads these per draw: g_object_shine enables the
 * OBJECT mode (Phong specular on models), g_wall_detail the AUX mode
 * (ProcTex grain ADD_SIGNED onto walls). Both default true. */
bool g_object_shine = true;
bool g_wall_detail  = true;

static C3D_RenderTarget *s_targetLeft    = NULL;
static C3D_RenderTarget *s_targetRight   = NULL;
static C3D_RenderTarget *s_targetBottom  = NULL;  /* mono 320×240 for HUD */
static C3D_RenderTarget *s_targetCurrent = NULL;  /* active render target */
static C3D_RenderTarget *s_targetSaved   = NULL;  /* stashed target for HUD restore */
static bool s_hudDrawnThisFrame = false;          /* HUD drawn once per frame */
/* Saved render_info state during HUD-on-bottom pass (restored on End) */
static int  s_hudSavedThisModeW = 0;
static int  s_hudSavedThisModeH = 0;
static int  s_hudSavedWinCx     = 0;
static int  s_hudSavedWinCy     = 0;
static bool              s_stereoFrame   = false;

/* GPU lighting uniform registers, set at init to the hardcoded UNIFORM_*
 * values (libctru's uniform lookup returns -1 for these; see
 * c3d_renderer_init). */
static s8 s_loc_ambient     = -1;
static s8 s_loc_light_count = -1;
static s8 s_loc_lights      = -1;

/* Color write mask — always GPU_WRITE_ALL on citro3d (software anaglyph
 * removed; hardware stereo uses separate render targets instead). */
#define s_colorMask GPU_WRITE_ALL

/* ---- GPU vertex format (all floats, matches shader inputs) ---- */
typedef struct {
	float pos[3];
	float color[4];
	float texcoord[2];
} gpu_vertex_t;  /* 36 bytes */

/* ---- Single-pass stereo vertex memo (STEREO_REPLAY) ----
 *
 * Converted vertices are eye-independent (world-space pos, baked color,
 * raw UVs; lighting is shader-side), so the left eye's converted ranges
 * can be re-submitted for the right eye with the right eye's live
 * uniforms and draw order. The memo is keyed by stable batch identity
 * (renderObject + texgroup + ortho + prim), NOT by draw order, so
 * differing per-eye culling cannot drop geometry the way the old
 * whole-list replay did (missed inset/HUD draws on one eye). A miss
 * converts live — always safe, never stale. Table lives one frame
 * (generation counter in stereomemo.cpp); overflow of the scratch arena
 * disarms the memo for the rest of the frame (recorded offsets would
 * be recycled).
 *
 * What runs per eye is UNCHANGED: full traversal, culling, state
 * setup, uniforms, draw order, HUD. Only the vertex-conversion memcpy
 * is skipped on memo hits. Pixel-identical by construction. */
typedef enum {
	SMEMO_OFF = 0,    /* mono / default: memo untouched */
	SMEMO_RECORD,     /* left eye: convert live + record ranges */
	SMEMO_LOOKUP      /* right eye: reuse recorded ranges, convert misses */
} smemo_mode_t;
static smemo_mode_t s_memoMode = SMEMO_OFF;
/* c3d_memo_suspend: a nested camera pass (missile inset) runs inside an
 * eye pass and refills the same stamped fills from another camera, so
 * it must neither record over the eye's ranges nor reuse them. */
static smemo_mode_t s_memoSaved = SMEMO_OFF;
static bool s_memoSuspended = false;

/* Whether this renderObject's converted vertices may be recorded and
 * reused by the other eye. Stamped shared fills carry their content
 * identity; level walls and water are converted once per frame from
 * content that does not change between the eyes. Everything else
 * (models: one vertex buffer per model type, rewritten per on-screen
 * copy for pose, colour and UVs) converts live in each eye. */
static bool memo_reusable(const RENDEROBJECT *ro)
{
	if (ro->memoEpochPass == SMEMO_PASS_NO_REUSE)
		return false;
	if (ro->memoEpochPass != SMEMO_PASS_UNSET)
		return true;
	return ro->vertOwner == VERT_OWNER_LEVEL || ro->vertOwner == VERT_OWNER_WATER;
}

void c3d_memo_suspend(bool suspend)
{
	if (suspend) {
		if (!s_memoSuspended) {
			s_memoSaved = s_memoMode;
			s_memoMode = SMEMO_OFF;
			s_memoSuspended = true;
		}
	} else if (s_memoSuspended) {
		s_memoMode = s_memoSaved;
		s_memoSuspended = false;
	}
}

/* ---- Persistent level geometry ----
 * Level-wall (VERT_OWNER_LEVEL) texgroups are converted once into
 * retained linear buffers and resubmitted every frame without
 * reconversion or scratch traffic. Only execbufs whose content is
 * stable across frames qualify (memoEpochPass == SMEMO_PASS_UNSET;
 * XLight1Group marks white-out / underwater execbufs PER_FRAME).
 * Validity is keyed by smemo_persist_gen(): level texture-animation
 * steps bump it (reconvert into the same allocations). The buffers are
 * freed when their level is released (c3d_release_level_cache).
 * Allocation failure or table pressure falls back to the scratch
 * path — never stale data, never a skipped draw. */
constexpr int PERSIST_BITS = 11;
constexpr auto PERSIST_SIZE = (1u << PERSIST_BITS);
constexpr auto PERSIST_MASK = (PERSIST_SIZE - 1u);
/* Linear memory the cache always leaves free (see persist_store). */
constexpr auto PERSIST_LINEAR_RESERVE = (2u * 1024u * 1024u);
typedef struct {
	uintptr_t ro;
	int sub;
	gpu_vertex_t *buf;
	int cap;
	int count;
	unsigned gen;
} persist_entry_t;
static persist_entry_t s_persist[PERSIST_SIZE];
static unsigned s_persistBytes = 0;
/* Runtime gate (owner policy 2026-11-14): persist pays in mono
 * (-8.6% measured) and is wall-flat in stereo, where its stills are
 * only LSB-equivalent across eyes. So it is active only in mono frames
 * (slider down; see the stereo_frame gate in FSBeginScene), refreshed at
 * every FSBeginScene; stereo keeps the memo-only path.
 * Buffers stay allocated across slider moves and simply go unused
 * in stereo (generation unchanged, still valid if mono resumes). */
static int s_persistActive = 0;

static unsigned persist_hash(uintptr_t ro, int sub)
{
	uint64_t z = (uint64_t)ro * 0x9E3779B97F4A7C15ULL + (uint64_t)sub;
	z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
	z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
	{
		unsigned h = (z ^ (z >> 31)) & PERSIST_MASK;
		return h;
	}
}

static persist_entry_t *persist_find(const void *ro, int sub, int create)
{
	unsigned i = persist_hash((uintptr_t)ro, sub);
	unsigned n;
	for (n = 0; n < 32; n++) {
		persist_entry_t *e = &s_persist[(i + n) & PERSIST_MASK];
		if (e->ro == (uintptr_t)ro && e->sub == sub)
			return e;
		if (create && e->buf == NULL) {
			e->ro = (uintptr_t)ro;
			e->sub = sub;
			return e;
		}
	}
	return NULL;
}

static gpu_vertex_t *persist_lookup(const void *ro, int sub, int *count)
{
	persist_entry_t *e = persist_find(ro, sub, 0);
	if (e && e->buf && e->gen == smemo_persist_gen()) {
		*count = e->count;
		return e->buf;
	}
	return NULL;
}

/* Retain a freshly converted range. Returns the retained buffer or
 * NULL (pressure/alloc failure — caller submits from scratch). */
static gpu_vertex_t *persist_store(const void *ro, int sub,
                                   const gpu_vertex_t *src, int count)
{
	persist_entry_t *e = persist_find(ro, sub, 1);
	if (!e)
		return NULL;
	if (e->cap < count) {
		if (e->buf) {
			s_persistBytes -= e->cap * sizeof(gpu_vertex_t);
			linearFree(e->buf);
			e->buf = NULL;
			e->cap = 0;
		}
		/* Never let the cache take the linear memory a texture or
		 * vertex buffer needs: refuse (the group converts per frame as
		 * before) unless PERSIST_LINEAR_RESERVE stays free. Checked
		 * only when allocating, never on a refill or a hit. */
		if (linearSpaceFree() < count * sizeof(gpu_vertex_t) + PERSIST_LINEAR_RESERVE)
			return NULL;
		e->buf = (gpu_vertex_t *)linearAlloc(count * sizeof(gpu_vertex_t));
		if (!e->buf)
			return NULL;
		e->cap = count;
		s_persistBytes += count * sizeof(gpu_vertex_t);
	}
	memcpy(e->buf, src, count * sizeof(gpu_vertex_t));
	e->count = count;
	e->gen = smemo_persist_gen();
	return e->buf;
}

static void persist_free_all(void)
{
	unsigned i;
	for (i = 0; i < PERSIST_SIZE; i++) {
		if (s_persist[i].buf) {
			linearFree(s_persist[i].buf);
			s_persist[i].buf = NULL;
			s_persist[i].cap = 0;
			s_persist[i].count = 0;
			s_persist[i].gen = 0;
		}
	}
	s_persistBytes = 0;
}


/* Scratch buffer for vertex conversion.
 * 4MB = ~116000 verts at 36 bytes each.  Needs to handle heavy
 * firefight frames (many ships + projectiles + explosion particles +
 * laser beams + mipped BSP walls) without FrameSplit stalls or
 * silent geometry drops at the overflow-after-flush fallback.
 *
 * Same value on both the OG-tier and N3DS-tier paths of the unified
 * CIA. On OG (22 MB linear heap) this is ~18% of the budget — tight
 * but validated working on real hardware. */
constexpr auto GPU_SCRATCH_SIZE = (4 * 1024 * 1024);
static gpu_vertex_t *s_scratch = NULL;

/* Point the vertex attribute buffer at v for the next draw. citro3d
 * re-sends the buffer config only when C3D_GetBufInfo() has marked it
 * dirty, and the draw that sends it clears the mark (C3Di_UpdateContext),
 * so every draw needs its own C3D_GetBufInfo(): editing the struct after
 * a draw is silently ignored and the next draw reads the previous
 * buffer. That is what drew corrupt sprite quads in the right eye once
 * FLUSH_BATCH split a batch into several runs. */
static inline void bind_vertex_buffer(const gpu_vertex_t *v)
{
	C3D_BufInfo *bi = C3D_GetBufInfo();
	BufInfo_Init(bi);
	BufInfo_Add(bi, v, sizeof(gpu_vertex_t), 3, 0x210);
}
static int s_scratchUsed = 0;
static int s_scratchMax = 0;


/* Byte-to-float color table: replaces 4 vdiv.f32 per converted vertex
 * (measured 7.4 ms/frame on Old 3DS, 12.3K verts/frame) with 4 L1D
 * reads. Bit-identical output: each entry IS ((float)b)/255.0f, the same
 * correctly-rounded quotient the loop computed, so pixels are unchanged
 * (pixel-verified). 1 KiB, stays hot in L1D. */
static float s_c255[256];
static int s_c255_ok = 0;
static void c255_init(void)
{
	int b;
	if (s_c255_ok)
		return;
	for (b = 0; b < 256; b++)
		s_c255[b] = (float)b / 255.0f;
	s_c255_ok = 1;
}

/* ---- globals expected by the engine ---- */


typedef struct { float anisotropic; } gl_caps_t;
gl_caps_t caps;

/* Forward declarations */
static void upload_matrices(void);
typedef struct texture_s texture_t;
static void apply_texenv_for_texture(struct texture_s *texdata);
static void apply_texenv_untextured(void);

/* Pick a material class from the source texture filename. Forsaken's
 * atlas naming is consistent enough for prefix matching: thermal /
 * lava / nuclear chambers all use therm/nuke/lava/fire prefixes;
 * biological surfaces (Foetoid, the Flesh Morph, bio-organic walls)
 * use bio/foe/flsh; everything else is industrial-metal. Path can be
 * NULL in fallback paths — returns MAT_METAL. */
static u8 classify_texture(const char *path)
{
	const char *base;
	if (!path) return MAT_METAL;
	base = path;
	for (const char *p = path; *p; p++)
		if (*p == '/' || *p == '\\') base = p + 1;
	if (strstr(base, "therm") || strstr(base, "nuke") ||
	    strstr(base, "lava")  || strstr(base, "fire") ||
	    strstr(base, "rock")  || strstr(base, "stone"))
		return MAT_ROCK;
	if (strstr(base, "bio")   || strstr(base, "foe")  ||
	    strstr(base, "flsh")  || strstr(base, "guts"))
		return MAT_ORGANIC;
	return MAT_METAL;
}

/* Deterministic phase offset derived from the texture path. Two
 * different textures of the same material class get distinct noise
 * tile positions; the same texture always gets the same phase across
 * level loads (no flicker on respawn). FNV-1a 32-bit, scaled into
 * [0,1) for the ProcTex phase argument. */
static float proctex_phase_from_path(const char *path)
{
	u32 h = 0x811C9DC5u;
	if (path)
		for (const char *p = path; *p; p++)
			h = (h ^ (u8)*p) * 0x01000193u;
	return (float)(h & 0xFFFFu) / 65536.0f;
}

MATRIX proj_matrix;
MATRIX view_matrix;
MATRIX world_matrix;

/* Texture handle — wraps a C3D_Tex for the engine's LPTEXTURE (void*).
 * material_class is set at load time by classify_texture() based on
 * the source filename; consumed by configure_texenv_aux to pick which
 * ProcTex preset to bind. proctex_phase is a deterministic hash of
 * the texture path that's used as the noise phase offset, so two
 * walls of the same material class don't share the same noise tile
 * position — but each individual texture's pattern is identical from
 * one level load to the next (deterministic, no flicker on respawn). */
typedef struct texture_s {
	C3D_Tex tex;
	bool    initialized;
	u8      material_class;
	float   proctex_phase;
} texture_t;

/* bSquareOnly — shared global, always true on 3DS */
bool bSquareOnly = true;

/* Additive blend flag (matches GL1 behavior) */
bool _additive_blend_active = false;

/* Frame state tracking */
static bool s_inFrame = false;

/* TexEnv cache: skip redundant GPU state changes when consecutive
 * draws use the same texture or same untextured mode. */
static void *s_lastBoundTexture = NULL;  /* last texture_t* passed to C3D_TexBind */
static bool  s_lastTexEnvTextured = false;  /* last TexEnv was textured vs vertex-only */

/* Per-execbuf ProcTex phase variation. draw_render_object writes a
 * per-renderObject seed; apply_texenv_for_texture reads it to pick a
 * phase bucket so two adjacent walls sharing the same base texture
 * get visually distinct grain. Folded into g_wall_detail — there's no
 * scenario where you'd want detail-grain on but the repeating version. */
static u32  s_atlasSeed        = 0;
static int  s_lastMatClass     = -1;
static int  s_lastPhaseBucket  = -1;
constexpr int ATLAS_PHASE_BUCKETS = 64;

/* True while inside an orthographic draw_render_object (HUD, screen
 * polys, damage-flash overlay). apply_texenv_for_texture suppresses
 * AUX-mode ProcTex for those — wall grain on a fullscreen red damage
 * tint reads as garbage, same shape for HUD glyphs and menu icons. */
static bool s_isOrthoDraw      = false;

/* Sticky flag: caller-managed. Wraps world-space billboard / particle
 * batches (explosion sprites, smoke puffs, lens flares, weapon trails)
 * so apply_texenv_for_texture skips AUX-mode ProcTex on them too. The
 * orthographic auto-flag covers HUD/screen-poly draws; this covers
 * 3D-space sprites that *aren't* orthographic but still shouldn't get
 * wall grain. */
static bool s_skipAuxProcTex   = false;

/* Track the last TexEnv mode so the combiner stages are reconfigured
 * only when a draw needs a different mode. */
typedef enum {
	TEXENV_MODE_NONE = 0,    /* untextured / vertex-only */
	TEXENV_MODE_DIFFUSE,     /* single-stage diffuse * vertex */
	TEXENV_MODE_AUX,         /* wall: diffuse + ADD_SIGNED ProcTex grain */
	TEXENV_MODE_OBJECT,      /* model: diffuse + Phong specular */
} texenv_mode_t;
static texenv_mode_t s_lastTexEnvMode = TEXENV_MODE_NONE;

/* Toggle: enable Phong-shine path for the next textured draw. Set by
 * mxload.cpp / mxaload.cpp around their draw_object calls so model + enemy
 * meshes get the specular highlight. (A misnomer: the OBJECT mode no
 * longer uses ProcTex.) */
void c3d_set_object_proctex(bool enable)
{
	s_objProcTexOn = enable;
}

/* Per-call eligibility gate for the OBJECT shine path. Defaults true
 * (every object draw gets shine). Set false by callers that want to
 * suppress shine for a specific model — currently used by models.cpp
 * for OWNER_BGOBJECT models (doors, vents, animated decorations)
 * where Phong specular reads as tacky on the large flat surfaces. */
static bool s_objShineEligible = true;
void c3d_set_object_shine_eligible(bool enable)
{
	s_objShineEligible = enable;
}


/* Context lifetime, frame present and stereo eye switch: the c3d_* calls
 * main_3ds.cpp and oct2.cpp make. */
void c3d_context_init(void)
{
	/* Idempotent: paired with c3d_renderer_init's own idempotency. If
	 * we let C3D_Init re-fire on a render_mode_select retrigger, it
	 * resets citro3d's context state — but the render targets we
	 * created in the previous c3d_renderer_init are tied to the OLD
	 * context. They become orphaned pointers with stale GPU bookkeeping
	 * and subsequent draws end up in undefined state. The portal
	 * "black void" regression on the autotest sweep traced to this:
	 * primary geometry rendered fine but through-portal groups landed
	 * on a target citro3d no longer treated as live. */
	static bool _c3d_context_inited = false;
	if (_c3d_context_inited) return;
	_c3d_context_inited = true;

	/* 16× the default GPU command buffer (256 KB → 4 MB), taken from
	 * the linear heap. A full command buffer trips GPUCMD_AddInternal's
	 * svcBreak. It was raised from 1 MB for a multi-pass flood-fill in
	 * visi.cpp FindVisible that has since been removed (the level
	 * extractor now repairs the visibility data); the size has not been
	 * re-measured since. */
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE * 16);
}

void c3d_context_exit(void)
{
	c3d_renderer_cleanup();
	C3D_Fini();
}

/* Frame-rate counter. Updated once per swap, exposed via the
 * `forsaken_get_fps()` accessor so the HUD overlay can draw it. */
static u32    s_fps_frames_in_win = 0;
static u64    s_fps_window_start_ms = 0;
static int    s_fps_current       = 0;

int forsaken_get_fps(void) { return s_fps_current; }

void c3d_present(void)
{
	if (s_inFrame)
	{
		C3D_FrameEnd(0);
		s_inFrame = false;
	}
	/* Reset stereo state for next frame */
	s_stereoFrame = false;
	s_memoMode = SMEMO_OFF;
	s_hudDrawnThisFrame = false;

	/* FPS measurement. Roll the count over a 1-second window so the
	 * displayed value is stable. */
	u64 now = osGetTime();
	if (s_fps_window_start_ms == 0)
		s_fps_window_start_ms = now;
	s_fps_frames_in_win++;
	u64 elapsed = now - s_fps_window_start_ms;
	if (elapsed >= 1000)
	{
		/* @vfp-exempt R11 — FPS counter window update, once per measurement window (~1/s), not per frame. measured n/a. Verified 2026-10-25. */
		s_fps_current = (int)((s_fps_frames_in_win * 1000ULL) / elapsed);
		s_fps_window_start_ms = now;
		s_fps_frames_in_win = 0;
	}
}

/*===================================================================
	HUD bottom-screen redirection
	-------------------------------------------------------------------
	The gameplay HUD (ammo / weapons / shield / messages) is rendered
	on the mono bottom screen instead of overlaying the stereo top
	screen. c3d_hud_begin_bottom() stashes the current top-screen target,
	clears the bottom target, switches C3D's draw target to it, and
	returns true if the caller should actually issue draws. It returns
	false on the second eye's pass so we only draw the HUD once per
	frame (bottom is mono — one copy is all we need).
===================================================================*/
bool c3d_hud_begin_bottom(void)
{
	if (!s_targetBottom || !s_inFrame)
		return false;
	if (s_hudDrawnThisFrame)
		return false;
	s_hudDrawnThisFrame = true;

	/* Save + override dimensions so Print4x5Text / 2D layout math
	   targets 320×240 instead of 400×240. */
	s_hudSavedThisModeW = (int)render_info.ThisMode.w;
	s_hudSavedThisModeH = (int)render_info.ThisMode.h;
	s_hudSavedWinCx     = (int)render_info.window_size.cx;
	s_hudSavedWinCy     = (int)render_info.window_size.cy;
	render_info.ThisMode.w    = 320;
	render_info.ThisMode.h    = 240;
	render_info.window_size.cx = 320;
	render_info.window_size.cy = 240;

	s_targetSaved = s_targetCurrent;
	C3D_RenderTargetClear(s_targetBottom, C3D_CLEAR_ALL, 0x000000FF, 0xFFFFFF);
	C3D_FrameDrawOn(s_targetBottom);
	s_targetCurrent = s_targetBottom;
	/* PICA portrait viewport for the 240×320 bottom target. */
	C3D_SetViewport(0, 0, 240, 320);

	/* Re-bind shader state for the new target (matches c3d_transfer_eye). */
	C3D_BindProgram(&s_shaderProgram);
	{
		C3D_AttrInfo *ai = C3D_GetAttrInfo();
		AttrInfo_Init(ai);
		AttrInfo_AddLoader(ai, 0, GPU_FLOAT, 3);
		AttrInfo_AddLoader(ai, 1, GPU_FLOAT, 4);
		AttrInfo_AddLoader(ai, 2, GPU_FLOAT, 2);
	}
	C3D_DepthTest(true, GPU_LESS, s_colorMask);
	C3D_CullFace(GPU_CULL_FRONT_CCW);
	C3D_DepthMap(true, 1.0f, 1.0f);
	return true;
}

/* Wipe the bottom-screen render target via the same GPU-side pattern
 * used in render_init().  Call this when leaving gameplay for the
 * title screen — without it, the gameplay HUD's last frame stays
 * frozen on the bottom screen until the next gameplay session draws
 * over it (the title screen never touches the bottom target).
 *
 * Safe to call between frames; if the engine is in an open frame,
 * close it first so the C3D_FrameBegin below doesn't conflict. */
void c3d_clear_bottom_screen(void)
{
	if (!s_targetBottom)
		return;

	if (s_inFrame)
	{
		C3D_FrameEnd(0);
		s_inFrame = false;
	}

	int i;
	for (i = 0; i < 3; i++)
	{
		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
		C3D_RenderTargetClear(s_targetBottom, C3D_CLEAR_ALL, 0x000000FF, 0xFFFFFF);
		C3D_FrameDrawOn(s_targetBottom);
		C3D_FrameEnd(0);
	}

	s_targetCurrent = NULL;
	s_hudDrawnThisFrame = false;
}

/* Level release: the persistent cache's buffers belong to the level
 * being released. Wait until the GPU has finished every queued draw
 * that may still read them (an empty SYNCDRAW frame waits for the
 * queue, as c3d_clear_bottom_screen does), then free them, so the next
 * level loads with that linear memory back. */
void c3d_release_level_cache(void)
{
	if (s_persistBytes == 0)
		return;
	if (s_inFrame)
	{
		C3D_FrameEnd(0);
		s_inFrame = false;
	}
	C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
	C3D_FrameEnd(0);
	persist_free_all();
}

void c3d_hud_end_bottom(void)
{
	if (!s_targetSaved)
		return;
	C3D_FrameDrawOn(s_targetSaved);
	s_targetCurrent = s_targetSaved;
	s_targetSaved = NULL;

	/* Restore saved dimensions. */
	render_info.ThisMode.w     = s_hudSavedThisModeW;
	render_info.ThisMode.h     = s_hudSavedThisModeH;
	render_info.window_size.cx = s_hudSavedWinCx;
	render_info.window_size.cy = s_hudSavedWinCy;

	/* Restore top-screen PICA portrait viewport (240×400). */
	C3D_SetViewport(0, 0, 240, 400);

	/* Re-bind shader state for the restored top target. */
	C3D_BindProgram(&s_shaderProgram);
	{
		C3D_AttrInfo *ai = C3D_GetAttrInfo();
		AttrInfo_Init(ai);
		AttrInfo_AddLoader(ai, 0, GPU_FLOAT, 3);
		AttrInfo_AddLoader(ai, 1, GPU_FLOAT, 4);
		AttrInfo_AddLoader(ai, 2, GPU_FLOAT, 2);
	}
	C3D_DepthTest(true, GPU_LESS, s_colorMask);
	C3D_CullFace(GPU_CULL_FRONT_CCW);
	C3D_DepthMap(true, 1.0f, 1.0f);
}

void c3d_transfer_eye(unsigned int eye)
{
	if (eye == GFX_LEFT)
	{
		/* Left eye finished. Switch the memo from record to lookup:
		 * the right eye reuses recorded ranges with its own live
		 * uniforms and draw order. */
		if (s_memoMode == SMEMO_RECORD)
			s_memoMode = SMEMO_LOOKUP;

		/* Switch to right eye target */
		C3D_RenderTargetClear(s_targetRight, C3D_CLEAR_ALL, 0x000000FF, 0xFFFFFF);
		C3D_FrameDrawOn(s_targetRight);
		s_targetCurrent = s_targetRight;

		/* Re-initialize GPU state for the new render target */
		C3D_BindProgram(&s_shaderProgram);
		{
			C3D_AttrInfo *ai = C3D_GetAttrInfo();
			AttrInfo_Init(ai);
			AttrInfo_AddLoader(ai, 0, GPU_FLOAT, 3);
			AttrInfo_AddLoader(ai, 1, GPU_FLOAT, 4);
			AttrInfo_AddLoader(ai, 2, GPU_FLOAT, 2);
		}
		C3D_DepthTest(true, GPU_LESS, s_colorMask);
		C3D_CullFace(GPU_CULL_FRONT_CCW);
		C3D_DepthMap(true, 1.0f, 1.0f);

		/* No trigger needed: the memo is keyed per batch, so every
		 * right-eye draw looks its own range up (hit = resubmit,
		 * miss = convert live). No draw is ever skipped, so inset,
		 * HUD and post-stereo draws are unaffected by construction.
		 * Default (flag off): s_memoMode stays OFF, both eyes run
		 * fully, exactly as before. */
	}
	if (eye == GFX_RIGHT)
	{
		/* Right eye done. Disarm the memo; post-stereo rendering
		 * (missile camera, rear view, HUD overlays) draws normally. */
		s_memoMode = SMEMO_OFF;
	}
	s_stereoFrame = true;
}

/* Gamma table for texture loading */
u_int8_t gamma_table[256];

/*===================================================================
	Gamma table (shared with texture loader)
===================================================================*/

/* @vfp-exempt R6 — one-shot gamma table build at init, not a per-frame path;
   pow output IS the table. measured n/a. Verified 2026-10-25. */
void build_gamma_table(double gamma)
{
	int i;
	double k;
	if (gamma <= 0.0) gamma = 1.0;
	k = 255.0 / pow(255.0, 1.0 / gamma);
	for (i = 0; i < 256; i++)
	{
		gamma_table[i] = (u_int8_t)(k * (pow((double)i, 1.0 / gamma)));
		if (gamma_table[i] > 255)
			gamma_table[i] = 255;
		if (gamma_table[i] < 1)
			gamma_table[i] = 1;
	}
}

/*===================================================================
	Shader init / cleanup
===================================================================*/

bool c3d_renderer_init(void)
{
	/* Idempotent: subsequent calls (engine triggers render_mode_select on
	 * stereo toggles, mode changes, fullscreen flips, etc.) would otherwise
	 * leak ~5MB of linear memory each time — shader DVLB, 4MB scratch, three
	 * render targets — because render_cleanup is a no-op. After ~5 calls
	 * linear heap is exhausted and FSCreateIndexBuffer starts returning NULL.
	 * Diagnosed via 246 "render targets created" lines in forsaken_c3d.log
	 * paired with 10+ "FSCreateIndexBuffer: linearAlloc FAILED" entries near
	 * EOF, immediately preceding GPUCMD_AddInternal's full-buffer svcBreak. */
	if (s_shaderReady) {
		return true;
	}
	s_shaderDVLB = DVLB_ParseFile((u32*)render_c3d_shbin, render_c3d_shbin_len);
	if (!s_shaderDVLB)
	{
		return false;
	}

	shaderProgramInit(&s_shaderProgram);
	if (shaderProgramSetVsh(&s_shaderProgram, &s_shaderDVLB->DVLE[0]) < 0) {
		return false;
	}

	/* Allocate scratch buffer for vertex conversion */
	s_scratch = (gpu_vertex_t*)linearAlloc(GPU_SCRATCH_SIZE);
	if (!s_scratch) {
		return false;
	}
	s_scratchMax = GPU_SCRATCH_SIZE / sizeof(gpu_vertex_t);
	s_scratchUsed = 0;

	/* Create render targets and link to display output.
	 * Both left and right eye targets are created unconditionally —
	 * the right target is only drawn to when stereo is active. */
	{
		u32 transferFlags =
			GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) |
			GX_TRANSFER_RAW_COPY(0) |
			GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) |
			GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) |
			GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO);

		s_targetLeft = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
		if (!s_targetLeft) {
			return false;
		}
		C3D_RenderTargetSetOutput(s_targetLeft, GFX_TOP, GFX_LEFT, transferFlags);

		s_targetRight = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
		if (!s_targetRight) {
			return false;
		}
		C3D_RenderTargetSetOutput(s_targetRight, GFX_TOP, GFX_RIGHT, transferFlags);

		/* Bottom screen (320×240, mono). Stored in GPU as 240×320 portrait
		 * like the top. Used for gameplay HUD text so the stereo 3D image
		 * above isn't cluttered. */
		s_targetBottom = C3D_RenderTargetCreate(240, 320, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
		if (!s_targetBottom) {
			return false;
		}
		C3D_RenderTargetSetOutput(s_targetBottom, GFX_BOTTOM, GFX_LEFT, transferFlags);
	}

	/* Match picaGL depth map: scale=1.0, offset=1.0 */
	C3D_DepthMap(true, 1.0f, 1.0f);

	/* GPU-lighting uniform register IDs are hardcoded to match the
	 * declaration order in shaders/render_c3d.v.pica. libctru's
	 * shaderInstanceGetUniformLocation / DVLE_GetUniformRegister return
	 * -1 for fvec uniforms beyond projection/modelView even though they
	 * appear in the binary's symbol table — works around that. */
	s_loc_ambient     = UNIFORM_AMBIENT;
	s_loc_light_count = UNIFORM_LIGHT_COUNT;
	s_loc_lights      = UNIFORM_LIGHTS;

	if (!s_procTexReady)
	{
		/* Shared LUTs — noise smoothstep + RGB map. Both are linear
		 * identity ramps; per-class character comes from the noise
		 * coefficients and the color palette LUT, not these. Bound
		 * once at init; PICA200 keeps them globally until rebound. */
		{
			float ramp[129];
			int i;
			for (i = 0; i <= 128; i++)
				ramp[i] = (float)i / 128.0f;
			ProcTexLut_FromArray(&s_wallProcNoise, ramp);
			C3D_ProcTexLutBind(GPU_LUT_NOISE, &s_wallProcNoise);
			ProcTexLut_FromArray(&s_wallProcMap, ramp);
			C3D_ProcTexLutBind(GPU_LUT_RGBMAP, &s_wallProcMap);
		}

		/* Per-class noise coefs + color LUT. Color LUT uses 2 stops:
		 * dark stop at the low-noise end, light stop at the high-noise
		 * end. Both straddle grey 0.5 so ADD_SIGNED contributes signed
		 * symmetric per-pixel modulation. */

		/* The diffuse texture stays the dominant signal; ProcTex is a
		 * faint per-class accent that gives each material kind a hint
		 * of unique surface character. All three classes use the same
		 * narrow tonal band centred slightly below grey 0.5 so the
		 * ADD_SIGNED contribution biases toward a subtle darkening
		 * (reads as grime / wear, not as wash-out). Class differences
		 * are driven by noise frequency (pattern scale) and palette
		 * tint, both kept gentle. Magnitudes:
		 *   palette 0x70..0x82 = 0.439..0.510 → ADD_SIGNED: -6%..+1%
		 *   palette 0x70..0x8C = 0.439..0.549 → ADD_SIGNED: -6%..+5%
		 * (Ranges asymmetric on purpose — slightly more dark stops
		 * than bright, leaning the look toward "lived-in".) */

		/* All three classes use GPU_PT_RGB combiner — looks up the
		 * color LUT independently on U and V noise channels rather
		 * than via the radial sqrt(U²+V²) metric, which previously
		 * produced visible elliptical / oval blob artifacts at low
		 * frequency. RGB combiner gives uniform per-pixel speckle.
		 * Frequencies bumped to 8-12 cycles per UV face so noise is
		 * fine grain, not big blobs. NoiseCoefs phase is rewritten
		 * per draw with the texture's hash so two walls of the same
		 * material class get distinct pattern positions.
		 *
		 * Palettes still narrow (ADD_SIGNED contribution ~+/-5%) and
		 * biased slightly below grey 0.5 so noise reads as wear/
		 * grime, not wash-out. Diffuse stays the dominant signal. */

		/* "Lived in" wear pass. Palettes widened from ±~5% to ±~12%
		 * around grey 0.5 — symmetric so the ProcTex contributes equal
		 * darkening (scuffs / dirt) and brightening (highlights / wear
		 * polish) rather than only darkening (which read as fog).
		 * Frequencies bumped 30% so features are finer and more
		 * defined. Amplitudes bumped ~30% so the modulation actually
		 * registers visually rather than fading into a wash. */

		/* 4-stop stepped palettes break up the bilinear color blend
		 * that produces splotches with 2-stop ramps. The output as
		 * noise sweeps 0→1 oscillates dark→mid→light→mid instead of
		 * smoothly fading dark→light, which reads as small bumps and
		 * wear features rather than large flat blobs. Each stop stays
		 * within ±6% of grey 0.5 (ADD_SIGNED) so contribution is
		 * subtle and symmetric. */

		/* MAT_METAL — fine paint-grain + brushed sheen. */
		C3D_ProcTexInit(&s_wallProcTex[MAT_METAL], 0, 2);
		C3D_ProcTexClamp(&s_wallProcTex[MAT_METAL],
		                 GPU_PT_REPEAT, GPU_PT_REPEAT);
		C3D_ProcTexNoiseCoefs(&s_wallProcTex[MAT_METAL],
		                      C3D_ProcTex_UV, 0.25f, 16.0f, 0.0f);
		C3D_ProcTexCombiner(&s_wallProcTex[MAT_METAL], false,
		                    GPU_PT_ADD, GPU_PT_ADD);
		C3D_ProcTexFilter(&s_wallProcTex[MAT_METAL], GPU_PT_LINEAR);
		C3D_ProcTexNoiseEnable(&s_wallProcTex[MAT_METAL], true);
		{
			u32 colors[4] = {
				0xFF727272u,  /*  -7% */
				0xFF909090u,  /*  +6% */
				0xFF787878u,  /*  -3% */
				0xFF8C8C8Cu,  /*  +5% */
			};
			ProcTexColorLut_Write(&s_wallProcColor[MAT_METAL],
			                      colors, 0, 4);
		}

		/* MAT_ROCK — pitted, cool/warm wear pattern. */
		C3D_ProcTexInit(&s_wallProcTex[MAT_ROCK], 0, 2);
		C3D_ProcTexClamp(&s_wallProcTex[MAT_ROCK],
		                 GPU_PT_REPEAT, GPU_PT_REPEAT);
		C3D_ProcTexNoiseCoefs(&s_wallProcTex[MAT_ROCK],
		                      C3D_ProcTex_UV, 0.31f, 13.0f, 0.0f);
		C3D_ProcTexCombiner(&s_wallProcTex[MAT_ROCK], false,
		                    GPU_PT_ADD, GPU_PT_ADD);
		C3D_ProcTexFilter(&s_wallProcTex[MAT_ROCK], GPU_PT_LINEAR);
		C3D_ProcTexNoiseEnable(&s_wallProcTex[MAT_ROCK], true);
		{
			u32 colors[4] = {
				0xFF70757Cu,  /* cool dark */
				0xFF938D86u,  /* warm light */
				0xFF767A80u,  /* cool mid */
				0xFF8E8783u,  /* warm mid-light */
			};
			ProcTexColorLut_Write(&s_wallProcColor[MAT_ROCK],
			                      colors, 0, 4);
		}

		/* MAT_ORGANIC — soft, faint green tint. */
		C3D_ProcTexInit(&s_wallProcTex[MAT_ORGANIC], 0, 2);
		C3D_ProcTexClamp(&s_wallProcTex[MAT_ORGANIC],
		                 GPU_PT_REPEAT, GPU_PT_REPEAT);
		C3D_ProcTexNoiseCoefs(&s_wallProcTex[MAT_ORGANIC],
		                      C3D_ProcTex_UV, 0.27f, 14.0f, 0.0f);
		C3D_ProcTexCombiner(&s_wallProcTex[MAT_ORGANIC], false,
		                    GPU_PT_ADD, GPU_PT_ADD);
		C3D_ProcTexFilter(&s_wallProcTex[MAT_ORGANIC], GPU_PT_LINEAR);
		C3D_ProcTexNoiseEnable(&s_wallProcTex[MAT_ORGANIC], true);
		{
			u32 colors[4] = {
				0xFF72766Eu,
				0xFF90948Cu,
				0xFF787C76u,
				0xFF8C9088u,
			};
			ProcTexColorLut_Write(&s_wallProcColor[MAT_ORGANIC],
			                      colors, 0, 4);
		}

		s_procTexReady = true;
	}

	if (!s_objLightReady)
	{
		C3D_LightEnvInit(&s_objLightEnv);

		/* Phong-30 LUT — tight specular highlight so the shine reads
		 * as a moving spot rather than a broad glow. */
		LightLut_Phong(&s_objLightLut, 30.0f);
		C3D_LightEnvLut(&s_objLightEnv, GPU_LUT_D0, GPU_LUTINPUT_LN,
		                false, &s_objLightLut);

		/* Material — strong specular, modest diffuse. We don't sample
		 * FRAGMENT_PRIMARY_COLOR (diffuse stays in the legacy TEX0 ×
		 * PRIMARY path), only FRAGMENT_SECONDARY_COLOR (specular). */
		{
			static const C3D_Material s_objMaterial = {
				{ 0.0f, 0.0f, 0.0f },  /* ambient — unused */
				{ 0.0f, 0.0f, 0.0f },  /* diffuse — unused */
				{ 0.6f, 0.6f, 0.6f },  /* specular0 — moving shine */
				{ 0.0f, 0.0f, 0.0f },  /* specular1 */
				{ 0.0f, 0.0f, 0.0f },  /* emission */
			};
			C3D_LightEnvMaterial(&s_objLightEnv, &s_objMaterial);
		}

		C3D_LightInit(&s_objLight, &s_objLightEnv);
		C3D_LightColor(&s_objLight, 1.0f, 1.0f, 1.0f);

		/* Positional light far up + forward — same convention as the
		 * earlier wall light. Distance falloff is dot-based via the
		 * LUT, not range-based, so absolute coords don't matter much. */
		{
			C3D_FVec pos = FVec4_New(500.0f, 500.0f, 200.0f, 1.0f);
			C3D_LightPosition(&s_objLight, &pos);
		}

		s_objLightReady = true;
	}

	s_shaderReady = true;
	return true;
}

void c3d_renderer_cleanup(void)
{
	if (s_shaderReady)
	{
		shaderProgramFree(&s_shaderProgram);
		s_shaderReady = false;
	}
	if (s_shaderDVLB)
	{
		DVLB_Free(s_shaderDVLB);
		s_shaderDVLB = NULL;
	}
}

/*===================================================================
	Matrix conversion: engine RENDERMATRIX → C3D_Mtx
===================================================================*/

static void matrix_to_c3d(const RENDERMATRIX *src, C3D_Mtx *dst)
{
	/* Engine matrices are column-major in memory: the raw float[16] is
	 * the column-major data the old OpenGL backend uploaded as is.
	 * Load it the same way picaGL's glLoadMatrixf does:
	 *   row[i].x = m[0+i]   (column 0, row i)
	 *   row[i].y = m[4+i]   (column 1, row i)
	 *   row[i].z = m[8+i]   (column 2, row i)
	 *   row[i].w = m[12+i]  (column 3, row i)
	 * C3D_FVec x/y/z/w accessors handle the WZYX storage internally. */
	const float *m = (const float *)src;
	int i;
	for (i = 0; i < 4; i++)
	{
		dst->r[i].x = m[0 + i];
		dst->r[i].y = m[4 + i];
		dst->r[i].z = m[8 + i];
		dst->r[i].w = m[12 + i];
	}
}

static void upload_matrices(void)
{
	/* Split the combined MVP into:
	 *   modelView slot = world_matrix       (model→world transform)
	 *   projection slot = view × proj        (camera + projection,
	 *                                          PICA depth/rotation tweaks)
	 *
	 * Shader composes them as `outpos = projection * (modelView * inpos)`,
	 * which algebraically equals `MVP * inpos` so existing draws are
	 * pixel-identical. The split lets the shader read the world-space
	 * vertex position out of `modelView * inpos` for lighting math —
	 * required for models, where `inpos` is in MODEL space and a single
	 * combined-MVP shader can't compute distance to world-space lights.
	 *
	 * The PICA row tweaks (depth remap, 90° rotation) commute with
	 * right-multiplication by world_matrix, so applying them only to the
	 * view×proj half is mathematically equivalent to applying them to
	 * the full MVP. */
	MATRIX vp;
	C3D_Mtx c3d_vp, c3d_world;

	MatrixMultiply(&view_matrix, &proj_matrix, &vp);
	matrix_to_c3d((const RENDERMATRIX*)&vp, &c3d_vp);

	/* PICA depth remap: D3D [0,1] → PICA [-1,0] */
	c3d_vp.r[2].x -= c3d_vp.r[3].x;
	c3d_vp.r[2].y -= c3d_vp.r[3].y;
	c3d_vp.r[2].z -= c3d_vp.r[3].z;
	c3d_vp.r[2].w -= c3d_vp.r[3].w;

	/* 90° screen rotation */
	{
		C3D_FVec row0 = c3d_vp.r[0];
		c3d_vp.r[0] = c3d_vp.r[1];
		c3d_vp.r[1].x = -row0.x;
		c3d_vp.r[1].y = -row0.y;
		c3d_vp.r[1].z = -row0.z;
		c3d_vp.r[1].w = -row0.w;
	}

	matrix_to_c3d((const RENDERMATRIX*)&world_matrix, &c3d_world);

	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, UNIFORM_PROJECTION, &c3d_vp);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, UNIFORM_MODELVIEW,  &c3d_world);
}

/*===================================================================
	Hybrid GPU vertex lighting — point lights, level mesh.

	The vertex shader sums ambient + per-light distance falloff for up
	to GPU_MAX_LIGHTS lights, replacing the per-vertex CPU iteration in
	lights.cpp for the level mesh (XLight1Group) and for models (the
	model paths only restore the baked colours and leave the lights to
	the shader). SPOT lights are treated as points (no cone math) —
	visually negligible for Forsaken's transient blaster/RT lights and
	keeps the shader simple.

	See shaders/render_c3d.v.pica for the shader side.
===================================================================*/

/* Ambient contribution. Zero matches the engine's XLight1Group baseline
 * (no global ambient term). Tunable per art direction. */
static float s_ambient_r = 0.0f;
static float s_ambient_g = 0.0f;
static float s_ambient_b = 0.0f;

/* Zero all lighting uniforms. Called from FSBeginScene so each frame
 * starts clean — without this, UI / crate-menu / title-screen frames
 * inherit the previous gameplay frame's lights as stale shading on
 * menu meshes. The gameplay path repopulates via c3d_upload_xlights. */
void c3d_clear_xlights(void)
{
	if (s_loc_lights < 0) return;
	if (s_loc_ambient >= 0)
		C3D_FVUnifSet(GPU_VERTEX_SHADER, s_loc_ambient, 0.0f, 0.0f, 0.0f, 0.0f);
	if (s_loc_light_count >= 0)
		C3D_FVUnifSet(GPU_VERTEX_SHADER, s_loc_light_count, 0.0f, 0.0f, 0.0f, 0.0f);
	C3D_FVec *dst = C3D_FVUnifWritePtr(GPU_VERTEX_SHADER, s_loc_lights, GPU_MAX_LIGHTS * 3);
	memset(dst, 0, sizeof(C3D_FVec) * GPU_MAX_LIGHTS * 3);
}

/*  Pack the closest GPU_MAX_LIGHTS visible XLights into the shader
 *  uniform. Distance ranking prevents on/off flicker on the blaster's
 *  own XLight in dense scenes (>8 visible) where naive first-N
 *  selection depends on linked-list traversal order.
 *
 *  No occlusion / wall-clipping: the shader has no way to know
 *  there's geometry between a light and the vertex it's touching, so
 *  blaster light bleeds through thin walls. A per-group GroupsAreVisible
 *  filter was tried; it's ineffective in practice because Forsaken
 *  levels are typically one large BSP group per visible area, leaving
 *  the bulk of the bleed (intra-group walls) unfiltered. Per-vertex
 *  shadow rays or per-light camera-occlusion raycasts would actually
 *  fix it but cost too much CPU on PICA200 at 60 Hz. */
void c3d_upload_xlights(void)
{
	if (s_loc_lights < 0) return;

	C3D_FVec lights_buf[GPU_MAX_LIGHTS * 3];

	struct XLIGHT *top[GPU_MAX_LIGHTS];
	float          top_d2[GPU_MAX_LIGHTS];
	int            n_top = 0;

	float cx = CurrentCamera.Pos.x;
	float cy = CurrentCamera.Pos.y;
	float cz = CurrentCamera.Pos.z;
	for (struct XLIGHT *L = FirstLightVisible; L; L = ((XLIGHT*)L)->NextVisible)
	{
		float dx = ((XLIGHT*)L)->Pos.x - cx;
		float dy = ((XLIGHT*)L)->Pos.y - cy;
		float dz = ((XLIGHT*)L)->Pos.z - cz;
		float d2 = dx*dx + dy*dy + dz*dz;

		if (n_top < GPU_MAX_LIGHTS) {
			top[n_top]    = L;
			top_d2[n_top] = d2;
			n_top++;
		} else {
			int worst = 0;
			for (int k = 1; k < GPU_MAX_LIGHTS; k++)
				if (top_d2[k] > top_d2[worst]) worst = k;
			if (d2 < top_d2[worst]) {
				top[worst]    = L;
				top_d2[worst] = d2;
			}
		}
	}

	int n_packed = 0;
	for (; n_packed < n_top; n_packed++)
	{
		struct XLIGHT *L = top[n_packed];
		float size = ((XLIGHT*)L)->Size;
		float invSizeSq = (size > 0.0f) ? (1.0f / (size * size)) : 0.0f;
		lights_buf[n_packed * 3 + 0] = FVec4_New(
			((XLIGHT*)L)->Pos.x,
			((XLIGHT*)L)->Pos.y,
			((XLIGHT*)L)->Pos.z,
			invSizeSq);

		/* Color values matched to lights.cpp::XLight1Group: 0..255 → 0..1
		 * with no dimming. Saturation behaviour now identical to CPU. */
		float r = ((XLIGHT*)L)->r / 255.0f;
		float g = ((XLIGHT*)L)->g / 255.0f;
		float b = ((XLIGHT*)L)->b / 255.0f;
		float type = (((XLIGHT*)L)->Type == SPOT_LIGHT) ? 1.0f : 0.0f;
		lights_buf[n_packed * 3 + 1] = FVec4_New(r, g, b, type);

		lights_buf[n_packed * 3 + 2] = FVec4_New(
			((XLIGHT*)L)->Dir.x,
			((XLIGHT*)L)->Dir.y,
			((XLIGHT*)L)->Dir.z,
			((XLIGHT*)L)->CosArc);
	}

	while (n_packed < GPU_MAX_LIGHTS) {
		lights_buf[n_packed * 3 + 0] = FVec4_New(0.0f, 0.0f, 0.0f, 0.0f);
		lights_buf[n_packed * 3 + 1] = FVec4_New(0.0f, 0.0f, 0.0f, 0.0f);
		lights_buf[n_packed * 3 + 2] = FVec4_New(0.0f, 0.0f, 0.0f, 0.0f);
		n_packed++;
	}

	if (s_loc_ambient >= 0) {
		C3D_FVUnifSet(GPU_VERTEX_SHADER, s_loc_ambient,
			s_ambient_r, s_ambient_g, s_ambient_b, 0.0f);
	}
	if (s_loc_light_count >= 0) {
		C3D_FVUnifSet(GPU_VERTEX_SHADER, s_loc_light_count,
			(float)GPU_MAX_LIGHTS, 0.0f, 0.0f, 0.0f);
	}
	{
		C3D_FVec *dst = C3D_FVUnifWritePtr(GPU_VERTEX_SHADER, s_loc_lights, GPU_MAX_LIGHTS * 3);
		memcpy(dst, lights_buf, sizeof(lights_buf));
	}
}

/* Phong-tint per-object update.
 *
 * Sample the level's lit ambient at the object's world position
 * (GetRealLightAmbientWorldSpace = baked grey base + every active
 * XLight's contribution at that point), then blend toward the nearest
 * dominant XLight's hue with a falloff scale based on how deep the
 * object is inside that light's radius.
 *
 * Effect: enemies in vol2's lava chambers naturally pick up warm
 * baked tone in their specular highlight even with no rockets nearby;
 * a yellow rocket flying close shifts the highlight more strongly
 * yellow; both signals blend continuously instead of switching.
 *
 * Call at the start of every OBJECT-mode draw. Cost: one
 * GetRealLightAmbientWorldSpace walk (~50-200 cycles per visible
 * light, sums all of FirstLightVisible) + a C3D_LightColor bind. At
 * ~10 enemies per frame and ~20 visible lights, total ~40 K cycles
 * per frame — under 1% of OG 3DS frame budget. */
void c3d_update_phong_tint_for_object_pos(VECTOR *world_pos)
{
	if (!s_objLightReady || !world_pos) return;

	/* Sampled ambient at object position: baked level base + every
	 * visible XLight's per-position contribution (linear falloff, same
	 * math as lights.cpp::XLight1Group). Returned in 0..255 range. */
	float ar, ag, ab, aa;
	GetRealLightAmbientWorldSpace(world_pos, &ar, &ag, &ab, &aa);

	/* @vfp-exempt R3 — light inclusion/attenuation window compare; integerizing changes which lights apply. measured n/a. Verified 2026-10-25. */
	/* Normalise to 0..1 and apply a 70%-white floor. The engine's
	 * baked ambient is 0, so a quiet area
	 * with no nearby XLights would otherwise zero out the Phong light
	 * colour and kill the highlight entirely. The floor keeps the
	 * specular visible everywhere; tinted samples just shift the
	 * remaining 30% headroom. */
	/* @vfp-exempt R3 — light inclusion/attenuation window compare; integerizing changes which lights apply. measured n/a. Verified 2026-10-25. */
	const float kFloor = 0.70f;
	ar = ar / 255.0f; if (ar < kFloor) ar = kFloor; if (ar > 1.0f) ar = 1.0f;
	ag = ag / 255.0f; if (ag < kFloor) ag = kFloor; if (ag > 1.0f) ag = 1.0f;
	ab = ab / 255.0f; if (ab < kFloor) ab = kFloor; if (ab > 1.0f) ab = 1.0f;

	/* Find nearest XLight whose radius covers this object position. */
	struct XLIGHT *nearest = NULL;
	float          nearest_d2 = 0.0f;
	for (struct XLIGHT *L = FirstLightVisible; L; L = ((XLIGHT*)L)->NextVisible) {
		float dx = ((XLIGHT*)L)->Pos.x - world_pos->x;
		float dy = ((XLIGHT*)L)->Pos.y - world_pos->y;
		float dz = ((XLIGHT*)L)->Pos.z - world_pos->z;
		float d2 = dx*dx + dy*dy + dz*dz;
		float size = ((XLIGHT*)L)->Size;
		if (d2 < size * size) {
			if (!nearest || d2 < nearest_d2) {
				nearest    = L;
				nearest_d2 = d2;
			}
		}
	}

	float r = ar, g = ag, b = ab;
	if (nearest) {
		float size  = ((XLIGHT*)nearest)->Size;
		float falloff = 1.0f - (nearest_d2 / (size * size));
		if (falloff < 0.0f) falloff = 0.0f;
		float lr = ((XLIGHT*)nearest)->r / 255.0f;
		float lg = ((XLIGHT*)nearest)->g / 255.0f;
		float lb = ((XLIGHT*)nearest)->b / 255.0f;
		r = ar + (lr - ar) * falloff;
		g = ag + (lg - ag) * falloff;
		b = ab + (lb - ab) * falloff;
	}
	C3D_LightColor(&s_objLight, r, g, b);
}


/*===================================================================
	Render init / cleanup / flip
===================================================================*/

static void detect_caps(void)
{
	caps.anisotropic = 0.0f;
}

bool render_init(render_info_t *info)
{
	detect_caps();
	build_gamma_table(1.0);
	/* C3D_Init already called by c3d_context_init() stub */
	if (!c3d_renderer_init())
		return false;

	/* Wipe the VRAM framebuffers via the GPU before any engine code
	 * can render.  gfxInit(..., true) hands us framebuffers backed by
	 * VRAM which comes up holding whatever was last written to those
	 * addresses — GPU debris, the previous .3dsx's render target,
	 * uninitialised noise.  The engine's title-screen load takes
	 * ~2 seconds during which the top framebuffer would otherwise
	 * display garbage, and the bottom screen would stay garbage until
	 * the gameplay HUD first drew over it.
	 *
	 * CPU memset is unsafe at this stage — the VRAM mapping is not
	 * fully writable from CPU before the first GPU frame, so naive
	 * memset crashes with a data abort on FAR=0x1f300000.
	 *
	 * The GPU-side pattern: C3D_RenderTargetClear queues a clear
	 * command, but the clear only actually executes once the target
	 * is bound via C3D_FrameDrawOn inside a frame, and the cleared
	 * pixels only reach the visible buffer after C3D_FrameEnd
	 * triggers display transfer.  Bind every target each frame and
	 * run a few frame cycles to catch both halves of the
	 * double-buffer pair on top + bottom. */
	{
		int i;
		for (i = 0; i < 3; i++)
		{
			C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

			C3D_RenderTargetClear(s_targetLeft, C3D_CLEAR_ALL, 0x000000FF, 0xFFFFFF);
			C3D_FrameDrawOn(s_targetLeft);

			C3D_RenderTargetClear(s_targetRight, C3D_CLEAR_ALL, 0x000000FF, 0xFFFFFF);
			C3D_FrameDrawOn(s_targetRight);

			C3D_RenderTargetClear(s_targetBottom, C3D_CLEAR_ALL, 0x000000FF, 0xFFFFFF);
			C3D_FrameDrawOn(s_targetBottom);

			C3D_FrameEnd(0);
		}
	}

	info->ok_to_render = true;
	return true;
}

void render_cleanup(render_info_t *info)
{
	/* Cleanup handled by c3d_context_exit() stub */
	info->ok_to_render = false;
}

bool render_mode_select(render_info_t *info)
{
	render_cleanup(info);
	if (!platform_init_video())
		return false;
	return render_init(info);
}

bool render_reset(render_info_t *info)
{
	(void)info;
	return false;
}

void render_set_filter(bool red, bool green, bool blue)
{
	/* No-op on citro3d — software anaglyph removed.
	 * Hardware stereo uses separate render targets instead of color masking.
	 * Function retained because shared code in oct2.cpp calls it. */
	(void)red; (void)green; (void)blue;
}

bool render_flip(render_info_t *info)
{
	/* platform_render_present calls our c3d_present which calls
	 * C3D_FrameEnd if a frame is active */
	platform_render_present(info);
	return true;
}

const char *render_error_description(int e)
{
	(void)e;
	return NULL;
}

/*===================================================================
	Scene begin / end
===================================================================*/


bool FSBeginScene(void)
{
	static int s_first_begin_scene = 1;
	if (s_first_begin_scene) {
		s_first_begin_scene = 0;
	}
	/* Single-pass stereo vertex memo (STEREO_REPLAY): the left eye
	 * records converted ranges keyed by stable batch identity; the
	 * right eye reuses them with its own live uniforms and draw
	 * order. The old whole-list replay (replay_display_list +
	 * draw-skipping) is gone: it dropped inset/HUD draws on one eye
	 * and broke on per-eye culling differences. Here no draw is ever
	 * skipped — only the vertex-conversion memcpy, on memo hits. */
	smemo_begin_frame();
	/* Gated on 3DS stereo: mono frames must not pay the record cost. */
	/* A frame is hardware stereo only while the slider is up
	 * (stereo_enabled, refreshed every frame) and the technique is 3DS
	 * stereo. stereo_mode alone is the saved technique choice: it stays
	 * STEREO_MODE_3DS after the slider goes down, which used to keep the
	 * mono persistent cache off for good. The slider is read after this
	 * point, so the gate lags a transition by one frame; both caches are
	 * correct in either kind of frame, only the cost differs. */
	const bool stereo_frame = render_info.stereo_enabled &&
		render_info.stereo_mode == STEREO_MODE_3DS;
	s_memoMode = stereo_frame ? SMEMO_RECORD : SMEMO_OFF;
	s_memoSuspended = false;
	/* Slider gate: mono only (see s_persistActive comment). */
	s_persistActive = !stereo_frame;
	s_scratchUsed = 0;
	if (!s_inFrame)
	{
		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
		C3D_RenderTargetClear(s_targetLeft, C3D_CLEAR_ALL, 0x000000FF, 0xFFFFFF);
		C3D_FrameDrawOn(s_targetLeft);
		s_targetCurrent = s_targetLeft;
		C3D_BindProgram(&s_shaderProgram);

		/* Initialize GPU state for the frame — without this, the first
		 * draw call operates on uninitialized GPU registers */
		{
			C3D_AttrInfo *ai = C3D_GetAttrInfo();
			AttrInfo_Init(ai);
			AttrInfo_AddLoader(ai, 0, GPU_FLOAT, 3);  /* v0: pos */
			AttrInfo_AddLoader(ai, 1, GPU_FLOAT, 4);  /* v1: color */
			AttrInfo_AddLoader(ai, 2, GPU_FLOAT, 2);  /* v2: texcoord */
		}
		{
			C3D_TexEnv *env = C3D_GetTexEnv(0);
			C3D_TexEnvInit(env);
			C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
			C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
		}
		C3D_DepthTest(true, GPU_LESS, s_colorMask);
		C3D_CullFace(GPU_CULL_FRONT_CCW);
		C3D_DepthMap(true, 1.0f, 1.0f);

		/* Reset TexEnv cache — force first draw to set up GPU state */
		s_lastBoundTexture = NULL;
		s_lastTexEnvTextured = false;
		s_lastTexEnvMode = TEXENV_MODE_NONE;
		s_lastMatClass = -1;
		s_lastPhaseBucket = -1;

		/* Initialize modelView uniform (register 4) to identity, so
		 * draws before the first upload_matrices (which writes
		 * world_matrix there) see no model transform. */
		/* Write identity to modelView uniform (register 4) via GPUCMD */
		{
			float id[16] = {
				1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1
			};
			GPUCMD_AddWrite(GPUREG_VSH_FLOATUNIFORM_CONFIG, 0x80000000 | UNIFORM_MODELVIEW);
			GPUCMD_AddWrites(GPUREG_VSH_FLOATUNIFORM_DATA, (u32*)id, 16);
		}

#if defined(GPU_LIGHTING)
		c3d_clear_xlights();
#endif

		s_inFrame = true;
	}
	return true;
}

bool FSEndScene(void) { return true; }

/*===================================================================
	GPU state management
===================================================================*/

void reset_trans(void)
{
	C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD,
		GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
	_additive_blend_active = false;
}

void reset_zbuff(void)
{
	C3D_DepthTest(true, GPU_LESS, s_colorMask);
}

void disable_zbuff_write(void)
{
	/* Write color only (no depth), respecting anaglyph channel mask */
	GPU_WRITEMASK mask = (GPU_WRITEMASK)(s_colorMask & ~GPU_WRITE_DEPTH);
	C3D_DepthTest(true, GPU_LESS, mask);
}

void disable_zbuff(void)
{
	GPU_WRITEMASK mask = (GPU_WRITEMASK)(s_colorMask & ~GPU_WRITE_DEPTH);
	C3D_DepthTest(false, GPU_ALWAYS, mask);
}

void cull_none(void)
{
	C3D_CullFace(GPU_CULL_NONE);
}

void reset_cull(void)
{
	/* Default: cull back faces. Game's CW-front = PICA200's CCW-back.
	 * So cull PICA200's front (= game's back). */
	C3D_CullFace(GPU_CULL_FRONT_CCW);
}

void set_normal_states(void)
{
	reset_zbuff();
	reset_trans();
	_additive_blend_active = false;
}

void set_alpha_states(void)
{
	disable_zbuff_write();
	C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD,
		GPU_SRC_ALPHA, GPU_ONE, GPU_SRC_ALPHA, GPU_ONE);
	_additive_blend_active = true;
}

void set_whiteout_state(void)
{
	disable_zbuff_write();
	C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD,
		GPU_SRC_ALPHA, GPU_ONE, GPU_SRC_ALPHA, GPU_ONE);
}

/*===================================================================
	Clear
===================================================================*/

/* Scissored sub-viewport clear via rasterized quad. C3D_RenderTargetClear
 * can only clear the entire render target (no scissor support), so a
 * sub-viewport (missile cam, PIP, rear view) can't rely on it to wipe
 * just its inset rectangle. Instead we draw a full-NDC black quad at
 * z=1 with GPU_ALWAYS + depth write: the rasterizer naturally clips it
 * to the currently-bound C3D_SetViewport, writing black color and
 * far-plane depth only within the inset. This kills primary's
 * color-bleed-through and gives secondary's GPU_LESS draws a clean
 * depth slate. */
static void clear_sub_viewport_via_quad(void)
{
	if (!s_scratch || s_scratchUsed + 6 > s_scratchMax) return;

	gpu_vertex_t *dst = s_scratch + s_scratchUsed;
	s_scratchUsed += 6;

	/* PICA's clip range is [-1, 0] in z (not [-1, 1] like GL), so
	 * z=1 would be clipped. Use z=0 (far plane in this convention). */
	static const float verts[6][3] = {
		{-1.0f, -1.0f, 0.0f},
		{ 1.0f, -1.0f, 0.0f},
		{ 1.0f,  1.0f, 0.0f},
		{-1.0f, -1.0f, 0.0f},
		{ 1.0f,  1.0f, 0.0f},
		{-1.0f,  1.0f, 0.0f},
	};

	for (int i = 0; i < 6; i++) {
		dst[i].pos[0] = verts[i][0];
		dst[i].pos[1] = verts[i][1];
		dst[i].pos[2] = verts[i][2];
		dst[i].color[0] = 0.0f;
		dst[i].color[1] = 0.0f;
		dst[i].color[2] = 0.0f;
		dst[i].color[3] = 1.0f;
		dst[i].texcoord[0] = 0.0f;
		dst[i].texcoord[1] = 0.0f;
	}

	/* Identity MVP so NDC vertices go straight to clip space; the
	 * viewport transform maps them onto the inset rectangle. */
	C3D_Mtx identity;
	Mtx_Identity(&identity);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, UNIFORM_PROJECTION, &identity);
	C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, UNIFORM_MODELVIEW,  &identity);

	C3D_DepthTest(true, GPU_ALWAYS, s_colorMask);
	C3D_CullFace(GPU_CULL_NONE);

	C3D_TexEnv *env = C3D_GetTexEnv(0);
	C3D_TexEnvInit(env);
	C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR);
	C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
	C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD,
		GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);

	bind_vertex_buffer(dst);
	C3D_DrawArrays(GPU_TRIANGLES, 0, 6);

	/* Restore standard game-draw state */
	C3D_DepthTest(true, GPU_LESS, s_colorMask);
	C3D_CullFace(GPU_CULL_FRONT_CCW);

	/* Quad clear writes TexEnv stage 0 directly without going through
	 * apply_texenv_*. Invalidate the TexEnv cache so the next textured
	 * draw re-runs configure_texenv_* — otherwise the cache lies and
	 * subsequent walls render untextured (chase-cam regression). */
	s_lastTexEnvMode     = TEXENV_MODE_NONE;
	s_lastBoundTexture   = NULL;
	s_lastTexEnvTextured = false;
	s_lastMatClass       = -1;
	s_lastPhaseBucket    = -1;
}

bool FSClear(XYRECT *rect)
{
	(void)rect;
	if (!s_targetCurrent) return true;

	/* Sub-viewports (missile cam, PIP, rear view) render AFTER the main
	 * camera. Rasterized quad clear scissored to viewport via the
	 * rasterizer — wipes color + depth inside just the inset rect.
	 * See clear_sub_viewport_via_quad comment for why. */
	if (CameraRendering != CAMRENDERING_None &&
	    CameraRendering != CAMRENDERING_Main)
	{
		clear_sub_viewport_via_quad();
		return true;
	}

	C3D_RenderTargetClear(s_targetCurrent, C3D_CLEAR_ALL, 0x000000FF, 0xFFFFFF);
	return true;
}

bool FSClearBlack(void)
{
	if (!s_targetCurrent) return true;
	C3D_RenderTargetClear(s_targetCurrent, C3D_CLEAR_COLOR, 0x000000FF, 0);
	return true;
}

bool FSClearDepth(XYRECT *rect)
{
	(void)rect;
	if (s_targetCurrent)
		C3D_RenderTargetClear(s_targetCurrent, C3D_CLEAR_DEPTH, 0, 0xFFFFFF);
	return true;
}

/*===================================================================
	Viewport
===================================================================*/

static render_viewport_t s_viewport = { 0, 0, 400, 240, 1.0f, 1.0f, 0.0f, 1.0f };

bool FSGetViewPort(render_viewport_t *view)
{
	*view = s_viewport;
	return true;
}

/* One-shot flag set by FSSetNextViewPortScissorMode() and consumed
 * (then reset) by the next FSSetViewPort() call. Allows visi.cpp
 * ClipGroup to hand-off per-group "use scissor instead of sub-rect
 * viewport" decisions without changing FSSetViewPort's signature
 * (which would touch every caller). */
static bool s_next_view_use_scissor = false;
/* Persistent record of the most recent FSSetViewPort dispatch
 * (full-vp+scissor vs sub-rect vp). Kept for viewport-dispatch
 * diagnostics; the stereo vertex memo does not need it (both eyes
 * run the live dispatch path). */
static bool s_current_view_used_scissor = false;
void FSSetNextViewPortScissorMode(bool scissor_mode)
{
	s_next_view_use_scissor = scissor_mode;
}

bool FSSetViewPort(render_viewport_t *view)
{
	s_viewport = *view;

	/* Translate game-space sub-rect into PICA portrait coordinates.
	 * The engine viewport is 400×240 landscape; PICA top-screen target
	 * is 240×400 portrait (Y is across the long edge after the 90°
	 * rotation). Compute the same x/y/w/h pair regardless of which
	 * dispatch path we take below. */
	int bottom = (int)render_info.ThisMode.h - (int)(view->Y + view->Height);
	int gl_x = (int)view->X;
	int pica_x = bottom < 0 ? 0 : bottom;
	int pica_y = (400 - (int)view->Width) - gl_x;
	if (pica_y < 0) pica_y = 0;
	int pica_w = (int)view->Height;
	int pica_h = (int)view->Width;

	bool use_scissor = s_next_view_use_scissor;
	s_next_view_use_scissor = false;	/* one-shot — reset after consume */

	s_current_view_used_scissor = use_scissor;
	if (use_scissor)
	{
		/* Per-group draws (level-mesh visibility groups). Full-screen
		 * GPU viewport + scissor to the sub-rect, paired with the
		 * unmagnified cam->Proj that visi.cpp uploads. Every visible
		 * group shares the same projection — eliminates the per-group
		 * magnified-projection flicker / cracks at portal boundaries
		 * that PICA200 produced under the sub-rect-viewport path. */
		C3D_SetViewport(0, 0, 240, 400);
		C3D_SetScissor(GPU_SCISSOR_NORMAL,
		               (u32)pica_x, (u32)pica_y,
		               (u32)(pica_x + pica_w), (u32)(pica_y + pica_h));
	}
	else
	{
		/* HUD / 2D / orthographic dispatch — sub-rect viewport, no
		 * scissor. Used for non-3D-mesh draws which don't go through
		 * visi.cpp's per-group projection setup. */
		C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
		C3D_SetViewport((u32)pica_x, (u32)pica_y,
		                (u32)pica_w, (u32)pica_h);
	}

	return true;
}

/*===================================================================
	Matrix state
===================================================================*/

bool FSSetView(RENDERMATRIX *matrix)
{
	memmove(&view_matrix, &matrix->m, sizeof(view_matrix));
	return true;
}

bool FSSetWorld(RENDERMATRIX *matrix)
{
	memmove(&world_matrix, &matrix->m, sizeof(world_matrix));
	return true;
}

bool FSGetWorld(RENDERMATRIX *matrix)
{
	memmove(&matrix->m, &world_matrix, sizeof(matrix->m));
	return true;
}

bool FSSetProjection(RENDERMATRIX *matrix)
{
	memmove(&proj_matrix, &matrix->m, sizeof(proj_matrix));
	return true;
}

/*===================================================================
	Vertex / Index buffer management
	Phase 2: use linearAlloc for GPU-visible memory
===================================================================*/

bool FSCreateVertexBuffer(RENDEROBJECT *renderObject, int numVertices)
{
	renderObject->lpVertexBuffer = linearAlloc(numVertices * sizeof(LVERTEX));
	return renderObject->lpVertexBuffer != NULL;
}

bool FSCreateDynamicVertexBuffer(RENDEROBJECT *renderObject, int numVertices)
{
	return FSCreateVertexBuffer(renderObject, numVertices);
}

bool FSCreateNormalBuffer(RENDEROBJECT *renderObject, int numNormals)
{
	renderObject->lpNormalBuffer = linearAlloc(numNormals * sizeof(NORMAL));
	return renderObject->lpNormalBuffer != NULL;
}

bool FSCreateIndexBuffer(RENDEROBJECT *renderObject, int numIndices)
{
	/* numIndices is an index-element count at every call site (model and
	 * water loaders pass triangleCount*3, dynamic buffers pass 32767*3),
	 * so allocate exactly that many WORDs. The old *3 triple-counted and
	 * reserved 589,806 bytes per 98,301-index dynamic buffer instead of
	 * 196,602 (768 KiB recovered across the two). Write-bounds audit in
	 * perf/OPTIMIZATION-LOG.md: per-frame fills are MAXPOLYVERTS-capped
	 * (800 verts/call), text overlays write tens of quads, load-time
	 * loops write exactly triangleCount*3 — all far below requested. */
	renderObject->lpIndexBuffer = linearAlloc(numIndices * sizeof(WORD));
	return renderObject->lpIndexBuffer != NULL;
}

bool FSCreateDynamicIndexBuffer(RENDEROBJECT *renderObject, int numIndices)
{
	return FSCreateIndexBuffer(renderObject, numIndices);
}

bool FSLockIndexBuffer(RENDEROBJECT *renderObject, WORD **indices)
{
	*indices = (WORD *)renderObject->lpIndexBuffer;
	return true;
}

bool FSLockVertexBuffer(RENDEROBJECT *renderObject, LVERTEX **verts)
{
	*verts = (LVERTEX *)renderObject->lpVertexBuffer;
	return true;
}

bool FSUnlockIndexBuffer(RENDEROBJECT *renderObject)  { (void)renderObject; return true; }
bool FSUnlockVertexBuffer(RENDEROBJECT *renderObject) { (void)renderObject; return true; }

bool FSLockNormalBuffer(RENDEROBJECT *renderObject, NORMAL **normals)
{
	*normals = (NORMAL *)renderObject->lpNormalBuffer;
	return true;
}

bool FSUnlockNormalBuffer(RENDEROBJECT *renderObject) { (void)renderObject; return true; }

bool FSCreateDynamic2dVertexBuffer(RENDEROBJECT *renderObject, int numVertices)
{
	renderObject->lpVertexBuffer = linearAlloc(numVertices * sizeof(TLVERTEX));
	return renderObject->lpVertexBuffer != NULL;
}

bool FSLockPretransformedVertexBuffer(RENDEROBJECT *renderObject, TLVERTEX **verts)
{
	*verts = (TLVERTEX*)renderObject->lpVertexBuffer;
	return true;
}

void FSReleaseRenderObject(RENDEROBJECT *renderObject)
{
	int i;
	if (renderObject->lpVertexBuffer)
	{
		linearFree(renderObject->lpVertexBuffer);
		renderObject->lpVertexBuffer = NULL;
	}
	if (renderObject->lpNormalBuffer)
	{
		linearFree(renderObject->lpNormalBuffer);
		renderObject->lpNormalBuffer = NULL;
	}
	if (renderObject->lpIndexBuffer)
	{
		linearFree(renderObject->lpIndexBuffer);
		renderObject->lpIndexBuffer = NULL;
	}
	for (i = 0; i < renderObject->numTextureGroups; i++)
		renderObject->textureGroups[i].texture = NULL;
	renderObject->numTextureGroups = 0;
}

/*===================================================================
	TexEnv combiner pipeline.

	Three modes the renderer can put the GPU into per draw:

	  TEXENV_MODE_NONE     — untextured (HUD vertex-color paths).
	  TEXENV_MODE_DIFFUSE  — single-stage modulate: TEX0 × PRIMARY_COLOR.
	                         Used for textured draws that get neither
	                         of the modes below.
	  TEXENV_MODE_AUX      — walls: TEX0 × PRIMARY, then ADD_SIGNED the
	                         ProcTex output (TEXTURE3), a noise palette
	                         centred on grey, so the grain both
	                         brightens and darkens without washing out.
	  TEXENV_MODE_OBJECT   — models: TEX0 × PRIMARY, then ADD the
	                         fragment-light specular
	                         (FRAGMENT_SECONDARY_COLOR).

	Cached per-mode + per-texdata pointer so identical adjacent draws
	skip GPU state changes.
===================================================================*/

static void configure_texenv_diffuse(void)
{
	/* Stage 0: TEXTURE0 * PRIMARY_COLOR (vertex color carries baked
	 * lighting + per-vertex GPU light contribution). Stages 1+2
	 * always reset — across-frame state could otherwise leak the
	 * prior frame's AUX/OBJECT stage config since lastTexEnvMode
	 * resets to NONE at frame start (not OBJECT/AUX), so a
	 * conditional reset wouldn't fire. */
	C3D_TexEnv *env = C3D_GetTexEnv(0);
	C3D_TexEnvInit(env);
	C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR,
	              GPU_PRIMARY_COLOR);
	C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);

	C3D_TexEnvInit(C3D_GetTexEnv(1));
	C3D_TexEnvInit(C3D_GetTexEnv(2));
}

static void configure_texenv_none(void)
{
	C3D_TexEnv *env = C3D_GetTexEnv(0);
	C3D_TexEnvInit(env);
	C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR,
	              GPU_PRIMARY_COLOR);
	C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);

	C3D_TexEnvInit(C3D_GetTexEnv(1));
	C3D_TexEnvInit(C3D_GetTexEnv(2));
}

static void configure_texenv_aux(texture_t *texdata)
{
	/* Wall detail via ProcTex unit. Stage 0 is the legacy diffuse-
	 * modulate path so walls keep proper color + baked lighting;
	 * stage 1 ADD_SIGNEDs the procedural noise (TEXTURE3 = proctex
	 * output). The proctex configuration was selected per material
	 * class by the caller binding s_wallProcTex[texdata->material_class].
	 * Alpha REPLACE preserves stage 0's alpha so colour-keyed
	 * transparent textures still work. */
	{
		C3D_TexEnv *env = C3D_GetTexEnv(0);
		C3D_TexEnvInit(env);
		C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR,
		              GPU_PRIMARY_COLOR);
		C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);
	}

	{
		C3D_TexEnv *env = C3D_GetTexEnv(1);
		C3D_TexEnvInit(env);
		C3D_TexEnvSrc(env, C3D_Both, GPU_PREVIOUS, GPU_TEXTURE3,
		              GPU_PRIMARY_COLOR);
		C3D_TexEnvFunc(env, C3D_RGB,   GPU_ADD_SIGNED);
		C3D_TexEnvFunc(env, C3D_Alpha, GPU_REPLACE);
	}
	(void)texdata;
}

static void configure_texenv_object(void)
{
	/* Bind the object Phong light env so fragment-lighting hardware
	 * computes FRAGMENT_SECONDARY_COLOR (specular highlight) using
	 * the per-vertex outnq quat from the vshader. */
	C3D_LightEnvBind(&s_objLightEnv);

	/* Stage 0: legacy TEX0 × PRIMARY — proper model color + baked
	 * lighting + GPU vertex-light contribution preserved. */
	{
		C3D_TexEnv *env = C3D_GetTexEnv(0);
		C3D_TexEnvInit(env);
		C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR,
		              GPU_PRIMARY_COLOR);
		C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);
	}

	/* Stage 1: + FRAGMENT_SECONDARY_COLOR (Phong specular). The
	 * hardware computes this from the per-vertex normal-quat × the
	 * scene light × the material specular coefficient. Adds a moving
	 * highlight as the camera angle changes. */
	{
		C3D_TexEnv *env = C3D_GetTexEnv(1);
		C3D_TexEnvInit(env);
		C3D_TexEnvSrc(env, C3D_RGB, GPU_PREVIOUS,
		              GPU_FRAGMENT_SECONDARY_COLOR, (GPU_TEVSRC)0);
		C3D_TexEnvFunc(env, C3D_RGB, GPU_ADD);
	}

	/* Stage 2 left empty. A ProcTex grain stage (ADD_SIGNED, as in AUX
	 * mode) could go here; it was dropped to keep models to the shine
	 * alone. */
	C3D_TexEnvInit(C3D_GetTexEnv(2));
}

/* Apply the right TexEnv mode for a textured draw, given the bound
 * texture handle. Caches by mode + texdata pointer (+ ProcTex bucket in
 * AUX mode) to skip redundant GPU state changes within a batch. */
static void apply_texenv_for_texture(texture_t *texdata)
{
	/* OBJECT (Phong shine on enemies / pickups) takes precedence over
	 * AUX (wall ProcTex grain) — the same texture would never want
	 * both, but the object toggle wraps narrow draw_object call sites
	 * while AUX is the fall-through path for level-mesh draws. */
	texenv_mode_t mode;
	if (s_objProcTexOn && s_objLightReady && g_object_shine &&
	    s_objShineEligible)
		mode = TEXENV_MODE_OBJECT;
	else if (s_procTexReady && g_wall_detail && !s_isOrthoDraw &&
	         !s_skipAuxProcTex)
		mode = TEXENV_MODE_AUX;
	else
		mode = TEXENV_MODE_DIFFUSE;

	/* Compute desired ProcTex state up front so the cache key includes
	 * the per-execbuf atlas bucket. Two adjacent execbufs sharing the
	 * same texture but different atlas seeds will land in different
	 * buckets and force a (cheap) NoiseCoefs rewrite — the color LUT
	 * stays cached across the bucket change. */
	material_class_t desired_mc = MAT_METAL;
	int desired_bucket = -1;
	if (mode == TEXENV_MODE_AUX)
	{
		desired_mc = (material_class_t)texdata->material_class;
		if ((unsigned)desired_mc >= MAT_COUNT) desired_mc = MAT_METAL;
		float seed_phase =
		    ((s_atlasSeed >> 8) & 0xFFFF) * (1.0f / 65536.0f);
		float combined = texdata->proctex_phase + seed_phase;
		combined -= fast_floorf(combined);
		desired_bucket = (int)(combined * (float)ATLAS_PHASE_BUCKETS);
		if (desired_bucket >= ATLAS_PHASE_BUCKETS)
			desired_bucket = ATLAS_PHASE_BUCKETS - 1;
	}

	bool mode_changed = (mode != s_lastTexEnvMode) || !s_lastTexEnvTextured;
	bool aux_state_changed = (mode == TEXENV_MODE_AUX) &&
	    (desired_bucket != s_lastPhaseBucket ||
	     (int)desired_mc != s_lastMatClass);

	/* Cache check — same mode + same texture + (for AUX) same atlas
	 * bucket means no GPU state change needed. */
	if (!mode_changed && !aux_state_changed &&
	    s_lastBoundTexture == (void*)texdata)
		return;

	/* R08: bind only on a cache miss. The early-return path provably
	 * already has this texture bound, so binding there was a redundant
	 * GPU register write per cached call. */
	C3D_TexBind(0, &texdata->tex);

	if (mode == TEXENV_MODE_AUX && (mode_changed || aux_state_changed))
	{
		/* Cheap path when only the bucket changed: rewrite NoiseCoefs +
		 * ProcTexBind. The color LUT (256 entries) stays cached unless
		 * the material class changed. */
		material_class_t mc = desired_mc;
		float base_freq = (mc == MAT_ROCK)    ? 13.0f
		                : (mc == MAT_ORGANIC) ? 14.0f
		                                      : 16.0f;
		float amp       = (mc == MAT_ROCK)    ? 0.31f
		                : (mc == MAT_ORGANIC) ? 0.27f
		                                      : 0.25f;
		float phase = ((float)desired_bucket + 0.5f)
		            * (1.0f / (float)ATLAS_PHASE_BUCKETS);
		/* Use the low 3 bits of the bucket as an orthogonal frequency
		 * axis — same phase value × 8 different frequencies = 8× the
		 * visible variants per phase position. Combined with 64 phase
		 * buckets this is effectively 64 distinct surface "looks" per
		 * material class. */
		int   freq_step = desired_bucket & 7;          /* 0..7 */
		float freq      = base_freq * (1.0f + freq_step * 0.06f);
		C3D_ProcTexNoiseCoefs(&s_wallProcTex[mc],
		    C3D_ProcTex_UV, amp, freq, phase);
		C3D_ProcTexBind(0, &s_wallProcTex[mc]);
		if (mode_changed || (int)mc != s_lastMatClass)
			C3D_ProcTexColorLutBind(&s_wallProcColor[mc]);
		s_lastMatClass    = (int)mc;
		s_lastPhaseBucket = desired_bucket;
	}

	if (mode_changed)
	{
		if (mode == TEXENV_MODE_AUX)
			configure_texenv_aux(texdata);
		else if (mode == TEXENV_MODE_OBJECT)
			configure_texenv_object();
		else
			configure_texenv_diffuse();
	}

	s_lastTexEnvMode      = mode;
	s_lastBoundTexture    = (void*)texdata;
	s_lastTexEnvTextured  = true;
}

static void apply_texenv_untextured(void)
{
	if (s_lastTexEnvMode == TEXENV_MODE_NONE && !s_lastTexEnvTextured)
		return;
	configure_texenv_none();
	s_lastTexEnvMode     = TEXENV_MODE_NONE;
	s_lastBoundTexture   = NULL;
	s_lastTexEnvTextured = false;
}

/*===================================================================
	Texture management
===================================================================*/

void release_texture(LPTEXTURE texture)
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
static const char *s_hd_textures_dir = NULL;
static const char *s_hd_textures_fallback = NULL;

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

static bool try_load_hd_texture(LPTEXTURE *t, const char *path,
	u_int16_t *width, u_int16_t *height, bool *colorkey)
{
	char hd_path[512];
	char *p;
	const char *data_start = NULL;
	int i;
	FILE *f;
	texture_t *texdata;
	Tex3DS_Texture t3x;

	resolve_hd_textures_dirs();

	/* Find "data\" in path */
	for (i = 0; path[i]; i++)
	{
		if ((path[i] == 'd' || path[i] == 'D') &&
		    (path[i+1] == 'a' || path[i+1] == 'A') &&
		    (path[i+2] == 't' || path[i+2] == 'T') &&
		    (path[i+3] == 'a' || path[i+3] == 'A') &&
		    (path[i+4] == '\\' || path[i+4] == '/'))
		{
			data_start = &path[i+5];
			break;
		}
	}
	if (!data_start)
		return false;

	snprintf(hd_path, sizeof(hd_path), "romfs:/%s/%s", s_hd_textures_dir, data_start);
	for (p = hd_path; *p; p++)
	{
		if (*p == '\\') *p = '/';
		if (*p >= 'A' && *p <= 'Z') *p += 32;
	}
	p = strrchr(hd_path, '.');
	if (p) strcpy(p, ".t3x");
	else strcat(hd_path, ".t3x");

	f = fopen(hd_path, "rb");
	if (!f)
	{
		/* Preferred dir isn't shipped — try the fallback. Defensive;
		 * production CIAs always ship both packs, but this lets a
		 * future single-pack variant work without code changes. */
		snprintf(hd_path, sizeof(hd_path), "romfs:/%s/%s", s_hd_textures_fallback, data_start);
		for (p = hd_path; *p; p++)
		{
			if (*p == '\\') *p = '/';
			if (*p >= 'A' && *p <= 'Z') *p += 32;
		}
		p = strrchr(hd_path, '.');
		if (p) strcpy(p, ".t3x");
		else strcat(hd_path, ".t3x");
		f = fopen(hd_path, "rb");
		if (!f) return false;
	}

	/* Allocate texture_t wrapper */
	if (*t == NULL)
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
	t3x = Tex3DS_TextureImportStdio(f, &texdata->tex, NULL, false);
	fclose(f);

	if (!t3x)
	{
		DebugPrintf("HD texture: Tex3DS import failed for %s\n", hd_path);
		if (*t == NULL) free(texdata);
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
	*t = (LPTEXTURE)texdata;

	Tex3DS_TextureFree(t3x);

	DebugPrintf("HD texture: loaded %s (%dx%d) class=%u\n",
		hd_path, texdata->tex.width, texdata->tex.height,
		(unsigned)texdata->material_class);
	return true;
}

static bool create_texture(LPTEXTURE *t, const char *path,
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
		bool is_new = (*t == NULL);

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
			*t = (LPTEXTURE)texdata;
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
		*t = (LPTEXTURE)texdata;
	}

	DebugPrintf("Created texture: file=%s, width=%d, height=%d, colorkey=%s\n",
		image.path, image.w, image.h, image.colorkey ? "true" : "false");

	destroy_image(&image);
	return true;
}

bool FSCreateTexture(LPTEXTURE *texture, const char *fileName,
	u_int16_t *width, u_int16_t *height, int numMips, bool *colourkey)
{
	return create_texture(texture, fileName, width, height, numMips, colourkey);
}

bool update_texture_from_file(LPTEXTURE dstTexture, const char *fileName,
	u_int16_t *width, u_int16_t *height, int numMips, bool *colorkey)
{
	create_texture(&dstTexture, fileName, width, height, numMips, colorkey);
	return true;
}

/*===================================================================
	Lighting — CPU-side per-vertex, ported from the old OpenGL 1 backend
===================================================================*/

/* Lighting globals — set by the engine's TransExe system per object */

int render_lighting_point_lights_only = 1;

int render_light_ambience = 0;
int render_light_ambience_alpha = 255.0f;


void GetRealLightAmbientWorldSpace(VECTOR *Pos, float *R, float *G, float *B, float *A)
{
	VECTOR ray;
	float rlen, rlen2, lsize2, intensity, cosa, cosarc2;
	XLIGHT *LightPnt = FirstLightVisible;

	*R = *G = *B = render_light_ambience;
	*A = render_light_ambience_alpha;

	while (LightPnt)
	{
		ray.x = Pos->x - LightPnt->Pos.x;
		ray.y = Pos->y - LightPnt->Pos.y;
		ray.z = Pos->z - LightPnt->Pos.z;

		/* @vfp-exempt R3 — light inclusion/attenuation window compare; integerizing changes which lights apply. measured n/a. Verified 2026-10-25. */
		rlen2 = ray.x * ray.x + ray.y * ray.y + ray.z * ray.z;
		lsize2 = LightPnt->Size * LightPnt->Size;

		if (rlen2 < lsize2)
		{
			if (render_lighting_point_lights_only || LightPnt->Type == POINT_LIGHT)
			{
				/* @vfp-exempt R3 — light inclusion/attenuation window compare; integerizing changes which lights apply. measured n/a. Verified 2026-10-25. */
				intensity = 1.0f - rlen2 / (int)lsize2;
			}
			else if (LightPnt->Type == SPOT_LIGHT)
			{
				if (rlen2 > 0.0f)
				{
					rlen = sqrtf(rlen2);
					ray.x /= rlen;
					ray.y /= rlen;
					ray.z /= rlen;
				}
				/* @vfp-exempt R3 — light inclusion/attenuation window compare; integerizing changes which lights apply. measured n/a. Verified 2026-10-25. */
				cosa = ray.x * LightPnt->Dir.x +
				       /* @vfp-exempt R3 — light inclusion/attenuation window compare; integerizing changes which lights apply. measured n/a. Verified 2026-10-25. */
				       ray.y * LightPnt->Dir.y +
				       ray.z * LightPnt->Dir.z;

				if (rlen2 > lsize2 * 0.5f)
				{
					if (cosa > LightPnt->CosArc)
						intensity = ((lsize2 - rlen2) / (0.75f * lsize2)) *
						            /* @vfp-exempt R3 — light inclusion/attenuation window compare; integerizing changes which lights apply. measured n/a. Verified 2026-10-25. */
						            ((cosa - LightPnt->CosArc) / (1.0f - LightPnt->CosArc));
					else
						goto NEXT_LIGHT;
				}
				else if (rlen2 > MIN_LIGHT_SIZE)
				{
					/* @vfp-exempt R3 — light inclusion/attenuation window compare; integerizing changes which lights apply. measured n/a. Verified 2026-10-25. */
					cosarc2 = LightPnt->CosArc *
						(1.0f - (lsize2 * 0.5f - rlen2) / (lsize2 * 0.5f - MIN_LIGHT_SIZE));
					if (cosa > cosarc2)
						intensity = ((lsize2 - rlen2) / (lsize2 - MIN_LIGHT_SIZE)) *
						            /* @vfp-exempt R3 — light inclusion/attenuation window compare; integerizing changes which lights apply. measured n/a. Verified 2026-10-25. */
						            ((cosa - cosarc2) / (1.0f - cosarc2));
					else
						goto NEXT_LIGHT;
				}
				else
				{
					intensity = (cosa > 0.0f) ? 1.0f : 1.0f + cosa;
				}
			}
			else
				goto NEXT_LIGHT;

			*R += LightPnt->r * intensity;
			*G += LightPnt->g * intensity;
			*B += LightPnt->b * intensity;
			*A += 255.0f * intensity;
		}

NEXT_LIGHT:
		LightPnt = LightPnt->NextVisible;
	}

	if (*R > 255.0f) *R = 255.0f;
	if (*G > 255.0f) *G = 255.0f;
	if (*B > 255.0f) *B = 255.0f;
	if (*A > 255.0f) *A = 255.0f;
}

#define MINUS(X, Y) \
	tmp = X; tmp -= Y; \
	X = (tmp < 0) ? 0 : (tmp > 255) ? 255 : tmp

#define ADD(X, Y) MINUS(X, -Y)

#define MIX_COLOR_BLEND_LIGHT(COLOR, BLEND, LIGHT) \
	ADD(COLOR, LIGHT); MINUS(COLOR, BLEND)

/*===================================================================
	Draw functions — native citro3d path
	Submits triangles directly with C3D_DrawArrays; picaGL is not linked.
===================================================================*/

bool draw_render_object(RENDEROBJECT *renderObject, int primitive_type, bool orthographic)
{
	int group;

	/* Per-execbuf atlas seed — hash of the renderObject pointer.
	 * Stable for the lifetime of the level, distinct per execbuf, so
	 * walls in different execbufs sharing the same base texture each
	 * land in their own ProcTex phase bucket. */
	s_atlasSeed   = (u32)((uintptr_t)renderObject) * 0x9E3779B1u;
	s_isOrthoDraw = orthographic;
	c255_init();

	/* For object-mode draws (s_objProcTexOn set by mxload/mxaload),
	 * sample the lit ambient at the object's world position and blend
	 * with the dominant nearby XLight to drive the Phong specular hue.
	 * world_matrix's translation row holds the object's world pos. */
	if (s_objProcTexOn && s_objLightReady && g_object_shine &&
	    s_objShineEligible && !orthographic) {
		VECTOR pos;
		pos.x = world_matrix._41;
		pos.y = world_matrix._42;
		pos.z = world_matrix._43;
		c3d_update_phong_tint_for_object_pos(&pos);
	}

	/* Single-pass stereo memo: lookup happens per texture group in
	 * the convert loop below (hit = reuse recorded verts, miss =
	 * convert live). Nothing is skipped here — traversal, culling,
	 * state, uniforms and draw order all run live per eye, so inset
	 * and HUD draws are unaffected by construction. */

	if (!s_shaderReady || !renderObject || !renderObject->lpVertexBuffer || !s_scratch)
		return false;

	if (!orthographic)
	{
		upload_matrices();
	}
	else
	{
		C3D_Mtx ortho, identity;
		Mtx_OrthoTilt(&ortho, 0.0f, render_info.ThisMode.w,
			render_info.ThisMode.h, 0.0f, -1.0f, 1.0f, true);
		Mtx_Identity(&identity);
		C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, UNIFORM_PROJECTION, &ortho);
		C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, UNIFORM_MODELVIEW, &identity);
	}

	/* ---- Texture sorting ----
	 * Sort texture groups by texture pointer so consecutive groups share
	 * the same texture, maximizing draw call batching below.  Uses a
	 * local index array (not modifying the renderObject) and insertion
	 * sort — fast for the small N (max 64 on 3DS). */
	int sorted_order[MAX_TEXTURE_GROUPS];
	int numGroups = renderObject->numTextureGroups;
	{
		int si, sj;
		for (si = 0; si < numGroups; si++)
			sorted_order[si] = si;
		for (si = 1; si < numGroups; si++)
		{
			int key = sorted_order[si];
			void *key_tex = (void*)renderObject->textureGroups[key].texture;
			sj = si - 1;
			while (sj >= 0 &&
			       (void*)renderObject->textureGroups[sorted_order[sj]].texture > key_tex)
			{
				sorted_order[sj + 1] = sorted_order[sj];
				sj--;
			}
			sorted_order[sj + 1] = key;
		}
	}

	/* ---- Draw call batching ----
	 * Accumulate consecutive texture groups with the same GPU state
	 * (texture, colourkey, blend) into a single C3D_DrawArrays call.
	 * Vertices are contiguous in the scratch buffer so batching just
	 * extends the vertex count without issuing intermediate draws. */
	int   batch_startOffset = 0;
	int   batch_vertCount   = 0;
	void *batch_texture     = NULL;
	bool  batch_colourkey   = false;
	bool  batch_hasTexture  = false;
	/* Eye-2 submit spans: per-texgroup converted ranges for the batch
	 * being flushed (batch_tg0 .. group-1). A skipped texgroup records
	 * count 0 and is not submitted. */
	int   batch_tg0 = 0;
	/* Must stay zero-initialized: ortho groups never set these, and the
	 * lookup flush below must read count 0 (skip), never stack garbage.
	 * Garbage counts were submitted to DrawArrays and wedged the GPU
	 * queue on defend2 (FrameBegin wait, deterministic). */
	int   tg_memo_off[MAX_TEXTURE_GROUPS] = {0};
	int   tg_memo_cnt[MAX_TEXTURE_GROUPS] = {0};
	/* R05: retained-buffer submit per texgroup (NULL = scratch path).
	 * Zero-initialized for the same reason as above. */
	gpu_vertex_t *tg_persist_buf[MAX_TEXTURE_GROUPS] = {0};
	int   tg_persist_cnt[MAX_TEXTURE_GROUPS] = {0};

/* Macro: flush the current batch. Eye 1 (and memo off): one draw call,
 * exactly as before. Eye 2 in lookup mode: one draw per texgroup range
 * in the batch, in walk order, with the same per-batch state. Splitting
 * only the submit (never state, order or uniforms) keeps pixels
 * identical while tolerating per-eye culling differences: a texgroup
 * the left eye never converted is simply converted live instead. */
#define FLUSH_BATCH() do { \
	if (batch_vertCount > 0) { \
		int _has_persist = 0; \
		{ int _pi; for (_pi = batch_tg0; _pi < group; _pi++) if (tg_persist_buf[_pi]) { _has_persist = 1; break; } } \
		if ((_has_persist || (s_memoMode == SMEMO_LOOKUP)) && !orthographic) { \
			int _si; \
			int _consumed = 0; \
			long _run_off = 0; \
			int _run_cnt = 0; \
			int _run_open = 0; \
			for (_si = batch_tg0; _si < group && _consumed < batch_vertCount; _si++) { \
				if (tg_persist_buf[_si]) { \
					if (_run_open) { \
						bind_vertex_buffer(s_scratch + _run_off); \
						C3D_DrawArrays(GPU_TRIANGLES, 0, _run_cnt); \
						_run_open = 0; \
					} \
					bind_vertex_buffer(tg_persist_buf[_si]); \
					C3D_DrawArrays(GPU_TRIANGLES, 0, tg_persist_cnt[_si]); \
					_consumed += tg_persist_cnt[_si]; \
					continue; \
				} \
				if (tg_memo_cnt[_si] <= 0 || (_run_open && tg_memo_off[_si] != _run_off + _run_cnt)) { \
					if (_run_open) { \
						bind_vertex_buffer(s_scratch + _run_off); \
						C3D_DrawArrays(GPU_TRIANGLES, 0, _run_cnt); \
						_run_open = 0; \
					} \
					if (tg_memo_cnt[_si] <= 0) \
						continue; \
				} \
				if (!_run_open) { \
					_run_off = tg_memo_off[_si]; \
					_run_cnt = 0; \
					_run_open = 1; \
				} \
				_run_cnt += tg_memo_cnt[_si]; \
				_consumed += tg_memo_cnt[_si]; \
			} \
			if (_run_open) { \
				bind_vertex_buffer(s_scratch + _run_off); \
				C3D_DrawArrays(GPU_TRIANGLES, 0, _run_cnt); \
			} \
		} else { \
			bind_vertex_buffer(s_scratch + batch_startOffset); \
			C3D_DrawArrays(GPU_TRIANGLES, 0, batch_vertCount); \
		} \
		if (batch_colourkey) \
			C3D_AlphaTest(false, GPU_ALWAYS, 0); \
		if (batch_hasTexture && !_additive_blend_active) \
			C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, \
				GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO); \
		batch_vertCount = 0; \
	} \
} while(0)

	for (group = 0; group < numGroups; group++)
	{
		TEXTUREGROUP *tg = &renderObject->textureGroups[sorted_order[group]];
		int numIndices = tg->numTriangles * 3;
		int startVert = tg->startVert;
		int totalVerts, i;
		gpu_vertex_t *dst;

		if (numIndices <= 0) {
			tg_memo_cnt[group] = 0;
			continue;
		}

		/* Memo lookup (right eye only, never ortho): reuse the recorded
		 * range and skip conversion. Misses convert live below. */
		long memo_off = 0;
		int memo_cnt = 0;
		bool memo_hit = false;
		bool persist_hit = false;
		/* R05: level walls consult the retained buffers first (both
		 * eyes, any mode). A hit submits the retained buffer at flush
		 * time and converts nothing. */
		if (s_persistActive && !orthographic && renderObject->vertOwner == VERT_OWNER_LEVEL &&
		    renderObject->memoEpochPass == SMEMO_PASS_UNSET) {
			int pcnt = 0;
			gpu_vertex_t *pbuf = persist_lookup(renderObject,
				smemo_sub(sorted_order[group], 0, primitive_type), &pcnt);
			if (pbuf) {
				persist_hit = true;
				tg_persist_buf[group] = pbuf;
				tg_persist_cnt[group] = pcnt;
				totalVerts = pcnt;
			}
		}
		if (!persist_hit && s_memoMode == SMEMO_LOOKUP && !orthographic &&
		    memo_reusable(renderObject)) {
			memo_hit = smemo_lookup(renderObject,
				smemo_sub(sorted_order[group], 0, primitive_type),
				&memo_off, &memo_cnt,
				renderObject->memoEpochPass,
				renderObject->memoEpochGroup,
				renderObject->memoEpochResume);
		}

		/* Mid-frame flush: if scratch is nearly full, flush pending batch
		 * and split the frame to reset the scratch pointer. Hits convert
		 * nothing and take no scratch, so only a live convert checks. */
		if (!persist_hit && !memo_hit && s_scratchUsed + numIndices > s_scratchMax)
		{
			if (s_inFrame)
			{
				FLUSH_BATCH();
				C3D_FrameSplit(0);
				s_scratchUsed = 0;
				/* Scratch reset recycles eye-1 recorded offsets: stop
				 * reusing them. The pending batch was already flushed
				 * above (valid ranges); the rest of the frame runs
				 * fully live. */
				if (s_memoMode != SMEMO_OFF)
					s_memoMode = SMEMO_OFF;
				s_memoSaved = SMEMO_OFF;	/* a suspended pass must not re-arm it */
				C3D_BindProgram(&s_shaderProgram);
				{
					C3D_AttrInfo *ai = C3D_GetAttrInfo();
					AttrInfo_Init(ai);
					AttrInfo_AddLoader(ai, 0, GPU_FLOAT, 3);
					AttrInfo_AddLoader(ai, 1, GPU_FLOAT, 4);
					AttrInfo_AddLoader(ai, 2, GPU_FLOAT, 2);
				}
				s_lastBoundTexture = NULL;
				s_lastTexEnvTextured = false;
				s_lastTexEnvMode = TEXENV_MODE_NONE;
				if (!orthographic)
					upload_matrices();
				else
				{
					C3D_Mtx ortho, identity;
					Mtx_OrthoTilt(&ortho, 0.0f, render_info.ThisMode.w,
						render_info.ThisMode.h, 0.0f, -1.0f, 1.0f, true);
					Mtx_Identity(&identity);
					C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, UNIFORM_PROJECTION, &ortho);
					C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, UNIFORM_MODELVIEW, &identity);
				}
			}
			if (s_scratchUsed + numIndices > s_scratchMax) {
				tg_memo_cnt[group] = 0;
				continue;
			}
		}

		dst = s_scratch + s_scratchUsed;

		/* Convert live unless the memo/persist hit (VERIFY converts
		 * anyway for the oracle compare below). */
		if (persist_hit) {
			/* totalVerts + tg_persist_* already set above */
		} else if (memo_hit) {
			totalVerts = memo_cnt;
			tg_memo_off[group] = memo_off;
			tg_memo_cnt[group] = memo_cnt;
		} else
		/* Convert LVERTEX/TLVERTEX → gpu_vertex_t, expanding indices */
		{
		if (renderObject->lpIndexBuffer)
		{
			WORD *indices = (WORD*)renderObject->lpIndexBuffer + tg->startIndex;


			if (orthographic)
			{
				TLVERTEX *verts = (TLVERTEX*)renderObject->lpVertexBuffer;
				for (i = 0; i < numIndices; i++)
				{
					TLVERTEX *v = &verts[startVert + indices[i]];
					dst[i].pos[0] = v->x;
					dst[i].pos[1] = v->y;
					dst[i].pos[2] = v->z;
					dst[i].color[0] = s_c255[(v->color >> 16) & 0xFF];
					dst[i].color[1] = s_c255[(v->color >> 8) & 0xFF];
					dst[i].color[2] = s_c255[(v->color) & 0xFF];
					dst[i].color[3] = s_c255[(v->color >> 24) & 0xFF];
					dst[i].texcoord[0] = v->tu;
					dst[i].texcoord[1] = v->tv;
				}
			}
			else
			{
				LVERTEX *verts = (LVERTEX*)renderObject->lpVertexBuffer;
				for (i = 0; i < numIndices; i++)
				{
					int vi = startVert + indices[i];
					LVERTEX *v = &verts[vi];
					dst[i].pos[0] = v->x;
					dst[i].pos[1] = v->y;
					dst[i].pos[2] = v->z;
					dst[i].color[0] = s_c255[(v->color >> 16) & 0xFF];
					dst[i].color[1] = s_c255[(v->color >> 8) & 0xFF];
					dst[i].color[2] = s_c255[(v->color) & 0xFF];
					dst[i].color[3] = s_c255[(v->color >> 24) & 0xFF];
					dst[i].texcoord[0] = v->tu;
					dst[i].texcoord[1] = v->tv;
				}
				totalVerts = numIndices;
			}
			totalVerts = numIndices;
		}
		else
		{
			LVERTEX *verts = (LVERTEX*)renderObject->lpVertexBuffer;
			int numVerts = tg->numVerts;
			if (s_scratchUsed + numVerts > s_scratchMax) {
				tg_memo_cnt[group] = 0;
				continue;
			}
			for (i = 0; i < numVerts; i++)
			{
				int vi = startVert + i;
				LVERTEX *v = &verts[vi];
				dst[i].pos[0] = v->x;
				dst[i].pos[1] = v->y;
				dst[i].pos[2] = v->z;
				dst[i].color[0] = s_c255[(v->color >> 16) & 0xFF];
				dst[i].color[1] = s_c255[(v->color >> 8) & 0xFF];
				dst[i].color[2] = s_c255[(v->color) & 0xFF];
				dst[i].color[3] = s_c255[(v->color >> 24) & 0xFF];
				dst[i].texcoord[0] = v->tu;
				dst[i].texcoord[1] = v->tv;
			}
			totalVerts = numVerts;
		}


		/* Record the converted range (eye 1). Runs ONLY on the live
		 * path: on a memo hit the tg arrays already hold the looked-up
		 * range and must not be overwritten with the cursor. The tg
		 * fill itself is unconditional on the live path so the
		 * per-group flush (persist/mono mixes) can always read it. */
		if (!orthographic) {
			tg_memo_off[group] = dst - s_scratch;
			tg_memo_cnt[group] = totalVerts;
			if (s_memoMode == SMEMO_RECORD && memo_reusable(renderObject)) {
				smemo_record(renderObject,
					smemo_sub(sorted_order[group], 0, primitive_type),
					dst - s_scratch, totalVerts,
					renderObject->memoEpochPass,
					renderObject->memoEpochGroup,
					renderObject->memoEpochResume);
			}
		}
		/* R05: retain this conversion for level walls. On success the
		 * flush submits the retained buffer instead of scratch. */
		if (s_persistActive && !orthographic && renderObject->vertOwner == VERT_OWNER_LEVEL &&
		    renderObject->memoEpochPass == SMEMO_PASS_UNSET) {
			gpu_vertex_t *pbuf = persist_store(renderObject,
				smemo_sub(sorted_order[group], 0, primitive_type),
				dst, totalVerts);
			if (pbuf) {
				tg_persist_buf[group] = pbuf;
				tg_persist_cnt[group] = totalVerts;
				tg_memo_cnt[group] = 0;
			}
		}
		}

		/* Determine effective texture state for this group */
		bool eff_hasTexture = false;
		if (tg->texture)
		{
			texture_t *texdata = (texture_t*)tg->texture;
			if (texdata->initialized)
				eff_hasTexture = true;
		}

		/* Check if we need to break the batch — state changed */
		if (batch_vertCount > 0 &&
		    ((void*)tg->texture != batch_texture ||
		     tg->colourkey      != batch_colourkey ||
		     eff_hasTexture     != batch_hasTexture))
		{
			FLUSH_BATCH();
		}

		/* Start a new batch if needed — set GPU state */
		if (batch_vertCount == 0)
		{
			batch_startOffset = s_scratchUsed;
			batch_tg0 = group;
			batch_texture     = (void*)tg->texture;
			batch_colourkey   = tg->colourkey;
			batch_hasTexture  = eff_hasTexture;

			if (tg->colourkey)
				C3D_AlphaTest(true, GPU_GREATER, 0x64);
			if (eff_hasTexture && !_additive_blend_active)
				C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD,
					GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA,
					GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);

			/* Texture binding with TexEnv cache. apply_texenv_*
			 * picks single-stage diffuse vs two-stage detail-add
			 * based on the texture's material_class. */
			if (eff_hasTexture)
				apply_texenv_for_texture((texture_t*)tg->texture);
			else
				apply_texenv_untextured();
		}

		/* Accumulate into current batch */
		batch_vertCount += totalVerts;
		if (!persist_hit && !memo_hit)	/* hits wrote no scratch */
			s_scratchUsed += totalVerts;
	}

	/* Flush the final pending batch */
	FLUSH_BATCH();

#undef FLUSH_BATCH

	return true;
}

bool draw_object(RENDEROBJECT *renderObject)
{
	return draw_render_object(renderObject, 0, false);
}

/* World-space billboards / sprites / particles (explosions, weapon
 * trails, smoke). Same as draw_object but flags the batch as
 * not-eligible for AUX-mode ProcTex so wall grain doesn't apply. */
bool draw_billboard_object(RENDEROBJECT *renderObject)
{
	s_skipAuxProcTex = true;
	bool ok = draw_render_object(renderObject, 0, false);
	s_skipAuxProcTex = false;
	return ok;
}

bool draw_2d_object(RENDEROBJECT *renderObject)
{
	return draw_render_object(renderObject, 0, true);
}

bool draw_line_object(RENDEROBJECT *renderObject)
{
	return draw_render_object(renderObject, 1, false);
}

#endif /* __3DS__ */
