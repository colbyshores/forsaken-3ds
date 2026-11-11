/* atm_prof_3ds.c — backing for atm_prof.h. Compiles to nothing unless
 * ATM_PROFILING is defined, so a default build is unaffected by this file
 * being present (same rule perf_capture_3ds.c and fidelity_3ds.c follow).
 *
 * The AtmFrameStats aggregator itself is NOT duplicated here: it is the
 * shared ATM core (atm_core.c, compiled from the netload_stub tree by
 * Makefile.3ds) so the game, Rosalina, and the host conformance test cannot
 * silently diverge on bucket edges or percentile rounding.
 */
#ifdef ATM_PROFILING

#include <3ds.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "atm_prof.h"

extern bool QuitRequested;
extern char ShortLevelNames[][32];
extern int  NumLevels;

/* ---- benchmark knobs (all compile-time; see Makefile.3ds) -------------- */
#ifndef ATM_BENCH_LEVEL
#define ATM_BENCH_LEVEL "vol2"
#endif
#ifndef ATM_BENCH_FRAMES
#define ATM_BENCH_FRAMES 240
#endif
#ifndef ATM_BENCH_WARMUP
#define ATM_BENCH_WARMUP 30
#endif
/* 71 Hz simulation step (AGENTS.md: the engine ticks at 71 Hz). */
#ifndef ATM_BENCH_BUDGET_US
#define ATM_BENCH_BUDGET_US 14084u
#endif
/* Give up and report if the level never becomes live. Bounded so a broken
 * build produces evidence instead of an ATM run timeout. */
#ifndef ATM_BENCH_ENTER_TIMEOUT_MS
#define ATM_BENCH_ENTER_TIMEOUT_MS 90000u
#endif
/* Hard wall-clock ceiling for the measured window. */
#ifndef ATM_BENCH_MAX_MS
#define ATM_BENCH_MAX_MS 240000u
#endif
#ifndef ATM_BENCH_SCENARIO
#define ATM_BENCH_SCENARIO "forsaken-bench"
#endif

/* SYSCLOCK_ARM11 is 268.11184 MHz. libctru spells it as a double macro; a
 * local integer constant keeps this path free of double arithmetic
 * (AGENTS.md R4) and free of a runtime 64-bit divide (R11 — the divisor
 * below is a compile-time constant, so GCC lowers it to multiply+shift). */
#define ATM_SYSCLOCK_HZ 268111840ULL

#define ATM_RESULT_PATH "sdmc:/3ds/atm_result.txt"

const char *const atm_prof_counter_name[ATM_C_MAX] = {
	"level_load_ms",
	"status_exits",
	"stereo_frames",
	"mono_frames",
	"prim_bullets",
	"sec_bullets",
	"sfx_used",
	"sfx_create",
	"sfx_release",
	"trig_pcheck",
	"trig_echeck",
	"trig_proc",
};

enum {
	ATM_ST_IDLE = 0,      /* bench never armed (game exited early)          */
	ATM_ST_WAIT_LEVEL,    /* autoboot fired, waiting for a gameplay frame   */
	ATM_ST_ARMED,         /* gameplay entered; next frame starts the window */
	ATM_ST_WARMUP,        /* frames rendered but not sampled                */
	ATM_ST_MEASURE,       /* frames sampled into g_stats                    */
	ATM_ST_DONE           /* target met or a bounded fallback fired         */
};

static AtmFrameStats g_stats;
static uint32_t g_counters[ATM_C_MAX];

static int      g_state = ATM_ST_IDLE;
static uint64_t g_start_ms;        /* when autoboot chose the level        */
static uint64_t g_window_ms;       /* when the measured window opened      */
static uint64_t g_t0;              /* svcGetSystemTick at frame begin      */
static int      g_in_frame;
static int      g_tick_seen;       /* bench_tick ran during this frame     */
static int      g_idle_streak;
static int      g_warmup_left;
static char     g_level[32];
static int      g_level_slot = -1;
static char     g_level_resolve[16] = "none";
static char     g_reason[24] = "unfinished";
static int      g_started_written;

void atm_prof_add(int idx, uint32_t delta)
{
	if (idx < 0 || idx >= ATM_C_MAX)
		return;
	g_counters[idx] += delta;
}

static uint32_t atm_ticks_to_us(uint64_t ticks)
{
	return (uint32_t)((ticks * 1000000ULL) / ATM_SYSCLOCK_HZ);
}

/* ---- result file -------------------------------------------------------
 * Raw open/write/fsync on its own fd, the perf_capture_3ds.c pattern. Not
 * routed through trace(): that is __3DS_DEBUG__-gated and would drag in
 * autoboot and god mode, i.e. a fidelity change dressed as instrumentation.
 * Two writes per run at most: a status=started marker when the window opens
 * and the final summary at teardown. Never anything per frame. */
static int g_fd = -1;

static void atm_res_write(const char *s)
{
	size_t len = strlen(s);
	if (g_fd < 0)
		return;
	if (write(g_fd, s, len) < 0) { /* SD full/removed: ignore, never block */ }
}

static void atm_res_open(int truncate)
{
	if (g_fd >= 0)
		return;
	g_fd = open(ATM_RESULT_PATH,
	            O_WRONLY | O_CREAT | (truncate ? O_TRUNC : O_APPEND), 0666);
}

static void atm_res_close(void)
{
	if (g_fd >= 0) {
		fsync(g_fd);
		close(g_fd);
		g_fd = -1;
	}
}

static const char *atm_stereo_mode(void)
{
#ifdef ATM_BENCH_STEREO
	return ATM_BENCH_STEREO ? "forced_stereo" : "forced_mono";
#else
	return "slider";
#endif
}

/* Written the moment the measured window opens. If the process then dies,
 * the retrieved file says started rather than replaying the previous run's
 * complete summary as if it were this one's. */
static void atm_write_started(void)
{
	char line[160];
	atm_res_open(1);
	if (g_fd < 0)
		return;
	atm_res_write("workload=forsaken\nstatus=started\n");
	snprintf(line, sizeof(line), "scenario=%s\n", ATM_BENCH_SCENARIO);
	atm_res_write(line);
	snprintf(line, sizeof(line), "level=%s slot=%d resolve=%s\n",
	         g_level, g_level_slot, g_level_resolve);
	atm_res_write(line);
	snprintf(line, sizeof(line), "stereo=%s target_frames=%d warmup=%d budget_us=%u\n",
	         atm_stereo_mode(), (int)ATM_BENCH_FRAMES, (int)ATM_BENCH_WARMUP,
	         (unsigned)ATM_BENCH_BUDGET_US);
	atm_res_write(line);
	atm_res_close();
	g_started_written = 1;
}

static void atm_finish(const char *reason)
{
	if (g_state == ATM_ST_DONE)
		return;
	g_state = ATM_ST_DONE;
	snprintf(g_reason, sizeof(g_reason), "%s", reason);
	QuitRequested = true;
}

/* ---- level selection ---------------------------------------------------
 * Resolved by NAME, not by numeric slot, because the mission.dat ordering
 * differs between EDITION=original and EDITION=remaster; a pinned slot
 * would silently benchmark a different level per edition. Falls back to
 * slot 0 and records the fallback so the result file cannot lie about it. */
int atm_prof_bench_level(void)
{
	int i, found = -1;

	g_start_ms = osGetTime();
	atm_frame_init(&g_stats);

	for (i = 0; i < NumLevels && i < 64; i++) {
		if (strncmp(ShortLevelNames[i], ATM_BENCH_LEVEL, 31) == 0) {
			found = i;
			break;
		}
	}

	if (found >= 0) {
		snprintf(g_level, sizeof(g_level), "%s", ShortLevelNames[found]);
		snprintf(g_level_resolve, sizeof(g_level_resolve), "exact");
	} else {
		found = 0;
		snprintf(g_level, sizeof(g_level), "%s",
		         (NumLevels > 0) ? ShortLevelNames[0] : "?");
		snprintf(g_level_resolve, sizeof(g_level_resolve), "fallback0");
	}
	g_level_slot = found;
	g_state = ATM_ST_WAIT_LEVEL;
	return found;
}

/* ---- per-frame hooks --------------------------------------------------- */
void atm_prof_frame_begin(void)
{
	if (g_state == ATM_ST_IDLE || g_state == ATM_ST_DONE)
		return;

	if (g_state == ATM_ST_WAIT_LEVEL) {
		if ((osGetTime() - g_start_ms) > (uint64_t)ATM_BENCH_ENTER_TIMEOUT_MS)
			atm_finish("level_never_loaded");
		return;
	}

	if (g_state == ATM_ST_ARMED) {
		g_state = ATM_ST_WARMUP;
		g_warmup_left = (int)ATM_BENCH_WARMUP;
		g_window_ms = osGetTime();
		atm_write_started();
	}

	g_t0 = (uint64_t)svcGetSystemTick();
	g_in_frame = 1;
	g_tick_seen = 0;
}

void atm_prof_frame_end(void)
{
	uint32_t us;

	if (!g_in_frame)
		return;
	g_in_frame = 0;
	if (g_state == ATM_ST_DONE)
		return;

	us = atm_ticks_to_us((uint64_t)svcGetSystemTick() - g_t0);

	if (g_state == ATM_ST_WARMUP) {
		if (--g_warmup_left <= 0)
			g_state = ATM_ST_MEASURE;
		return;
	}

	atm_frame_add(&g_stats, us, (uint32_t)ATM_BENCH_BUDGET_US);
	if (gfxIs3D())
		atm_prof_add(ATM_C_STEREO_FRAMES, 1);
	else
		atm_prof_add(ATM_C_MONO_FRAMES, 1);

	/* bench_tick() only runs inside STATUS_SinglePlayer. Several consecutive
	 * measured frames without it means gameplay ended (death, menu, level
	 * transition); stop and say so instead of sampling title-screen frames. */
	if (g_tick_seen) {
		g_idle_streak = 0;
	} else {
		atm_prof_add(ATM_C_STATUS_EXITS, 1);
		if (++g_idle_streak >= 5) {
			atm_finish("left_gameplay");
			return;
		}
	}

	if (g_stats.count >= (uint64_t)ATM_BENCH_FRAMES) {
		atm_finish("frame_target");
		return;
	}
	if ((osGetTime() - g_window_ms) > (uint64_t)ATM_BENCH_MAX_MS)
		atm_finish("deadline");
}

void atm_prof_bench_tick(void)
{
	if (g_state == ATM_ST_WAIT_LEVEL) {
		atm_prof_add(ATM_C_LEVEL_LOAD_MS,
		             (uint32_t)(osGetTime() - g_start_ms));
		g_state = ATM_ST_ARMED;
		return;
	}
	if (g_state == ATM_ST_WARMUP || g_state == ATM_ST_MEASURE)
		g_tick_seen = 1;
}

/* ---- final summary ----------------------------------------------------- */
void atm_prof_write_result(void)
{
	char line[256];
	char summary[256];
	int i;
	u32 fnv = 2166136261u;
	const u8 *raw;
	size_t n;

	atm_res_open(1);
	if (g_fd < 0)
		return;

	atm_res_write("workload=forsaken\n");
	snprintf(line, sizeof(line), "status=%s\n",
	         g_state == ATM_ST_DONE ? "complete" : "incomplete");
	atm_res_write(line);

	/* Hardware identity proof: is_n3ds=0 plus the application region size is
	 * what distinguishes an Old-3DS pass from a New-3DS or proxy run. */
	{
		u32 is_n3ds = 0;
		APT_CheckNew3DS(&is_n3ds);
		snprintf(line, sizeof(line), "console=is_n3ds=%u region_mb=%u\n",
		         (unsigned)is_n3ds,
		         (unsigned)(osGetMemRegionSize(MEMREGION_APPLICATION) >> 20));
		atm_res_write(line);
	}

	snprintf(line, sizeof(line), "scenario=%s reason=%s\n",
	         ATM_BENCH_SCENARIO, g_reason);
	atm_res_write(line);
	snprintf(line, sizeof(line), "level=%s slot=%d resolve=%s stereo=%s\n",
	         g_level, g_level_slot, g_level_resolve, atm_stereo_mode());
	atm_res_write(line);
	snprintf(line, sizeof(line),
	         "target_frames=%d warmup=%d budget_us=%u window_ms=%llu\n",
	         (int)ATM_BENCH_FRAMES, (int)ATM_BENCH_WARMUP,
	         (unsigned)ATM_BENCH_BUDGET_US,
	         (unsigned long long)(g_window_ms ? osGetTime() - g_window_ms : 0));
	atm_res_write(line);

	for (i = 0; i < ATM_C_MAX; i++) {
		snprintf(line, sizeof(line), "CNT: %s=%u\n",
		         atm_prof_counter_name[i], (unsigned)g_counters[i]);
		atm_res_write(line);
	}

	n = atm_frame_format_summary(&g_stats, summary, sizeof(summary));
	if (n > 0) {
		summary[sizeof(summary) - 1] = '\0';
		atm_res_write(summary);
	}

	/* Trailing integrity marker over the emitted summary, so a truncated or
	 * interleaved file is detectable rather than silently trusted. */
	raw = (const u8 *)&g_stats;
	for (i = 0; i < ATM_C_MAX; i++) {
		fnv ^= g_counters[i];
		fnv *= 16777619u;
	}
	for (n = 0; n < sizeof(g_stats); n++) {
		fnv ^= raw[n];
		fnv *= 16777619u;
	}
	snprintf(line, sizeof(line), "checksum=%08x\n", (unsigned)fnv);
	atm_res_write(line);

	atm_res_close();
}

#endif /* ATM_PROFILING */
