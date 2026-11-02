/* perf_capture_3ds.c — backing counters for perf_capture.h.
 * Entire file compiles to nothing unless PERF_CAPTURE is defined, so the
 * wildcard-built default binary is byte-identical with this file present.
 *
 * Evidence recorded per stage: call count, total ticks, max ticks (worst
 * frame for that stage). Plus a frame-tick histogram (64 buckets, linear
 * 1 ms each up to 64 ms at 71 Hz budget ~14.1 ms) for the whole frame.
 * All on-device cost: one svcGetSystemTick() per gate (SYSCLOCK_ARM11
 * 268.11184 MHz), integer adds.
 * Dump writes sdmc:/forsaken_perf.txt at teardown (SD writes once, not
 * per frame) via its own fd — independent of the __3DS_DEBUG__ trace gate.
 */
#ifdef PERF_CAPTURE

#include <3ds.h>
#include <3ds/svc.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "perf_capture.h"

bool MainGame(void);
bool MainGameRender(void);

bool perf_call_maingame(void)
{
	bool r;
	perf_stage_begin(PERF_S_MAINGAME);
	r = MainGame();
	perf_stage_end(PERF_S_MAINGAME, -1);
	return r;
}

bool perf_call_maingamerender(void)
{
	bool r;
	perf_stage_begin(PERF_S_RENDER);
	r = MainGameRender();
	perf_stage_end(PERF_S_RENDER, -1);
	return r;
}

#define PERF_BUCKETS 128   /* 8 ms each -> covers 0..1024 ms (frame mean ~154 ms) */

typedef struct {
	unsigned long count;
	unsigned long long total;
	unsigned long max;
} perf_acc;

static perf_acc g_stage[PERF_S_MAX];
static unsigned long g_frame_ticks[PERF_BUCKETS + 1];
static unsigned long long g_t0[PERF_S_MAX];
static unsigned long g_frames;

#if defined(MODEL_BUCKET_VERIFY)
static unsigned long g_model_bucket_groups;
static unsigned long g_model_bucket_entries;
static unsigned long g_model_bucket_mismatches;
static int g_model_bucket_first_group = -1;
static unsigned int g_model_bucket_first_expected;
static unsigned int g_model_bucket_first_actual;

void perf_model_bucket_verify_result(unsigned long groups,
	unsigned long entries, unsigned long mismatches,
	int first_group, unsigned int first_expected, unsigned int first_actual)
{
	g_model_bucket_groups = groups;
	g_model_bucket_entries = entries;
	g_model_bucket_mismatches = mismatches;
	g_model_bucket_first_group = first_group;
	g_model_bucket_first_expected = first_expected;
	g_model_bucket_first_actual = first_actual;
}
#endif

static void perf_reset(void)
{
	int i;
	for (i = 0; i < PERF_S_MAX; i++) {
		g_stage[i].count = 0;
		g_stage[i].total = 0;
		g_stage[i].max = 0;
	}
	for (i = 0; i <= PERF_BUCKETS; i++)
		g_frame_ticks[i] = 0;
	g_frames = 0;
#if defined(MODEL_BUCKET_VERIFY)
	{
		extern void ModelVisBucket_Verify_Reset(void);
		g_model_bucket_groups = 0;
		g_model_bucket_entries = 0;
		g_model_bucket_mismatches = 0;
		g_model_bucket_first_group = -1;
		g_model_bucket_first_expected = 0;
		g_model_bucket_first_actual = 0;
		ModelVisBucket_Verify_Reset();
	}
#endif
}

void perf_stage_begin(int stage)
{
	if (stage < 0 || stage >= PERF_S_MAX)
		return;
	g_t0[stage] = (unsigned long long)svcGetSystemTick();
}

void perf_stage_end(int stage, int stereo_pass)
{
	unsigned long long d;

	if (stage < 0 || stage >= PERF_S_MAX)
		return;
	d = (unsigned long long)svcGetSystemTick() - g_t0[stage];
	g_stage[stage].count++;
	g_stage[stage].total += d;
	if (d > g_stage[stage].max)
		g_stage[stage].max = (unsigned long)d;
	if (stage == PERF_S_FRAME) {
		unsigned b = (unsigned)(d / 2144895ULL); /* 8 ms buckets (268.11184 MHz) */
		if (b > PERF_BUCKETS)
			b = PERF_BUCKETS;
		g_frame_ticks[b]++;
		g_frames++;
	}
	(void)stereo_pass; /* stereo attribution is done offline via RENDERCAM count/frames */
}

/* Direct SD writer, same pattern as boot_log: raw open/write/fsync, NOT
 * gated on __3DS_DEBUG__. Reusing trace() would force DEBUG=1, which also
 * turns on autoboot and god mode — a fidelity change for a perf capture.
 * One file, truncated per process, written only at teardown. */
static int g_perf_fd = -1;

static void perf_out(const char *msg)
{
	int len;
	if (g_perf_fd < 0) {
		/* Append: one autotest sweep chain-loads per process, and the
		 * profiling loop wants every level's row in one file. */
		g_perf_fd = open("sdmc:/forsaken_perf.txt",
		                 O_WRONLY | O_CREAT | O_APPEND, 0666);
		if (g_perf_fd < 0)
			return;
	}
	len = (int)strlen(msg);
	write(g_perf_fd, msg, len);
	write(g_perf_fd, "\n", 1);
	fsync(g_perf_fd);
}

static void perf_dump_body(void)
{
	static char line[128];
	static char line2[128];
	int i;

	for (i = 0; i < PERF_S_MAX; i++) {
		snprintf(line, sizeof(line),
		         "PERF: stage=%d count=%lu total=%llu max=%lu",
		         i, g_stage[i].count, g_stage[i].total, g_stage[i].max);
		perf_out(line);
		if (i == PERF_S_PRESENT) {
			snprintf(line2, sizeof(line2), "PERF: presents=%lu", g_stage[i].count);
			perf_out(line2);
		}
	}
	for (i = 0; i <= PERF_BUCKETS; i++) {
		if (g_frame_ticks[i]) {
			snprintf(line2, sizeof(line2), "PERF: hist ms<=%d count=%lu",
			         (i + 1) * 8, g_frame_ticks[i]);
			perf_out(line2);
		}
	}
	snprintf(line, sizeof(line), "PERF: frames=%lu", g_frames);
	perf_out(line);
#if defined(MODEL_BUCKET_VERIFY)
	if (g_model_bucket_groups) {
		static char verify_line[192];
		snprintf(verify_line, sizeof(verify_line),
		         "PERF: model_bucket_verify groups=%lu entries=%lu mismatches=%lu first_group=%d first_expected=%u first_actual=%u",
		         g_model_bucket_groups, g_model_bucket_entries,
		         g_model_bucket_mismatches, g_model_bucket_first_group,
		         g_model_bucket_first_expected, g_model_bucket_first_actual);
		perf_out(verify_line);
	}
#endif
}

void perf_level_marker(const char *level_name)
{
	static char line[160];
	snprintf(line, sizeof(line), "PERF: === level=%s ===",
	         level_name ? level_name : "?");
	perf_out(line);
	{
		extern size_t Hunk_Used(int), Hunk_Available(void), Hunk_Total(void);
		snprintf(line, sizeof(line),
		         "PERF: hunk level_used=%u startup_used=%u avail=%u total=%u",
		         (unsigned)Hunk_Used(1), (unsigned)Hunk_Used(0),
		         (unsigned)Hunk_Available(), (unsigned)Hunk_Total());
		perf_out(line);
	}
	perf_dump_body();
	perf_reset();
}

void perf_dump(void)
{
	perf_out("PERF: dump begin");
	perf_dump_body();
	perf_out("PERF: dump end");
}

#endif /* PERF_CAPTURE */
