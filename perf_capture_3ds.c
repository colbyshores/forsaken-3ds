/* perf_capture_3ds.c — backing counters for perf_capture.h.
 * Entire file compiles to nothing unless PERF_CAPTURE is defined, so the
 * wildcard-built default binary is byte-identical with this file present.
 *
 * Evidence recorded per stage: call count, total ticks, max ticks (worst
 * frame for that stage). Plus a frame-tick histogram (64 buckets, linear
 * 1 ms each up to 64 ms at 71 Hz budget ~14.1 ms) for the whole frame.
 * All on-device cost: one timerGetTicks() per gate, integer adds.
 * Dump is a single trace() call at teardown (SD write once, not per frame).
 */
#ifdef PERF_CAPTURE

#include <3ds.h>
#include <stdio.h>
#include <string.h>
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

#define PERF_BUCKETS 64

typedef struct {
	unsigned long count;
	unsigned long long total;
	unsigned long max;
} perf_acc;

static perf_acc g_stage[PERF_S_MAX];
static unsigned long g_frame_ticks[PERF_BUCKETS + 1];
static unsigned long long g_t0[PERF_S_MAX];
static unsigned long g_frames;

void perf_stage_begin(int stage)
{
	if (stage < 0 || stage >= PERF_S_MAX)
		return;
	g_t0[stage] = (unsigned long long)timerGetTicks();
}

void perf_stage_end(int stage, int stereo_pass)
{
	unsigned long long d;

	if (stage < 0 || stage >= PERF_S_MAX)
		return;
	d = (unsigned long long)timerGetTicks() - g_t0[stage];
	g_stage[stage].count++;
	g_stage[stage].total += d;
	if (d > g_stage[stage].max)
		g_stage[stage].max = (unsigned long)d;
	if (stage == PERF_S_FRAME) {
		unsigned b = (unsigned)(d / 3355440ULL); /* 1 ms buckets (3.35544 MHz) */
		if (b > PERF_BUCKETS)
			b = PERF_BUCKETS;
		g_frame_ticks[b]++;
		g_frames++;
	}
	(void)stereo_pass; /* stereo attribution is done offline via RENDERCAM count/frames */
}

void perf_dump(void)
{
	static char line[128];
	static char line2[128];
	int i;
	extern void trace(const char *);

	trace("PERF: dump begin");
	for (i = 0; i < PERF_S_MAX; i++) {
		snprintf(line, sizeof(line),
		         "PERF: stage=%d count=%lu total=%llu max=%lu",
		         i, g_stage[i].count, g_stage[i].total, g_stage[i].max);
		trace(line);
	}
	for (i = 0; i <= PERF_BUCKETS; i++) {
		if (g_frame_ticks[i]) {
			snprintf(line2, sizeof(line2), "PERF: hist ms<=%d count=%lu",
			         i == PERF_BUCKETS ? 9999 : i + 1, g_frame_ticks[i]);
			trace(line2);
		}
	}
	snprintf(line, sizeof(line), "PERF: frames=%lu", g_frames);
	trace(line);
	trace("PERF: dump end");
}

#endif /* PERF_CAPTURE */
