/* ============================================================================
 * 01_state.inc -- part of render_c3d.cpp (SINGLE TRANSLATION UNIT).
 *
 * Uniform register ids, the material classes, the GPU vertex format, the
 * stereo memo, and the shader and frame state (ShaderState, FrameState).
 * The other state classes sit with their code: PersistCache and
 * VertexScratch in 02, TexEnvState in 03, HdTexturePacks in 11.
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
enum material_class_t {
	MAT_METAL    = 0,
	MAT_ROCK,
	MAT_ORGANIC,
	MAT_COUNT
};

/* The per-class ProcTex tables live in TexEnvState (03_context.inc). */

/* User-toggleable graphics features (persisted via title.cpp's config_*).
 * TexEnvState::apply_for_texture reads these per draw: g_object_shine
 * enables the OBJECT mode (Phong specular on models), g_wall_detail the AUX
 * mode (ProcTex grain ADD_SIGNED onto walls). Both default true. */
bool g_object_shine = true;
bool g_wall_detail  = true;

/* Color write mask — always GPU_WRITE_ALL on citro3d (software anaglyph
 * removed; hardware stereo uses separate render targets instead). */
#define s_colorMask GPU_WRITE_ALL

/* ---- GPU vertex format (all floats, matches shader inputs) ---- */
struct gpu_vertex_t {
	float pos[3];
	float color[4];
	float texcoord[2];
};  /* 36 bytes */

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
enum smemo_mode_t {
	SMEMO_OFF = 0,    /* mono / default: memo untouched */
	SMEMO_RECORD,     /* left eye: convert live + record ranges */
	SMEMO_LOOKUP      /* right eye: reuse recorded ranges, convert misses */
};

namespace {

/* The vertex shader (one instance, g_shader): the parsed DVLB, the program
 * every frame and target switch binds, and whether c3d_renderer_init has
 * run. */
class ShaderState {
	public:
	DVLB_s          *shaderDVLB    = nullptr;
	shaderProgram_s  shaderProgram = {};
	bool             shaderReady   = false;

	/* GPU lighting uniform registers, set at init to the hardcoded UNIFORM_*
	 * values (libctru's uniform lookup returns -1 for these; see
	 * c3d_renderer_init). */
	s8 loc_ambient     = -1;
	s8 loc_light_count = -1;
	s8 loc_lights      = -1;

	/* The ambient term (06_matrices_lights.inc). Static, as the old file
	 * statics were: nothing writes them, so GCC folds their zeros. */
	static float ambient_r;
	static float ambient_g;
	static float ambient_b;
};

/* Where and in which frame the GPU draws (one instance, g_frame): the
 * render targets (left and right top-screen eyes, the bottom screen), the
 * one being drawn on, whether a frame is open, the HUD's bottom-screen
 * redirection, the stereo memo's mode, the frame-rate counter and the
 * viewport. */
class FrameState {
	public:
	void suspend_memo( bool suspend );  // was c3d_memo_suspend
	void present();                     // was c3d_present

	/* Frame state tracking */
	bool inFrame = false;

	C3D_RenderTarget *targetLeft    = nullptr;
	C3D_RenderTarget *targetRight   = nullptr;
	C3D_RenderTarget *targetBottom  = nullptr;  /* mono 320×240 for HUD */
	C3D_RenderTarget *targetCurrent = nullptr;  /* active render target */
	C3D_RenderTarget *targetSaved   = nullptr;  /* stashed target for HUD restore */
	bool hudDrawnThisFrame = false;          /* HUD drawn once per frame */
	/* Saved render_info state during HUD-on-bottom pass (restored on End) */
	int  hudSavedThisModeW = 0;
	int  hudSavedThisModeH = 0;
	int  hudSavedWinCx     = 0;
	int  hudSavedWinCy     = 0;

	smemo_mode_t memoMode = SMEMO_OFF;
	/* c3d_memo_suspend: a nested camera pass (missile inset) runs inside an
	 * eye pass and refills the same stamped fills from another camera, so
	 * it must neither record over the eye's ranges nor reuse them. */
	smemo_mode_t memoSaved = SMEMO_OFF;
	bool memoSuspended = false;

	/* Frame-rate counter. Updated once per swap, exposed via the
	 * `forsaken_get_fps()` accessor so the HUD overlay can draw it. */
	u32    fps_frames_in_win = 0;
	u64    fps_window_start_ms = 0;
	int    fps_current       = 0;

	/* The engine viewport (08_viewport.inc). */
	render_viewport_t viewport = { 0, 0, 400, 240, 1.0f, 1.0f, 0.0f, 1.0f };
	/* One-shot flag set by FSSetNextViewPortScissorMode() and consumed
	 * (then reset) by the next FSSetViewPort() call. Allows visi.cpp
	 * ClipGroup to hand-off per-group "use scissor instead of sub-rect
	 * viewport" decisions without changing FSSetViewPort's signature
	 * (which would touch every caller). */
	bool next_view_use_scissor = false;

	/* Written and never read, so GCC drops their stores, as it did for the
	 * old file statics; as instance members the stores would stay.
	 * stereoFrame: set by c3d_transfer_eye, cleared by c3d_present.
	 * current_view_used_scissor: persistent record of the most recent
	 * FSSetViewPort dispatch (full-vp+scissor vs sub-rect vp). Kept for
	 * viewport-dispatch diagnostics; the stereo vertex memo does not need
	 * it (both eyes run the live dispatch path). */
	static bool stereoFrame;
	static bool current_view_used_scissor;
};

bool FrameState::stereoFrame = false;
bool FrameState::current_view_used_scissor = false;

ShaderState g_shader;
FrameState g_frame;

} // namespace

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

void FrameState::suspend_memo(bool suspend)
{
	if (suspend) {
		if (!memoSuspended) {
			memoSaved = memoMode;
			memoMode = SMEMO_OFF;
			memoSuspended = true;
		}
	} else if (memoSuspended) {
		memoMode = memoSaved;
		memoSuspended = false;
	}
}

void c3d_memo_suspend(bool suspend) { g_frame.suspend_memo(suspend); }
