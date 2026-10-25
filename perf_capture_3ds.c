/* perf_capture_3ds.c — backing counters for perf_capture.h.
 * Entire file compiles to nothing unless PERF_CAPTURE is defined, so the
 * wildcard-built default binary is byte-identical with this file present.
 *
 * Evidence recorded per stage: call count, total ticks, max ticks (worst
 * frame for that stage). Plus a frame-tick histogram (64 buckets, linear
 * 1 ms each up to 64 ms at 71 Hz budget ~14.1 ms) for the whole frame.
 * All on-device cost: one svcGetSystemTick() per gate, integer adds.
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
		unsigned b = (unsigned)(d / 33ULL); /* ~1 ms buckets (2^15 Hz tick) */
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
		g_perf_fd = open("sdmc:/forsaken_perf.txt",
		                 O_WRONLY | O_CREAT | O_TRUNC, 0666);
		if (g_perf_fd < 0)
			return;
	}
	len = (int)strlen(msg);
	write(g_perf_fd, msg, len);
	write(g_perf_fd, "\n", 1);
	fsync(g_perf_fd);
}

void perf_dump(void)
{
	static char line[128];
	static char line2[128];
	int i;

	perf_out("PERF: dump begin");
	for (i = 0; i < PERF_S_MAX; i++) {
		snprintf(line, sizeof(line),
		         "PERF: stage=%d count=%lu total=%llu max=%lu",
		         i, g_stage[i].count, g_stage[i].total, g_stage[i].max);
		perf_out(line);
	}
	for (i = 0; i <= PERF_BUCKETS; i++) {
		if (g_frame_ticks[i]) {
			snprintf(line2, sizeof(line2), "PERF: hist ms<=%d count=%lu",
			         i == PERF_BUCKETS ? 9999 : i + 1, g_frame_ticks[i]);
			perf_out(line2);
		}
	}
	snprintf(line, sizeof(line), "PERF: frames=%lu", g_frames);
	perf_out(line);
	perf_out("PERF: dump end");
}

#endif /* PERF_CAPTURE */
