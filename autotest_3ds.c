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

#include <stdbool.h>

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
/* Default: full 32-level sweep from slot 0. Override AUTOTEST_FIRST_LEVEL
 * at compile time to focus on a subset (e.g. -DAUTOTEST_FIRST_LEVEL=5
 * for Night-Dive + N64 only). */
#ifndef AUTOTEST_FIRST_LEVEL
#define AUTOTEST_FIRST_LEVEL  0
#endif

#ifndef AUTOTEST_MAX_LEVELS_TO_SWEEP
#define AUTOTEST_MAX_LEVELS_TO_SWEEP  32
#endif

/* Two transition modes:
 *   AUTOTEST_MODE_HOTJUMP — manual cleanup (ReleaseLevel + ReleaseView +
 *                           InitScrPolys) followed by a direct jump to
 *                           STATUS_StartingSinglePlayer. Skips the
 *                           engine's natural ViewingStats / BetweenLevels
 *                           menu chain. Diagnostic mode for measuring
 *                           how robust the manual cleanup is.
 *   AUTOTEST_MODE_NATURAL — set NewLevelNum and let oct2.c's own
 *                           STATUS_SinglePlayer detector
 *                           (`NewLevelNum != LevelNum`) trigger
 *                           STATUS_ViewingStats. autotest_between_levels_tick
 *                           then auto-confirms the inter-level menu by
 *                           calling StartASinglePlayerGame(NULL) after
 *                           a short dwell. Mirrors the path a real
 *                           player takes by exiting via the level's
 *                           end portal — the path that has worked since
 *                           1998. Use this when validating that a level
 *                           itself is intact. */
#define AUTOTEST_MODE_HOTJUMP  0
#define AUTOTEST_MODE_NATURAL  1
#ifndef AUTOTEST_MODE
#define AUTOTEST_MODE  AUTOTEST_MODE_NATURAL
#endif

/* Per-level dwell measured in WALL-CLOCK milliseconds via osGetTime(),
 * not a frame counter. tuben64's water mesh + lighting brought the
 * frame rate down to ~0.6 fps after the water cap was raised — at the
 * old 1800-frame budget that meant ~52 minutes of wall-clock just to
 * advance past one level. With wall-time we sweep on schedule
 * regardless of per-level frame rate; visual inspection still works
 * because the user reads the screen in real time, not in frames. */
#define AUTOTEST_MS_PER_LEVEL  60000   /* 1 minute */

static u64  s_level_enter_time_ms = 0;
static int  s_last_level_logged = -1;
static bool s_done = false;

/* Natural-mode state. When s_advancing is true, autotest_tick is just
 * waiting for the engine's natural ViewingStats → BetweenLevels →
 * StartASinglePlayerGame chain to deposit us back in STATUS_SinglePlayer
 * with the new LevelNum. The "entered" detector below clears it. */
static bool s_advancing = false;
static int  s_target_level_natural = -1;
static int  s_between_levels_frames = 0;

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
		s_level_enter_time_ms = osGetTime();
		s_advancing = false;
		s_between_levels_frames = 0;
		s_target_level_natural = -1;
		return;
	}

	/* Natural mode: once we've armed an advance, just wait for the
	 * engine to land us on the target level. autotest_tick may continue
	 * firing during the brief window before STATUS_SinglePlayer hands
	 * off to STATUS_ViewingStats, so don't double-arm. */
	if (s_advancing) return;

	if ((osGetTime() - s_level_enter_time_ms) < AUTOTEST_MS_PER_LEVEL) return;

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
	/* End-of-sweep detection. Three conditions that all stop the
	 * sweep: real mission end (empty level name), array end
	 * (next >= NumLevels), or hitting the AUTOTEST_MAX_LEVELS_TO_SWEEP
	 * cap. The cap lets us short-circuit a full 32-level run during
	 * focused diagnosis (e.g. perf work on the early levels). */
	int swept = (next - AUTOTEST_FIRST_LEVEL);
	if (next >= NumLevels
	    || ShortLevelNames[next][0] == '\0'
	    || swept >= AUTOTEST_MAX_LEVELS_TO_SWEEP)
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
		         "AUTOTEST: dwell elapsed in LevelNum=%d (%s) — advancing to %d (%s)",
		         (int)LevelNum, &ShortLevelNames[LevelNum][0],
		         next, &ShortLevelNames[next][0]);
		trace(buf);
	}

#if AUTOTEST_MODE == AUTOTEST_MODE_NATURAL
	/* NATURAL MODE
	 * ---------------------------------------------------------------
	 * Just set NewLevelNum and let the engine handle everything else.
	 * oct2.c's STATUS_SinglePlayer case at ~line 4163 already detects
	 * NewLevelNum != LevelNum on the same frame and transitions:
	 *   SP -> ViewingStats (ReleaseLevel, ReleaseView, InitScene,
	 *                       MenuRestart for stats)
	 *      -> BetweenLevels (DisplayTitle with menu)
	 *      -> WaitingToStartSinglePlayer (VDU finishes)
	 *      -> StartASinglePlayerGame -> TitleStartingSinglePlayer
	 *      -> CLPIV -> SP (with new LevelNum).
	 *
	 * The inter-level menu blocks until the player confirms; we
	 * auto-confirm via autotest_between_levels_tick().
	 *
	 * s_advancing prevents autotest_tick from re-arming on subsequent
	 * frames while the engine works through the chain. The "entered"
	 * branch above clears it once the new level is live. */
	NewLevelNum = next;
	s_target_level_natural = next;
	s_advancing = true;
	s_between_levels_frames = 0;
	trace("AUTOTEST: natural transition armed (NewLevelNum set, awaiting engine)");
#else
	/* HOT-JUMP MODE
	 * ---------------------------------------------------------------
	 * Bypass the engine's ViewingStats / BetweenLevels chain. Manually
	 * release the level + view, then jump straight to
	 * STATUS_StartingSinglePlayer. STATUS_StartingSinglePlayer's
	 * handler resets LevelNum to -1 and calls ChangeLevel() to load
	 * the new NewLevelNum value as a fresh level.
	 *
	 * STATUS_StartingSinglePlayer's own ReleaseView() doesn't free
	 * the level's model / texture / Mloadheader buffers (those are
	 * gated on the OUTGOING status, and StartingSinglePlayer is in
	 * the skip-list of ReleaseLevel/ReleaseScene). So we call
	 * ReleaseLevel + ReleaseView ourselves NOW while MyGameStatus is
	 * still STATUS_Normal — otherwise each hot-jump leaks a level's
	 * worth of linearAlloc.
	 *
	 * CRITICAL: end the current GPU frame BEFORE releasing buffers.
	 * autotest_tick runs between MainGame()'s render and
	 * render_flip()/pglSwapBuffers(), so s_inFrame == true and the
	 * C3D command stream has pending references to vertex/normal/
	 * index buffers we're about to free. Without flushing first, the
	 * dangling commands stay queued and the next level's first frame
	 * trips GPUCMD_AddInternal's full-buffer svcBreak.
	 *
	 * Also clear the screen-poly list — gameplay frames added HUD /
	 * weapon / VDU text polys that reference textures we just freed.
	 * CLPIV's PrintInitViewStatus renders the entire ScrPolys list;
	 * stale polys overflow citro3d's command buffer. The natural
	 * ViewingStats path eventually clears this list on its own. */
	NewLevelNum = next;
	extern void pglSwapBuffers(void);
	extern void ReleaseLevel(void);
	extern void ReleaseView(void);
	extern void InitScrPolys(void);
	trace("AUTOTEST: pglSwapBuffers (close current GPU frame)");
	pglSwapBuffers();
	trace("AUTOTEST: ReleaseLevel + ReleaseView before transition");
	ReleaseLevel();
	ReleaseView();
	trace("AUTOTEST: InitScrPolys (clear stale screen-poly list)");
	InitScrPolys();
	trace("AUTOTEST: release done");
	LevelNum = next;
	extern BYTE MyGameStatus;
	MyGameStatus = STATUS_StartingSinglePlayer;
	s_level_enter_time_ms = osGetTime();

#ifdef VERBOSE_TRACE
	/* Arm the FLIP enter/exit traces so we can tell if pglSwapBuffers
	 * hangs after the hot-jump completes. ~30 swaps covers SSP frame +
	 * all CLPIV sub-states + first few SP frames. */
	extern int _vt_flip_remaining;
	_vt_flip_remaining = 30;
#endif
#endif /* AUTOTEST_MODE */
}

/* Called once per frame from oct2.c's STATUS_BetweenLevels case while
 * we are mid-natural-transition. The engine sits in BetweenLevels
 * displaying the inter-level menu (MENU_NEW_NumberOfCrystals). After a
 * short dwell we call StartASinglePlayerGame(NULL) — the same call the
 * engine itself makes from STATUS_WaitingToStartSinglePlayer once VDU
 * playback finishes — which dismisses the menu and advances to
 * STATUS_TitleStartingSinglePlayer / CLPIV / next level load.
 *
 * Only fires while s_advancing is true so it doesn't auto-skip the
 * very first menu the user might want at boot. */
void autotest_between_levels_tick(void)
{
	if (s_done) return;
	if (!s_advancing) return;

	s_between_levels_frames++;
	/* ~30 frames at 60 Hz = 0.5 s in BetweenLevels before we confirm.
	 * Long enough for the menu / VDU plumbing to finish initialising,
	 * short enough that the autotest sweep doesn't drag. */
	if (s_between_levels_frames == 30)
	{
		trace("AUTOTEST: BetweenLevels dwell elapsed -> StartASinglePlayerGame");
		/* MENUITEM* is the formal type but autoboot in oct2.c also
		 * passes NULL — engine treats NULL as "no menu context, just
		 * proceed". Using void* to avoid pulling menu.h here. */
		extern bool StartASinglePlayerGame(void *);
		StartASinglePlayerGame(NULL);
	}
}

#else  /* !AUTOTEST_REMASTER */

bool autotest_active(void)              { return false; }
int  autotest_first_level(void)         { return 0; }
void autotest_tick(void)                {}
void autotest_between_levels_tick(void) {}
void autotest_on_serious_error(void)    {}

#endif
