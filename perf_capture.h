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
 * Tick source is svcGetSystemTick() (32768 Hz); conversion to ms is done
 * offline so on-device cost is one tick read per gate.
 */
#ifndef PERF_CAPTURE_INCLUDED
#define PERF_CAPTURE_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#ifdef PERF_CAPTURE

enum perf_stage {
	PERF_S_FRAME = 0,   /* one full aptMainLoop iteration (main.c) */
	PERF_S_MAINGAME,    /* MainGame  (oct2.c) — simulation tick */
	PERF_S_RENDER,      /* MainGameRender (oct2.c) — camera/scene render */
	PERF_S_MAX
};

void perf_stage_begin(int stage);
void perf_stage_end(int stage, int stereo_pass);
void perf_dump(void);

bool perf_call_maingame(void);
bool perf_call_maingamerender(void);

#define PERF_MAINGAME()         perf_call_maingame()
#define PERF_MAINGAME_RENDER()  perf_call_maingamerender()
#define PERF_FRAME_BEGIN()      perf_stage_begin(PERF_S_FRAME)
#define PERF_FRAME_END()        perf_stage_end(PERF_S_FRAME, -1)
#define PERF_DUMP()             perf_dump()

#else /* !PERF_CAPTURE */

bool MainGame(void);
bool MainGameRender(void);

#define PERF_MAINGAME()         MainGame()
#define PERF_MAINGAME_RENDER()  MainGameRender()
#define PERF_FRAME_BEGIN()      ((void)0)
#define PERF_FRAME_END()        ((void)0)
#define PERF_DUMP()             ((void)0)

#endif /* PERF_CAPTURE */

#ifdef __cplusplus
}
#endif

#endif /* PERF_CAPTURE_INCLUDED */
