/* ============================================================================
 * 03_context.inc -- part of render_c3d.cpp (SINGLE TRANSLATION UNIT).
 *
 * Engine-facing globals, forward declarations, texture material classes,
 * the citro3d context (c3d_context_init / exit) and frame presentation
 * (c3d_present).
 *
 * Not a standalone header: no include guard and no includes of its own. It is
 * #included in a fixed order from render_c3d.cpp, which says why.
 * ========================================================================== */

/* ---- globals expected by the engine ---- */


struct gl_caps_t { float anisotropic; };
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

/* Texture handle — wraps a C3D_Tex for the engine's TEXTURE_HANDLE (void*).
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

/* Set by set_alpha_states (additive translucency) and cleared by
 * set_normal_states / reset_trans. While set, a textured draw leaves the
 * additive blend in place instead of restoring opaque blending after it. */
bool _additive_blend_active = false;

/* Frame state tracking */
static bool s_inFrame = false;

/* TexEnv cache: skip redundant GPU state changes when consecutive
 * draws use the same texture or same untextured mode. */
static void *s_lastBoundTexture = nullptr;  /* last texture_t* passed to C3D_TexBind */
static bool  s_lastTexEnvTextured = false;  /* last TexEnv was textured vs vertex-only */

/* Per-mesh ProcTex phase variation. draw_render_object writes a
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
enum texenv_mode_t {
	TEXENV_MODE_NONE = 0,    /* untextured / vertex-only */
	TEXENV_MODE_DIFFUSE,     /* single-stage diffuse * vertex */
	TEXENV_MODE_AUX,         /* wall: diffuse + ADD_SIGNED ProcTex grain */
	TEXENV_MODE_OBJECT,      /* model: diffuse + Phong specular */
};
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
