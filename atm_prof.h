/* atm_prof.h — ATM frame-telemetry producer + deterministic benchmark driver.
 *
 * Compiled in only with -DATM_PROFILING (Makefile.3ds ATM_PROFILING=1).
 * Outside that flag every macro below expands to nothing and the backing TU
 * (atm_prof_3ds.c) compiles to an empty object, so release behavior is
 * unchanged (same discipline as perf_capture.h / fidelity.h).
 *
 * Three jobs, each with an explicit bound:
 *
 *   1. Feed one AtmFrameStats sample per rendered frame through
 *      atm_frame_add() from the shared ATM core
 *      (/mnt/nas/Development/netload_stub/sysmodules/rosalina/). That
 *      aggregator is a fixed 32-bucket histogram plus count/sum/min/max:
 *      O(1) per frame, no allocation, no I/O, no float math.
 *
 *   2. Drive a FIXED deterministic benchmark: one named level, no player
 *      input, a fixed number of measured frames after a fixed warmup, then
 *      a clean QuitRequested exit through normal teardown. Never
 *      interactive play. Bounded fallbacks stop the run (and say why) if the
 *      level never loads or gameplay is left early, so a bad build can never
 *      hang the console waiting on an ATM timeout.
 *
 *   3. Write /3ds/atm_result.txt — a status=started marker when the measured
 *      window opens and the final summary at teardown — through its own fd.
 *      Nothing is written per frame: an fsync'd SD write costs 5-10 ms on
 *      frame budget being measured.
 *
 * Clock choice, and its honest limits: samples come from
 * svcGetSystemTick() (SYSCLOCK_ARM11 = 268.11184 MHz), NOT osGetTime().
 * osGetTime() has 1 ms granularity and would quantize every sample; the tick
 * counter resolves to ~3.7 ns. The ATM histogram still buckets at 1 ms, so
 * p50/p95/p99 remain bucket-EDGE estimates (a documented ATM limitation)
 * while avg_us and max_us are tick-derived. Neither number is a CPU/GPU
 * utilization or memory-headroom measurement; ATM claims none of those.
 */
#ifndef ATM_PROF_INCLUDED
#define ATM_PROF_INCLUDED

#ifdef ATM_PROFILING

#include <stdint.h>
#include "atm/atm_core.h"

/* Bounded application counters alongside the frame stats. Fixed-size array,
 * indexed by this enum; every addition is a plain integer add. Phase D adds
 * per-eye traversal counters here rather than inventing a second mechanism. */
enum {
	ATM_C_LEVEL_LOAD_MS = 0, /* ms from autoboot fire to first gameplay frame */
	ATM_C_STATUS_EXITS,      /* gameplay left before the frame target was met */
	ATM_C_STEREO_FRAMES,     /* measured frames rendered with stereo on */
	ATM_C_MONO_FRAMES,       /* measured frames rendered with stereo off */
	ATM_C_PRIM_BULLETS,      /* sum of per-frame live primary bullets (÷frames = mean live) */
	ATM_C_SEC_BULLETS,        /* sum of per-frame live secondary bullets */
	ATM_C_SFX_USED,           /* looping-sfx slots visited per frame */
	ATM_C_SFX_CREATE,         /* sound_source_create attempts (failing loads retry forever) */
	ATM_C_SFX_RELEASE,        /* sound_release_source calls */
	ATM_C_DRAWS,              /* draw_render_object calls per frame */
	ATM_C_VERTS,              /* vertices converted per frame */
	ATM_C_TDRAWS,             /* draw_render_object calls inside the TRANS phase */
	ATM_C_TVERTS,             /* vertices converted inside the TRANS phase */
	ATM_C_LIGHTS,             /* active XLight count packed per upload (÷frames = mean) */
	ATM_C_LIGHTUPLDS,         /* c3d_upload_xlights calls (lights/uplds = mean per pack) */
	ATM_C_LIGHTLE2,           /* uploads packing <=2 lights (0/1/2-capacity eligibility) */
	ATM_C_LIGHTLE4,           /* uploads packing <=4 lights (4-capacity eligibility) */
	ATM_C_LIGHTSW,            /* program switches (proves the 2-cap path executes) */
	ATM_C_FILLPIX,            /* submitted pixel bounds (scissor rect or full target) */
	ATM_C_TFILLPIX,           /* same, inside the TRANS phase */
	ATM_C_COLORSKIP,          /* R06: level color-restores skipped (clean baseline) */
	ATM_C_PROJSKIP,           /* R11: groups taking fullscreen cam->Proj (magnified math skipped) */
	ATM_C_FILLQ0,            /* fill probe: draws covering <25% of screen */
	ATM_C_FILLQ1,            /* fill probe: draws covering 25-50% */
	ATM_C_FILLQ2,            /* fill probe: draws covering 50-75% */
	ATM_C_FILLQ3,            /* fill probe: draws covering >=75% (incl. fullscreen) */
	ATM_C_ORTHO,              /* fill probe: orthographic (HUD/2D) draws, any size */
	ATM_C_HITSKIP,            /* bullets: enemies skipped by squared-distance pre-reject */
	ATM_C_HITTEST,            /* bullets: enemies taking the full hit battery */
	ATM_C_NCMIX,              /* fill probe: near-straddling portals hit (mixed front/clean verts) */
	ATM_C_NCTIGHT,            /* fill probe: ...of those, SH clip produced tight bounds */
	ATM_C_INTERP_DIRTY,       /* M1: repeated-render purity violations (must stay 0) */
	ATM_C_MEMO_HIT,           /* opt5b: eye-2 texgroup memo hits (conversion skipped) */
	ATM_C_MEMO_MISS,          /* opt5b: eye-2 texgroup memo misses (converted live) */
	ATM_C_MEMO_MISMATCH,      /* opt5b verify: recorded vs live bytes differ (must stay 0) */
	ATM_C_MEMO_MF_SUB,        /* opt5b forensics: first-mismatch texgroup sub-key (one-shot) */
	ATM_C_MEMO_MF_CNT,        /* opt5b forensics: first-mismatch recorded word count (one-shot) */
	ATM_C_MEMO_MF_FI,         /* opt5b forensics: first-mismatch first-diff word index (one-shot) */
	ATM_C_MEMO_MF_EXP,        /* opt5b forensics: first-mismatch recorded word low 32b (one-shot) */
	ATM_C_MEMO_MF_GOT,        /* opt5b forensics: first-mismatch live word low 32b (one-shot) */
	ATM_C_MEMO_MF_WORD,       /* opt5b forensics: first-mismatch word index mod 9 (field id, one-shot) */
	ATM_C_MEMO_MF_SUB2,       /* opt5b forensics: second-mismatch texgroup sub-key (one-shot) */
	ATM_C_MEMO_MF_EPASS,      /* opt5b forensics: first-mismatch lookup epoch pass (system id, one-shot) */
	ATM_C_SCRATCH_SPLITS,     /* R02: mid-frame scratch overflows (FrameSplit+reset). Must stay 0 in benches; >0 = overflow path exercised, stills invalid until fenced */
	ATM_C_VOWN_OTHER,         /* R05 sizing: live-converted verts by owner class (unclassified) */
	ATM_C_VOWN_LEVEL,         /* R05 sizing: level-wall converted verts (static candidate) */
	ATM_C_VOWN_MODEL,         /* R05 sizing: model converted verts */
	ATM_C_VOWN_MXA,           /* R05 sizing: deformable-model converted verts */
	ATM_C_VOWN_SHARED,        /* R05 sizing: shared-slot converted verts */
	ATM_C_VOWN_WATER,         /* R05 sizing: water converted verts */
	ATM_C_LEVEL_UNIQUE_VERTS, /* R05 memory gate: unique level source verts at load (x36B = persistent bytes) */
	ATM_C_MAX
};
extern const char *const atm_prof_counter_name[ATM_C_MAX];

/* main.c render-loop hooks: one begin/end per aptMainLoop iteration. */
void atm_prof_frame_begin(void);
void atm_prof_frame_end(void);

/* oct2.c autoboot: level slot to start the benchmark in (name-resolved). */
int atm_prof_bench_level(void);

/* oct2.c STATUS_SinglePlayer, once per gameplay frame after MainGame(). */
void atm_prof_bench_tick(void);

/* main.c after the render loop exits: final result file, then clean exit. */
void atm_prof_write_result(void);

/* Add a delta to a bounded app counter. Out-of-range indices are dropped. */
void atm_prof_add(int idx, uint32_t delta);
/* Raw result-file writer (owned by atm_prof_3ds.c). Lets harness-owned
 * diagnostics (e.g. the q3 ring in render_c3d.c) append sections. */
void atm_prof_res_write(const char *s);
/* q3 ring dump, implemented in render_c3d.c (owns the batch context). */
void atm_prof_dump_q3(void);

#else /* !ATM_PROFILING */

#define atm_prof_frame_begin()   ((void)0)
#define atm_prof_frame_end()     ((void)0)
#define atm_prof_bench_tick()    ((void)0)
#define atm_prof_write_result()  ((void)0)
#define atm_prof_add(i, d)       ((void)0)
#define atm_prof_dump_q3()       ((void)0)

#endif /* ATM_PROFILING */

#endif /* ATM_PROF_INCLUDED */
