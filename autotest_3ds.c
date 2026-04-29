/*
 * autotest_3ds.c — autonomous level-cycler for hardware iteration.
 *
 * Compiled only when -DAUTOTEST_REMASTER is set (via the Makefile flag
 * AUTOTEST_REMASTER=1). When enabled:
 *   - The title-screen autoboot path (oct2.c STATUS_Title) starts the
 *     game at AUTOTEST_FIRST_LEVEL instead of NewLevelNum=0.
 *   - Each frame inside gameplay, autotest_tick() is called. After
 *     60 seconds (3600 frames @ 60 Hz) it advances NewLevelNum to the
 *     next entry in mission.dat and forces a level transition.
 *   - When mission.dat is exhausted, traces a final "AUTOTEST: DONE"
 *     line and chain-loads back to sdmc:/3ds/netload_stub.3dsx so the
 *     host can pull the trace.
 *
 * Trace output goes to sdmc:/forsaken_trace.txt — same as everything
 * else. Look for "AUTOTEST:" lines.
 */

#ifdef AUTOTEST_REMASTER

#include <3ds.h>
#include <stdio.h>
#include <string.h>

#include "main.h"
#include "title.h"

extern void trace(const char *msg);
extern int  NumLevels;
extern int16_t NewLevelNum;
extern int16_t LevelNum;
extern char ShortLevelNames[][32];   /* MAXLEVELS x 32 */

/* mission_remaster.dat layout (32 entries):
 *   0..4   1998 SP entries (vol2..fedbankv)
 *   5..23  Remaster's 19 SP additions (defend2..azchb)
 *   24..31 N64 secret levels (nuken64..final)
 *
 * Full-sweep mode: cycle all 32 levels at 60 s each so the user can
 * visually validate the portal fix across every level (1998 base,
 * Remaster SP, N64 ports). The previous BAD-only list mode is kept
 * compiled out via the #define for quick re-enabling during diagnosis. */
#define AUTOTEST_USE_BAD_LIST  0
static const int s_bad_levels[] = { 7, 8, 10, 13 };
#define S_BAD_LEVELS_COUNT (sizeof(s_bad_levels) / sizeof(s_bad_levels[0]))
static int s_bad_idx = 0;
#define AUTOTEST_FIRST_LEVEL  0

/* 30 seconds at 60 Hz. main.c's main loop runs RenderScene → game tick
 * once per video frame, so a flat frame counter is the right unit.
 * 30 s is enough for a visual sanity check at each level when sweeping
 * all 32 in one go (16 minutes total wall-clock at 60 Hz, longer in
 * mandarine where it tends to run sub-realtime). */
#define AUTOTEST_FRAMES_PER_LEVEL  1800

static int  s_frames_in_level = 0;
static int  s_last_level_logged = -1;
static bool s_done = false;

bool autotest_active(void)
{
	return true;
}

int autotest_first_level(void)
{
	if (AUTOTEST_FIRST_LEVEL < NumLevels) return AUTOTEST_FIRST_LEVEL;
	return 0;
}

/* Shared chain-load to stub. Used by both the normal "all levels done"
 * path and the SeriousError abort path so a level that fails to load
 * (InitView returns false, SeriousError flag set in main.c) doesn't
 * leave the device on a Luma crash screen — it just hops back to the
 * stub, ready for the next 3dslink push. */
static void autotest_chainload_to_stub(void)
{
	extern void trace_dump(void);
	trace_dump();
	const char *path = "/3ds/netload_stub.3dsx";
	Handle hbldr;
	if (R_SUCCEEDED(svcConnectToPort(&hbldr, "hb:ldr")))
	{
		u32 pathLen = strlen(path) + 1;
		u32 *cmdbuf = getThreadCommandBuffer();
		cmdbuf[0] = IPC_MakeHeader(2, 0, 2);
		cmdbuf[1] = IPC_Desc_StaticBuffer(pathLen, 0);
		cmdbuf[2] = (u32)path;
		svcSendSyncRequest(hbldr);
		svcCloseHandle(hbldr);
	}
}

/* Called from main.c's SeriousError branch — when the engine couldn't
 * load a level and is about to hard-quit, log which level died and hop
 * back to the netload_stub so we can iterate the next push. */
void autotest_on_serious_error(void)
{
	if (s_done) return;
	s_done = true;
	char buf[160];
	snprintf(buf, sizeof(buf),
	         "AUTOTEST: SeriousError on LevelNum=%d (%s) — chain-loading back to netload_stub",
	         (int)LevelNum,
	         (LevelNum >= 0 && LevelNum < NumLevels)
	             ? &ShortLevelNames[LevelNum][0] : "?");
	trace(buf);
	autotest_chainload_to_stub();
}

/* Called once per gameplay frame from oct2.c's main game loop. */
void autotest_tick(void)
{
	if (s_done) return;

	/* Log when the level actually became active so the trace records
	 * which level is currently being timed. */
	if (LevelNum != s_last_level_logged && LevelNum >= 0 && LevelNum < NumLevels)
	{
		char buf[160];
		snprintf(buf, sizeof(buf),
		         "AUTOTEST: entered LevelNum=%d name=%s",
		         (int)LevelNum, &ShortLevelNames[LevelNum][0]);
		trace(buf);
		s_last_level_logged = LevelNum;
		s_frames_in_level = 0;
		return;
	}

	s_frames_in_level++;
	if (s_frames_in_level < AUTOTEST_FRAMES_PER_LEVEL) return;

	/* Time's up — advance. */
#if AUTOTEST_USE_BAD_LIST
	/* Skip-to-next-bad-level mode: pick the next slot from s_bad_levels[]
	 * rather than incrementing LevelNum. Stops cleanly after the last bad
	 * level so the user gets back to the stub without sitting through
	 * unrelated content. */
	s_bad_idx++;
	if ((size_t)s_bad_idx >= S_BAD_LEVELS_COUNT)
	{
		trace("AUTOTEST: DONE bad-list — chain-loading back to netload_stub");
		s_done = true;
		autotest_chainload_to_stub();
		extern bool QuitRequested;
		QuitRequested = true;
		return;
	}
	int next = s_bad_levels[s_bad_idx];
#else
	int next = LevelNum + 1;
	if (next >= NumLevels)
	{
		trace("AUTOTEST: DONE — chain-loading back to netload_stub");
		s_done = true;
		autotest_chainload_to_stub();
		extern bool QuitRequested;
		QuitRequested = true;
		return;
	}
#endif

	{
		char buf[160];
		snprintf(buf, sizeof(buf),
		         "AUTOTEST: 60s elapsed in LevelNum=%d (%s) — advancing to %d (%s)",
		         (int)LevelNum, &ShortLevelNames[LevelNum][0],
		         next, &ShortLevelNames[next][0]);
		trace(buf);
	}

	NewLevelNum = next;
	/* Setting NewLevelNum alone is not enough: the STATUS_SinglePlayer
	 * case in oct2.c (~line 4128) checks `NewLevelNum != LevelNum` AFTER
	 * autotest_tick() returns and, if true, forces MyGameStatus =
	 * STATUS_ViewingStats which pops the stats screen and bounces back
	 * to the title menu (verified in the trace: ReleaseView status=35,
	 * WTSS: first frame, ...). Set LevelNum = NewLevelNum here so the
	 * mismatch check is silent, then set MyGameStatus to
	 * STATUS_StartingSinglePlayer. STATUS_StartingSinglePlayer's
	 * handler resets LevelNum to -1 and calls ChangeLevel(), which will
	 * load the new NewLevelNum value as a fresh level — true hot-jump
	 * with no menu in between.
	 *
	 * Also: STATUS_StartingSinglePlayer's ReleaseView() doesn't free
	 * the level's model / texture / Mloadheader buffers (those are
	 * gated on the OUTGOING status, and StartingSinglePlayer is in the
	 * skip-list of ReleaseLevel/ReleaseScene). So we must call
	 * ReleaseLevel + ReleaseView ourselves NOW, while MyGameStatus is
	 * still STATUS_Normal — otherwise each hot-jump leaks a level's
	 * worth of linearAlloc, and after 2-3 transitions the linear heap
	 * runs out and FSCreateVertexBuffer starts returning NULL inside
	 * InitModel (verified: spc-sp01 died at xcop400.mxa model #153). */
	/* CRITICAL: end the current GPU frame BEFORE releasing buffers.
	 * autotest_tick runs from STATUS_SinglePlayer in oct2.c, between
	 * MainGame()'s render and main.c's render_flip()/pglSwapBuffers().
	 * That means s_inFrame == true and the C3D command stream has
	 * pending references to vertex/normal/index buffers we're about
	 * to free in ReleaseLevel(). Without flushing first, the dangling
	 * commands stay queued, and the next level's first frame trips
	 * GPUCMD_AddInternal's full-buffer svcBreak() — diagnosed via
	 * Luma3DS dump (PC=svcBreak, LR=GPUCMD_AddInternal) on the
	 * defend2 -> pship hot-jump. */
	extern void pglSwapBuffers(void);
	extern void ReleaseLevel(void);
	extern void ReleaseView(void);
	extern void InitScrPolys(void);
	trace("AUTOTEST: pglSwapBuffers (close current GPU frame)");
	pglSwapBuffers();
	trace("AUTOTEST: ReleaseLevel + ReleaseView before transition");
	ReleaseLevel();
	ReleaseView();
	/* CRITICAL: clear the screen-poly list before the next ChangeLevel
	 * cycle. defend2's gameplay frames added HUD / weapon / VDU text
	 * polys that reference textures we just freed in ReleaseLevel.
	 * CLPIV calls PrintInitViewStatus which renders the entire ScrPolys
	 * list — issuing GPU commands for thousands of stale polys overflows
	 * citro3d's 1MB command buffer and trips GPUCMD_AddInternal's
	 * svcBreak() guard. Diagnosed via Luma3DS dump (PC=svcBreak,
	 * LR=GPUCMD_AddInternal, addtl="3dsx_app") on the defend2 -> pship
	 * hot-jump's PrintInitViewStatus draw step. The engine's normal
	 * level-transition flow goes through STATUS_ViewingStats which
	 * eventually clears the list; the autotest hot-jump skips that
	 * intermediate state and inherits stale polys without this call. */
	trace("AUTOTEST: InitScrPolys (clear stale screen-poly list)");
	InitScrPolys();
	trace("AUTOTEST: release done");
	LevelNum = next;
	extern BYTE MyGameStatus;
	MyGameStatus = STATUS_StartingSinglePlayer;
	s_frames_in_level = 0;

#ifdef VERBOSE_TRACE
	/* Arm the FLIP enter/exit traces so we can tell if pglSwapBuffers
	 * hangs after the hot-jump completes. ~30 swaps covers SSP frame +
	 * all CLPIV sub-states + first few SP frames. */
	extern int _vt_flip_remaining;
	_vt_flip_remaining = 30;
#endif
}

#else  /* !AUTOTEST_REMASTER */

bool autotest_active(void)        { return false; }
int  autotest_first_level(void)   { return 0; }
void autotest_tick(void)          {}
void autotest_on_serious_error(void) {}

#endif
