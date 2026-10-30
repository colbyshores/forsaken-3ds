/* perf_capture.h — compile-gated hardware capture instrumentation.
 *
 * Enabled only with -DPERF_CAPTURE (Makefile.3ds PERF_CAPTURE=1). Default
 * builds compile these macros to the plain calls / nothing: no new symbols
 * in the shipped binary (perf_capture_3ds.c is an empty TU when off).
 *
 * with fsync is 5-10 ms, so NOTHING here writes per frame. Counters live
 * in static memory; exactly ONE dump sequence to sdmc:/forsaken_perf.txt
 * happens at teardown, through perf_capture_3ds.c's own fd (the trace()
 * path is __3DS_DEBUG__-gated and would drag in autoboot/god-mode).
 * Tick source is svcGetSystemTick() (SYSCLOCK_ARM11 = 268.11184 MHz);
 * conversion to ms is done offline so on-device cost is one tick read
 * per gate.
 */
#ifndef PERF_CAPTURE_INCLUDED
#define PERF_CAPTURE_INCLUDED

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef PERF_CAPTURE

enum perf_stage {
	PERF_S_FRAME = 0,   /* one full aptMainLoop iteration (main.c) */
	PERF_S_MAINGAME,    /* MainGame  (oct2.c) — simulation tick */
	PERF_S_RENDER,      /* MainGameRender (oct2.c) — camera/scene render */
	PERF_S_FSBEGIN,     /* FSBeginScene (render_c3d.c) — frame acquire */
	PERF_S_MROUT,       /* MainRoutines (oct2.c) — object/AI routines */
	PERF_S_CAM,         /* RenderCurrentCamera* (oct2.c) — traversal+submit */
	PERF_S_FSEND,       /* FSEndScene (render_c3d.c) — GPU submit+wait */
	PERF_S_VIS,         /* RenderCurrentCamera: FindVisible+setup (per call) */
	PERF_S_BG,          /* RenderCurrentCamera: clear+background+unclipped */
	PERF_S_OPAQUE,      /* RenderCurrentCamera: opaque draw loop (per call) */
	PERF_S_TRANS,       /* RenderCurrentCamera: translucent phase (per call) */
	PERF_S_ENEMIES,     /* MainRoutines: ProcessEnemies (AI) */
	PERF_S_SPOTFX,      /* MainRoutines: ProcessSpotFX (effects) */
	PERF_S_BGOBJ,       /* MainRoutines: ProcessBGObjects */
	PERF_S_MODELS,      /* MainRoutines: ProcessModels */
	PERF_S_POLYS,       /* MainRoutines: ProcessPolys */
	PERF_S_RTLIGHTS,    /* MainRoutines: ProcessRTLights */
	PERF_S_WATER,       /* MainRoutines: WaterProcess */
	PERF_S_BULLETS,     /* MainRoutines: primary+secondary bullets */
	PERF_S_SHIPS,       /* MainRoutines: ProcessShips */
	PERF_S_FIRES,       /* MainRoutines: FirePrimary+FireSecondary */
	PERF_S_PICKUPS,     /* MainRoutines: ProcessPickups */
	PERF_S_XLIGHTS,     /* MainRoutines: ProcessXLights */
	PERF_S_AFTERBRN,    /* MainRoutines: DoAfterBurnerEffects */
	PERF_S_FMPOLY,      /* MainRoutines: FmPolyProcess */
	PERF_S_CONDITIONS,  /* MainRoutines: ProcessActiveConditions */
	PERF_S_TRIGGERS,    /* MainRoutines: ProcessTriggerAreas */
	PERF_S_REGEN,       /* MainRoutines: RegeneratePickups */
	PERF_S_RESTART,     /* MainRoutines: ProcessRestartPoints */
	PERF_S_TIME,        /* MainRoutines: CheckTimeLimit + countdown digits */
	PERF_S_MULTIPLES,   /* MainRoutines: ShowScreenMultiples */
	PERF_S_GOALS,       /* MainRoutines: ProcessGoals */
	PERF_S_SHIPMODE,    /* ProcessShips: ModeControl dispatch per ship */
	PERF_S_SHIPCOLLIDE, /* ProcessShips: ship/enemy/background collide block */
	PERF_S_SHIPPRE,     /* ProcessShips: per-ship head (nearest-node, damage fx) */
	PERF_S_SHIPPOST,    /* ProcessShips: per-ship tail (area checks, goals, CTF) */
	PERF_S_SHIPMODEL,   /* ProcessShips: UpdateShipModel per ship */
	PERF_S_SHIPCAM,     /* ProcessShips: Process_Remote_Camera + listener */
	PERF_S_SHIPDMG,     /* ProcessShips: DoDamagedEffects per ship */
	PERF_S_SHIPNODE,    /* ProcessShips: nearest-node find/update per ship */
	PERF_S_LOSNODE,     /* UpdateNearestNodeSpecial: AI_ClearLOS rays (count=rays) */
	PERF_S_BULLPRIM,    /* MainRoutines: ProcessPrimaryBullets */
	PERF_S_BULLSEC,     /* MainRoutines: ProcessSecondaryBullets */
	PERF_S_OPQTEX,      /* opaque loop: BackGroundTextureAnimation */
	PERF_S_OPQCLIP,     /* opaque loop: ClipGroup calls */
	PERF_S_OPQLINES,    /* opaque loop: ExecuteLines */
	PERF_S_OPQMODEL,    /* opaque loop: ModelDisp */
	PERF_S_OPQPOLYS,    /* opaque loop: DisplaySolidGroupClippedPolys */
	PERF_S_MDLINTERP,   /* ModelDisp: InterpFrames (per-vertex morph) */
	PERF_S_MDLLIGHT,    /* ModelDisp: per-model lighting (XLight/Ambient/Update) */
	PERF_S_MDLEXEC,     /* ModelDisp: ExecuteMxaloadHeader submission; count=morph models drawn */
	PERF_S_MDLENTRY,    /* ModelDisp: full per-model iteration */
	PERF_S_MDLTRANSFORM,/* ModelDisp: per-model matrix build + world upload */
	PERF_S_MDLLOD,      /* ModelDisp: camera distance + LOD selection */
	PERF_S_MDLBOUNDS,   /* ModelDisp: debug bounding-box generation */
	PERF_S_MDLSTATIC,   /* ModelDisp: static-model lighting + ExecuteMxloadHeader */
	PERF_S_MDLWRAP,     /* ModelDisp: inclusive function timer */
	PERF_S_INTERPPROBE, /* FRAME_INTERP_PROBE: second render traversal cost */
	PERF_S_INTERPTOTAL, /* FRAME_INTERP: total camera render including extra pass */
	PERF_S_INTERPONLY,  /* FRAME_INTERP: isolated extra-pass render cost */
	PERF_S_MAX
};

void perf_stage_begin(int stage);
void perf_stage_end(int stage, int stereo_pass);
void perf_dump(void);
/* profiling branch: append a level-headed dump and reset accumulators,
 * so one autotest sweep yields per-level rows instead of one aggregate. */
void perf_level_marker(const char *level_name);

bool perf_call_maingame(void);
bool perf_call_maingamerender(void);

/* end-the-stage-and-forward-result helper for `if (!X())` call sites */
static inline bool perf_guarded(int stage, bool r)
{
	perf_stage_end(stage, -1);
	return r;
}

/* end-the-stage-and-forward-int helper (RenderCurrentCamera returns int) */
static inline int perf_guarded_i(int stage, int r)
{
	perf_stage_end(stage, -1);
	return r;
}

#define PERF_MAINGAME()         perf_call_maingame()
#define PERF_MAINGAME_RENDER()  perf_call_maingamerender()
#define PERF_FRAME_BEGIN()      perf_stage_begin(PERF_S_FRAME)
#define PERF_FRAME_END()        perf_stage_end(PERF_S_FRAME, -1)
#define PERF_DUMP()             perf_dump()
#define PERF_LEVEL_MARKER(s)    perf_level_marker(s)

#else /* !PERF_CAPTURE */

bool MainGame(void);
bool MainGameRender(void);

/* Macros (not functions) so the PERF_S_* enum arguments are never named
 * in builds where the enum does not exist. */
#define perf_guarded(s, r)      (r)
#define perf_guarded_i(s, r)    (r)

/* no-op stage gates: call sites in oct2.c/ships.c/node.c use these
 * unconditionally, so the !PERF_CAPTURE build must accept them too. */
#define perf_stage_begin(s)     ((void)0)
#define perf_stage_end(s, e)    ((void)0)

#define PERF_MAINGAME()         MainGame()
#define PERF_MAINGAME_RENDER()  MainGameRender()
#define PERF_FRAME_BEGIN()      ((void)0)
#define PERF_FRAME_END()        ((void)0)
#define PERF_DUMP()             ((void)0)
#define PERF_LEVEL_MARKER(s)    ((void)0)

#endif /* PERF_CAPTURE */

#ifdef __cplusplus
}
#endif

#endif /* PERF_CAPTURE_INCLUDED */
