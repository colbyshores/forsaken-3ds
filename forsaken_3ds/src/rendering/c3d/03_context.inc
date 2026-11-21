/* ============================================================================
 * 03_context.inc -- part of render_c3d.cpp (SINGLE TRANSLATION UNIT).
 *
 * Engine-facing globals, forward declarations, texture material classes,
 * the TexEnv state (TexEnvState), the citro3d context (c3d_context_init /
 * exit) and frame presentation (c3d_present).
 *
 * Not a standalone header: no include guard and no includes of its own. It is
 * #included in a fixed order from render_c3d.cpp, which says why.
 * ========================================================================== */

/* ---- globals expected by the engine ---- */


struct gl_caps_t { float anisotropic; };
gl_caps_t caps;

/* Forward declarations */
static void upload_matrices(void);
struct texture_t;

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
struct texture_t {
	C3D_Tex tex;
	bool    initialized;
	u8      material_class;
	float   proctex_phase;
};

/* bSquareOnly — shared global, always true on 3DS */
bool bSquareOnly = true;

/* Set by set_alpha_states (additive translucency) and cleared by
 * set_normal_states / reset_trans. While set, a textured draw leaves the
 * additive blend in place instead of restoring opaque blending after it. */
bool _additive_blend_active = false;

/* Track the last TexEnv mode so the combiner stages are reconfigured
 * only when a draw needs a different mode. */
enum texenv_mode_t {
	TEXENV_MODE_NONE = 0,    /* untextured / vertex-only */
	TEXENV_MODE_DIFFUSE,     /* single-stage diffuse * vertex */
	TEXENV_MODE_AUX,         /* wall: diffuse + ADD_SIGNED ProcTex grain */
	TEXENV_MODE_OBJECT,      /* model: diffuse + Phong specular */
};
constexpr int ATLAS_PHASE_BUCKETS = 64;

namespace {

/* The TexEnv combiner state (one instance, g_texenv): what the last draw
 * left bound and configured, the per-draw inputs that pick the next mode,
 * the ProcTex wall-grain tables and the object fragment light. */
class TexEnvState {
	public:
	void apply_for_texture(texture_t *texdata);  // was apply_texenv_for_texture
	void apply_untextured(void);                 // was apply_texenv_untextured
	void configure_object(void);                 // was configure_texenv_object

	/* TexEnv cache: skip redundant GPU state changes when consecutive
	 * draws use the same texture or same untextured mode. */
	void *lastBoundTexture = nullptr;  /* last texture_t* passed to C3D_TexBind */
	bool  lastTexEnvTextured = false;  /* last TexEnv was textured vs vertex-only */
	texenv_mode_t lastTexEnvMode = TEXENV_MODE_NONE;

	/* Per-mesh ProcTex phase variation. draw_render_object writes a
	 * per-renderObject seed; apply_for_texture reads it to pick a
	 * phase bucket so two adjacent walls sharing the same base texture
	 * get visually distinct grain. Folded into VideoOpts.g_wall_detail — there's no
	 * scenario where you'd want detail-grain on but the repeating version. */
	u32  atlasSeed        = 0;
	int  lastMatClass     = -1;
	int  lastPhaseBucket  = -1;

	/* True while inside an orthographic draw_render_object (HUD, screen
	 * polys, damage-flash overlay). apply_for_texture suppresses
	 * AUX-mode ProcTex for those — wall grain on a fullscreen red damage
	 * tint reads as garbage, same shape for HUD glyphs and menu icons. */
	bool isOrthoDraw      = false;

	/* Sticky flag: caller-managed. Wraps world-space billboard / particle
	 * batches (explosion sprites, smoke puffs, lens flares, weapon trails)
	 * so apply_for_texture skips AUX-mode ProcTex on them too. The
	 * orthographic auto-flag covers HUD/screen-poly draws; this covers
	 * 3D-space sprites that *aren't* orthographic but still shouldn't get
	 * wall grain. */
	bool skipAuxProcTex   = false;

	bool objProcTexOn = false;  /* per-draw toggle */

	/* Per-call eligibility gate for the OBJECT shine path. Defaults true
	 * (every object draw gets shine). Set false by callers that want to
	 * suppress shine for a specific model — currently used by models.cpp
	 * for OWNER_BGOBJECT models (doors, vents, animated decorations)
	 * where Phong specular reads as tacky on the large flat surfaces. */
	bool objShineEligible = true;

	/* The ProcTex wall-grain classes (01_state.inc says why), built once by
	 * c3d_renderer_init. */
	bool                 procTexReady = false;

	/* PICA200 fragment-lighting for object Phong shading. Vshader outputs
	 * a pseudo-normal-derived quaternion (outnq) per vertex; the hardware
	 * uses it + the bound light env to compute FRAGMENT_PRIMARY_COLOR
	 * (diffuse) and FRAGMENT_SECONDARY_COLOR (specular). The OBJECT
	 * TexEnv mode reads SECONDARY for the moving shine highlight. */
	bool          objLightReady = false;

	/* The tables (8 KB) are static data members: each its own .bss symbol,
	 * as the old file statics were. As instance members they would move
	 * the instance into .data (the cache's -1 and true initialisers), and
	 * objLight would sit 7 KB in, past the reach of a load offset. */
	static C3D_ProcTex          wallProcTex[MAT_COUNT];
	static C3D_ProcTexColorLut  wallProcColor[MAT_COUNT];
	static C3D_ProcTexLut       wallProcNoise;   /* shared smoothstep */
	static C3D_ProcTexLut       wallProcMap;     /* shared RGB map */
	static C3D_LightEnv  objLightEnv;
	static C3D_Light     objLight;
	static C3D_LightLut  objLightLut;
};

C3D_ProcTex          TexEnvState::wallProcTex[MAT_COUNT] = {};
C3D_ProcTexColorLut  TexEnvState::wallProcColor[MAT_COUNT] = {};
C3D_ProcTexLut       TexEnvState::wallProcNoise = {};
C3D_ProcTexLut       TexEnvState::wallProcMap = {};
C3D_LightEnv  TexEnvState::objLightEnv = {};
C3D_Light     TexEnvState::objLight = {};
C3D_LightLut  TexEnvState::objLightLut = {};

TexEnvState g_texenv;

} // namespace

/* Toggle: enable Phong-shine path for the next textured draw. Set by
 * mxload.cpp / mxaload.cpp around their draw_object calls so model + enemy
 * meshes get the specular highlight. (A misnomer: the OBJECT mode no
 * longer uses ProcTex.) */
void c3d_set_object_proctex(bool enable)
{
	g_texenv.objProcTexOn = enable;
}

void c3d_set_object_shine_eligible(bool enable)
{
	g_texenv.objShineEligible = enable;
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

int forsaken_get_fps(void) { return g_frame.fps_current; }

void FrameState::present()
{
	if (inFrame)
	{
		C3D_FrameEnd(0);
		inFrame = false;
	}
	/* Reset stereo state for next frame */
	stereoFrame = false;
	memoMode = SMEMO_OFF;
	hudDrawnThisFrame = false;

	/* FPS measurement. Roll the count over a 1-second window so the
	 * displayed value is stable. */
	u64 now = osGetTime();
	if (fps_window_start_ms == 0)
		fps_window_start_ms = now;
	fps_frames_in_win++;
	u64 elapsed = now - fps_window_start_ms;
	if (elapsed >= 1000)
	{
		/* @vfp-exempt R11 — FPS counter window update, once per measurement window (~1/s), not per frame. measured n/a. Verified 2026-10-25. */
		fps_current = (int)((fps_frames_in_win * 1000ULL) / elapsed);
		fps_window_start_ms = now;
		fps_frames_in_win = 0;
	}
}

void c3d_present(void) { g_frame.present(); }
