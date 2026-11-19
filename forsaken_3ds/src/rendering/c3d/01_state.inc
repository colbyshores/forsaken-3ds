/* ============================================================================
 * 01_state.inc -- part of render_c3d.cpp (SINGLE TRANSLATION UNIT).
 *
 * Uniform register ids, the citro3d state (targets, shader, scratch,
 * texenv/proctex/light caches), the GPU vertex format and the stereo memo.
 *
 * Not a standalone header: no include guard and no includes of its own. It is
 * #included in a fixed order from render_c3d.cpp, which says why.
 * ========================================================================== */

/* ---- uniform register locations (must match .v.pica) ---- */
constexpr int UNIFORM_PROJECTION = 0; /* fvec4[4] — view × proj, PICA-adjusted */
constexpr int UNIFORM_MODELVIEW = 4; /* fvec4[4] — world_matrix (model→world) */
constexpr int UNIFORM_AMBIENT = 8; /* fvec4    — .xyz=ambient, .w=unused */
constexpr int UNIFORM_LIGHT_COUNT = 9; /* fvec4    — .x=count(0..GPU_MAX_LIGHTS) */
constexpr int UNIFORM_LIGHTS = 10;   /* fvec4[24]— GPU_MAX_LIGHTS × 3 fvec4 each
								  *   light[3i+0] = (Pos.xyz, invSizeSq)
								  *   light[3i+1] = (r, g, b, type)
								  *   light[3i+2] = (Dir.xyz, CosArc)  */
constexpr int GPU_MAX_LIGHTS = 8; /* must match .v.pica `lights[24]` (= 8*3) */

/* ---- citro3d state ---- */

static DVLB_s          *s_shaderDVLB    = nullptr;
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

static C3D_RenderTarget *s_targetLeft    = nullptr;
static C3D_RenderTarget *s_targetRight   = nullptr;
static C3D_RenderTarget *s_targetBottom  = nullptr;  /* mono 320×240 for HUD */
static C3D_RenderTarget *s_targetCurrent = nullptr;  /* active render target */
static C3D_RenderTarget *s_targetSaved   = nullptr;  /* stashed target for HUD restore */
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
