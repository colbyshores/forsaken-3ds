#pragma once

/* render.h -- renderer seam: buffers, textures, matrices and draw calls
 *
 * Core code draws only through these FS*, draw_* and render_* calls. The 3DS
 * citro3d backend (render_c3d.cpp) and the PC Vulkan backend
 * (render_vulkan.cpp) implement them; the 3DS build (__3DS__) adds the c3d_*
 * calls.
 * render_info (main.cpp) is the single process-lifetime render state.
 * A RENDEROBJECT is a vertex buffer (plus optional index and normal buffers)
 * split into TEXTUREGROUP draw ranges. Loaders point textureGroups at TAG_LEVEL
 * hunk memory sized to the file's count; the shared RenderBufs (oct2.cpp) use
 * MAX_TEXTURE_GROUPS-slot static scratch that the per-frame batchers fill and
 * flush when full. RENDERMATRIX is D3D row-vector layout. The memoEpoch* and
 * vertOwner fields are backend hints that other backends ignore.
 */

#include <assert.h>

#include "game/main.h"
#include "math/new3d.h"
#ifdef __3DS__
#include <3ds.h>
#else
#include <SDL.h>
#endif

extern bool  bSquareOnly;

typedef struct {
    DWORD       X;
    DWORD       Y;            /* Viewport Top left */
    DWORD       Width;
    DWORD       Height;       /* Viewport Dimensions */
    float       MinZ;         /* Min/max of clip Volume */
    float       MaxZ;
    float	ScaleX; 
    float	ScaleY; 
} render_viewport_t;

typedef struct {
    int     w;                /* width */
    int     h;                /* height */
} render_display_mode_t;

typedef struct
{
    long cx;
    long cy;
} CLIENTSIZE;

// stereo related enums
typedef enum { STEREO_MODE_COLOR, STEREO_MODE_HALF_HEIGHT, STEREO_MODE_HALF_WIDTH, STEREO_MODE_3DS } stereo_mode_t;
typedef enum { ST_CENTER, ST_LEFT, ST_RIGHT } stereo_position_t;
typedef enum { ST_GREEN, ST_BLUE, ST_CYAN } stereo_right_color_t;

typedef struct {
    int                     NumModes;				/* number of available display modes */
    int                     CurrMode;				/* number of current display mode (only when fullscreen) */
    render_display_mode_t * Mode;					/* desc avail modes */
    render_display_mode_t   ThisMode;				/* description of this mode, identical to Mode[CurrMode] */
    render_display_mode_t   WindowsDisplay;			/* current Windows disply mode */
    render_display_mode_t   default_mode;			/* current Windows disply mode */
    bool                    fullscreen;				/* in fullscreen exclusive mode? */
    CLIENTSIZE              window_size;			/* dimensions of client win */
    bool                    bPaused;				/* the app is paused */
    bool                    app_active;				/* the app is active */
    bool                    minimized;				/* app window is minimized */
    bool                    ok_to_render;			/* All objects etc. necessary rendering are in ok */
	bool					vsync;					/* vertical sync */
	float					aspect_ratio;			/* screen aspect ratio */
	bool					force_accel;			/* force 3d acelleration on gl */
	bool					wireframe;

#ifdef __3DS__
	/* no window/context handles on 3DS - Citro3D manages the framebuffer */
	int _3ds_reserved;
#else
	SDL_Window*             window;
#endif

	// stereo related options
	bool                  stereo_enabled;
	stereo_mode_t         stereo_mode;
	stereo_right_color_t  stereo_right_color;
	stereo_position_t     stereo_position;
	float                 stereo_eye_sep;
	float                 stereo_focal_dist;
} render_info_t;

/* The one render state (defined in main.cpp). */
extern render_info_t render_info;

typedef bool (*RenderCurrentCameraPt) (void);
bool RenderCurrentCameraInStereo( RenderCurrentCameraPt );


#undef RELEASE
#define RELEASE(x) {if (x != NULL) {x->Release(); x = NULL;}}

void cull_none( void );
void reset_cull( void );
void disable_zbuff( void );
void set_alpha_states( void );
void set_normal_states( void );
void reset_zbuff( void );
void reset_filtering( void );
void set_whiteout_state( void );
const char * render_error_description( int error );

typedef struct {
    long x1;
    long y1;
    long x2;
    long y2;
} XYRECT;

bool FSClearBlack(void);
bool FSClear(XYRECT * rect);
bool FSClearDepth(XYRECT * rect);

/* MAX_LEVEL_TEXTURE_GROUPS — per-execbuf texture group cap inside a
 * LEVELRENDEROBJECT (the level-mesh form of RENDEROBJECT, distinct from
 * the model RENDEROBJECT which uses MAX_TEXTURE_GROUPS).
 *
 * Was 8 on 3DS. The unguarded INCREASE_TEXTURE_GROUPS at mload.c:664
 * checks against MAX_TEXTURE_GROUPS (64) — not MAX_LEVEL_TEXTURE_GROUPS —
 * so when a level mesh execbuf used more than 8 unique textures, the
 * loader silently wrote textureGroups[8..N] past the end of the 8-slot
 * array, clobbering the start of the next execbuf's LEVELRENDEROBJECT
 * (lpVertexBuffer, lpIndexBuffer, vbLocked). At render time the next
 * execbuf's draws fed garbage pointers to the GPU and produced nothing —
 * presenting as the "black void where the next room should be" portal
 * regression on Remaster / N64-port levels (vol2 stays under 8 textures
 * per execbuf so it never triggered).
 *
 * Raised to 64 to match MAX_TEXTURE_GROUPS so the array and the
 * INCREASE_TEXTURE_GROUPS assert agree and overflow is properly caught
 * (or in release, properly bounded). Cost ~2.7 MB extra BSS in
 * Mloadheader's per-group renderObject[MAXEXECBUFSPERGROUP] arrays —
 * small fraction of the existing ~27 MB ModelHeaders BSS, fits comfortably
 * inside the OG 3DS application memory budget. */

#ifdef __3DS__
/*
 * MAX_TEXTURE_GROUPS — 3DS value (64 vs. 600 on PC).
 *
 * Load-time RENDEROBJECTs point textureGroups at hunk memory sized to the
 * file's count, so this cap only sizes the dynamic batch buffers
 * (RenderBufs' static scratch and stack-local batch objects) and bounds
 * INCREASE_TEXTURE_GROUPS.
 *
 * PC can afford 600 groups because desktop RAM is not a concern and the
 * in-game scene geometry rarely drives group counts that high.  On 3DS,
 * however, the loading screen and menu renderers call INCREASE_TEXTURE_GROUPS
 * once per sprite blit — hundreds of times per frame — so even 64 groups
 * can be exhausted before a single screen is fully composited.
 *
 * The mid-draw flush mechanism in screenpolys.c (ScrPolyDispSolid /
 * ScrPolyDispNonSolid) acts as a safety net: when numTextureGroups reaches
 * MAX_TEXTURE_GROUPS the current batch is flushed (unlock → draw → re-lock)
 * and the counter is reset to 0, preventing the assert/abort that
 * INCREASE_TEXTURE_GROUPS would otherwise trigger on overflow.
 */
#define MAX_TEXTURE_GROUPS 64
#else
#define MAX_TEXTURE_GROUPS 600
#endif

#define INCREASE_TEXTURE_GROUPS( group ) \
	assert( group->numTextureGroups < MAX_TEXTURE_GROUPS ); \
	group->numTextureGroups++;

typedef void * LPTEXTURE;

typedef struct TEXTUREGROUP
{
	int startVert;
	int startIndex;
	int numVerts;
	int numTriangles;
	bool colourkey;
	LPTEXTURE texture;
} TEXTUREGROUP;

typedef void * LPVERTEXBUFFER;
typedef void * LPARRAYBUFFER;
typedef void * LPINDEXBUFFER;

// taken from d3d9

typedef struct {
    float r;
    float g;
    float b;
    float a;
} COLORVALUE;

/* RENDEROBJECT / LEVELRENDEROBJECT: textureGroups is a TAG_LEVEL hunk
 * pointer sized to numTextureGroups at load time. Was an inline
 * MAX_TEXTURE_GROUPS-element array (~2 KB per RENDEROBJECT, ~256 KB
 * per Mloadheader.Group[g].renderObject[16] BSS) — moved to dynamic
 * allocation so each entry costs exactly numTextureGroups *
 * sizeof(TEXTUREGROUP) instead of 2 KB. Per-execbuf textureGroups[]
 * across an entire level fits comfortably in an 8 MB hunk.
 *
 * The two struct types are now structurally identical; LEVELRENDEROBJECT
 * is kept distinct for now so call-site signatures don't churn, but it's
 * a candidate for a follow-up unification. */
/* Stereo-memo fill epoch (opt5b). Shared exec-buffer slots
 * (RenderBufs[*] across visible groups) are refilled per batch with
 * texgroups restarting at 0, so (renderObject, texgroup) alone aliases
 * across fills. Every shared fill stamps the identity of the content it
 * just wrote; the memo records and re-checks it, so a lookup only hits
 * when the current fill is the same batch. Stable renderObjects (models,
 * water, per-slot execs) never stamp: their identity is already unique
 * and the field simply stays put within a frame, which compares equal.
 * Additive fields only — all backends ignore them. */
typedef enum {
	SMEMO_PASS_UNSET = 0,      /* never stamped (stable identity) */
	SMEMO_PASS_POLYS_CLIPPED = 1,
	SMEMO_PASS_POLYS_UNCLIPPED = 2,
	SMEMO_PASS_SOLID_CLIPPED = 3,
	SMEMO_PASS_SOLID_UNCLIPPED = 4,
	SMEMO_PASS_FM_CLIPPED = 5,
	SMEMO_PASS_FM_UNCLIPPED = 6,
	SMEMO_PASS_LINES = 7,
	/* Never recorded, never reused: the other eye (or frame) converts
	 * its own. Stamped where the content is not identified by the
	 * renderObject: FmPoly fills with FM_FLAG_MOVEOUT (pulled toward
	 * CurrentCamera.Pos, which differs per eye) and TransExe copies
	 * (slot address, refilled per camera pass in visibility order). */
	SMEMO_PASS_NO_REUSE = 8,
	/* Level execbuf whose colours are rewritten every frame (death
	 * white-out, underwater tint) but identically for both eyes: the
	 * stereo memo may reuse it within the frame; the persistent level
	 * cache must not keep it. Set and cleared per frame by XLight1Group. */
	SMEMO_PASS_PER_FRAME = 9
} SMEMO_PASS;
/* Fill epoch for a whole-scene (unclipped) fill: no single group. */
constexpr auto SMEMO_WHOLE_GROUP = 0xFFFFFFFFu;
/* Resume point of a poly/FmPoly fill: the texture page AND the poly it
 * starts from. A fill that runs out of vertex space mid-page resumes on
 * the same page from a later poly, so the page alone does not identify
 * the content (two chunks of one page would share an epoch and the
 * right eye would draw the other chunk's vertices). */
static inline unsigned int smemo_resume(int tpage, unsigned int poly)
{
	return (((unsigned int)tpage & 0xFFFu) << 16) | (poly & 0xFFFFu);
}
/* A fill that reaches MAX_TEXTURE_GROUPS (one texgroup per sprite quad)
 * draws, resets numTextureGroups and keeps filling: every chunk after
 * the first adds this step so its texgroup ids do not alias the earlier
 * chunk's. Pages fit in 12 bits (MAXTPAGESPERTLOAD is 50), leaving the
 * top 4 bits for the chunk number. */
constexpr auto SMEMO_CHUNK_STEP = (1u << 28);

/* Vertex-owner class (R05 sizing + future persistent-geometry tagging).
 * Set once at buffer-creation time by the owning loader; never changed
 * per frame. Backends may ignore it; the Citro3D bench counters use it
 * to attribute converted verts by owner class. */
typedef enum {
	VERT_OWNER_OTHER = 0,   /* unclassified (billboard temps, execs, HUD) */
	VERT_OWNER_LEVEL = 1,   /* MLOADHEADER level walls (static candidate) */
	VERT_OWNER_MODEL = 2,   /* MXLOADHEADER models */
	VERT_OWNER_MXA = 3,     /* MXALOADHEADER deformable models */
	VERT_OWNER_SHARED = 4,  /* RenderBufs[*] shared exec slots */
	VERT_OWNER_WATER = 5    /* water surface objects */
} VERT_OWNER;

typedef struct RENDEROBJECT
{
	LPVERTEXBUFFER	lpVertexBuffer;
	LPARRAYBUFFER	lpNormalBuffer;
	LPINDEXBUFFER	lpIndexBuffer;
	bool			vbLocked;
	int numTextureGroups;
	TEXTUREGROUP *textureGroups;
	unsigned int memoEpochPass;
	unsigned int memoEpochGroup;
	unsigned int memoEpochResume;
	unsigned int vertOwner;
} RENDEROBJECT;

typedef struct LEVELRENDEROBJECT
{
	LPVERTEXBUFFER	lpVertexBuffer;
	LPARRAYBUFFER	lpNormalBuffer;
	LPINDEXBUFFER	lpIndexBuffer;
	bool			vbLocked;
	int numTextureGroups;
	TEXTUREGROUP *textureGroups;
	unsigned int memoEpochPass;
	unsigned int memoEpochGroup;
	unsigned int memoEpochResume;
	unsigned int vertOwner;
} LEVELRENDEROBJECT;

typedef struct {
    union {
        struct {
            float        _11, _12, _13, _14;
            float        _21, _22, _23, _24;
            float        _31, _32, _33, _34;
            float        _41, _42, _43, _44;
        };
        float m[4][4];
    };
} RENDERMATRIX;

extern int render_lighting_point_lights_only;
extern int render_light_ambience;
extern int render_light_ambience_alpha;
bool render_mode_select( render_info_t * info );
void build_gamma_table( double gamma );
bool render_init( render_info_t * info );
void render_cleanup( render_info_t * info );
bool render_reset( render_info_t * info );
void render_set_filter( bool red, bool green, bool blue );
bool render_flip( render_info_t * info );
void reset_trans( void );
void disable_zbuff_write( void );

bool FSGetViewPort(render_viewport_t *returnViewPort);
bool FSBeginScene(void);
bool FSEndScene(void);
bool FSSetViewPort(render_viewport_t *newViewPort);

/* When set to true, the NEXT FSSetViewPort call uses a full-screen
 * GPU viewport plus a GPU scissor rect to the passed-in viewport,
 * instead of collapsing the GPU viewport to that sub-rect. The flag
 * auto-resets to false after one FSSetViewPort call so other callers
 * (HUD, missile cam, stats screen, title) keep the engine standard
 * behaviour. Set by visi.c ClipGroup for groups whose use_scissor_mode
 * is true — see VISGROUP::use_scissor_mode in camera.h for the why. */
void FSSetNextViewPortScissorMode(bool scissor_mode);

bool FSGetWorld(RENDERMATRIX *matrix);
bool FSSetWorld( RENDERMATRIX *matrix );
bool FSSetProjection( RENDERMATRIX *matrix );
bool FSSetView( RENDERMATRIX *matrix );

bool FSCreateNormalBuffer(RENDEROBJECT *renderObject, int numNormals);
bool FSLockNormalBuffer(RENDEROBJECT *renderObject, NORMAL **normals);
bool FSUnlockNormalBuffer(RENDEROBJECT *renderObject);
bool FSCreateDynamicVertexBuffer(RENDEROBJECT *renderObject, int numVertices);
bool FSCreateVertexBuffer(RENDEROBJECT *renderObject, int numVertices);
bool FSLockVertexBuffer(RENDEROBJECT *renderObject, LVERTEX **verts);
bool FSUnlockVertexBuffer(RENDEROBJECT *renderObject);
bool FSCreateDynamicIndexBuffer(RENDEROBJECT *renderObject, int numIndices);
bool FSCreateIndexBuffer(RENDEROBJECT *renderObject, int numIndices);
bool FSLockIndexBuffer(RENDEROBJECT *renderObject, WORD **indices);
bool FSUnlockIndexBuffer(RENDEROBJECT *renderObject);
bool FSLockPretransformedVertexBuffer(RENDEROBJECT *renderObject, TLVERTEX **verts);
bool FSCreateDynamic2dVertexBuffer(RENDEROBJECT *renderObject, int numVertices);

bool FSCreateTexture(LPTEXTURE *texture, const char *fileName, u_int16_t *width, u_int16_t *height, int numMips, bool * colourkey);
bool update_texture_from_file(LPTEXTURE dstTexture, const char *fileName, u_int16_t *width, u_int16_t *height, int numMips, bool * colourkey);
void release_texture( LPTEXTURE texture );

bool draw_render_object( RENDEROBJECT *renderObject, int primitive_type, bool orthographic );
bool draw_line_object(RENDEROBJECT *renderObject);
bool draw_object(RENDEROBJECT *renderObject);
bool draw_billboard_object(RENDEROBJECT *renderObject);
bool draw_2d_object(RENDEROBJECT *renderObject);

void FSReleaseRenderObject(RENDEROBJECT *renderObject);

#ifdef __3DS__
/* Citro3D backend extensions (render_c3d.c). Material-class switches set
   per draw by the model paths, and the Visuals menu toggles behind them
   (saved as ObjectShine / WallDetail in Configs/main.txt). */
void c3d_set_object_proctex(bool on);
void c3d_set_object_shine_eligible(bool eligible);
void c3d_transfer_eye(unsigned int eye);	/* stereo: switch to the other eye's target */
void c3d_memo_suspend(bool suspend);		/* stereo: render a nested camera (missile inset) with no memo */
void c3d_release_level_cache(void);		/* free the persistent level cache (level release) */
void c3d_upload_xlights(void);				/* push the visible XLights to the vshader */
void c3d_clear_bottom_screen(void);
bool c3d_hud_begin_bottom(void);			/* route HUD draws to the bottom screen */
void c3d_hud_end_bottom(void);
int forsaken_get_fps(void);
extern bool g_object_shine;
extern bool g_wall_detail;
#endif


/* One vertex as stored in the .mxv, .mx and .mxa files: Direct3D's 32-byte
   D3DLVERTEX record. The loaders convert each one to an LVERTEX. */
typedef struct OLDLVERTEX {
	float	x;
	float	y;
	float	z;
	DWORD	dwReserved;
	COLOR	color;
	COLOR	specular;
	float	tu;
	float	tv;
} OLDLVERTEX, *LPOLDLVERTEX;
static_assert( sizeof( OLDLVERTEX ) == 32, "OLDLVERTEX is a file record" );

/* bjd - remove when done. taken from SDK */
constexpr auto CLIP_LEFT = 0x00000001L;
constexpr auto CLIP_RIGHT = 0x00000002L;
constexpr auto CLIP_TOP = 0x00000004L;
constexpr auto CLIP_BOTTOM = 0x00000008L;
constexpr auto CLIP_FRONT = 0x00000010L;
