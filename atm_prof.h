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

#else /* !ATM_PROFILING */

#define atm_prof_frame_begin()   ((void)0)
#define atm_prof_frame_end()     ((void)0)
#define atm_prof_bench_tick()    ((void)0)
#define atm_prof_write_result()  ((void)0)
#define atm_prof_add(i, d)       ((void)0)

#endif /* ATM_PROFILING */

#endif /* ATM_PROF_INCLUDED */
