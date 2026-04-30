/*
 * death_sim_3ds.c — game-over (lose-all-lives) crash repro harness.
 *
 * Compiled only when -DDEATH_SIM is set (via Makefile flag DEATH_SIM=1).
 *
 * The known-broken sequence: in-game → all lives lost → STATUS_QuitCurrentGame
 * → ReleaseView (default branch, traces "default DONE") → ReleaseLevel
 * → STATUS_Title → InitScene → InitView → InitTitle. Trace ends at
 * "default DONE"; the next user-visible artefact is a Luma3DS crash dump.
 *
 * Strategy:
 *   1. Autoboot into a level (vol2). The existing __3DS_DEBUG__-gated
 *      autoboot in oct2.c handles this.
 *   2. Once we're in STATUS_Normal, dwell ~3 seconds of wall-clock
 *      (level fully loaded, ship spawned, frame loop steady).
 *   3. Force MyGameStatus = STATUS_QuitCurrentGame. The state-machine
 *      runs the same teardown chain a real death triggers.
 *   4. If we survive that and land back at the title screen, dwell
 *      another ~3 s, then chain-load to netload_stub so the host can
 *      pull the trace.
 *   5. On crash, Luma writes a dump to sdmc:/luma/dumps/arm11/. The
 *      trace.txt tail still tells us where we got to.
 */

#include <stdbool.h>

#ifdef DEATH_SIM

#include <3ds.h>
#include <stdio.h>
#include <string.h>

#include "main.h"

extern void trace(const char *msg);
extern void trace_dump(void);
extern BYTE MyGameStatus;
extern int16_t LevelNum;
extern bool QuitRequested;

/* Fire the death sequence shortly after gameplay starts. ~30 frames
 * (≈500 ms at 60 fps) is enough for the level to settle and the first
 * gameplay frame to fully render — but short enough that the user
 * doesn't wait. */
#define DEATH_SIM_FRAMES_BEFORE_DEATH    30
#define DEATH_SIM_DWELL_MS_AT_TITLE      3000

typedef enum {
	DS_WAITING_FOR_GAMEPLAY = 0,
	DS_DWELLING_IN_GAMEPLAY,
	DS_DEATH_FIRED,
	DS_DWELLING_AT_TITLE,
	DS_CHAINLOADING,
} ds_state_t;

static ds_state_t s_state = DS_WAITING_FOR_GAMEPLAY;
static u64        s_state_enter_ms = 0;
static int        s_gameplay_frames = 0;

/* Both single-player (STATUS_SinglePlayer=34) and multi/normal-mode
 * (STATUS_Normal=10) count as "in gameplay" for our purposes. The
 * autoboot path takes us through STARTING -> POST_STARTING ->
 * SINGLE_PLAYER, so we only want to start firing once we land on the
 * actual gameplay state. */
static int is_gameplay_status(BYTE st)
{
	return (st == STATUS_Normal) || (st == STATUS_SinglePlayer);
}

static void chainload_to_stub(void)
{
	trace("DEATH_SIM: chain-loading back to netload_stub");
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
	QuitRequested = true;
}

/* Called once per main-loop tick from oct2.c, regardless of state. */
void death_sim_tick(void)
{
	switch (s_state)
	{
	case DS_WAITING_FOR_GAMEPLAY:
		/* Autoboot has fired; LevelNum gets set when StartASinglePlayerGame
		 * lands us in the actual level. Wait for gameplay status (single-
		 * or multiplayer) AND a real level number — autoboot transitions
		 * through several intermediate states, we only want to start
		 * dwelling once gameplay is steady. */
		if (is_gameplay_status(MyGameStatus) && LevelNum >= 0)
		{
			char b[96];
			snprintf(b, sizeof(b),
			         "DEATH_SIM: gameplay reached, status=%d LevelNum=%d — counting frames",
			         (int)MyGameStatus, (int)LevelNum);
			trace(b);
			s_state = DS_DWELLING_IN_GAMEPLAY;
			s_gameplay_frames = 0;
		}
		break;

	case DS_DWELLING_IN_GAMEPLAY:
		s_gameplay_frames++;
		if (s_gameplay_frames >= DEATH_SIM_FRAMES_BEFORE_DEATH)
		{
			char b[96];
			snprintf(b, sizeof(b),
			         "DEATH_SIM: %d frames elapsed — forcing STATUS_QuitCurrentGame",
			         (int)s_gameplay_frames);
			trace(b);
			MyGameStatus = STATUS_QuitCurrentGame;
			s_state = DS_DEATH_FIRED;
			s_state_enter_ms = osGetTime();
		}
		break;

	case DS_DEATH_FIRED:
		/* The state-machine in oct2.c will run the QuitCurrentGame case,
		 * tear down the level, set MyGameStatus = STATUS_Title, then
		 * call InitScene + InitView. If we get back here with
		 * MyGameStatus == STATUS_Title, the teardown survived and we
		 * have the title screen up. Dwell briefly so the user can
		 * confirm the title is rendering, then chainload. */
		if (MyGameStatus == STATUS_Title)
		{
			trace("DEATH_SIM: title screen reached after death — dwelling");
			s_state = DS_DWELLING_AT_TITLE;
			s_state_enter_ms = osGetTime();
		}
		break;

	case DS_DWELLING_AT_TITLE:
		if ((osGetTime() - s_state_enter_ms) >= DEATH_SIM_DWELL_MS_AT_TITLE)
		{
			s_state = DS_CHAINLOADING;
			chainload_to_stub();
		}
		break;

	case DS_CHAINLOADING:
		break;
	}
}

#else  /* !DEATH_SIM */

void death_sim_tick(void) {}

#endif
