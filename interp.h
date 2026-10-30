/* interp.h — temporal frame interpolation prototype (RT64-3DS style).
 *
 * Reference studied: oneup03/rt64-3ds. Its "interpolated frames" are
 * TEMPORAL: the game keeps simulating at its own rate and the renderer
 * produces extra in-between views by interpolating transforms between two
 * simulation snapshots. It is presentation smoothing, not a faster
 * simulation — the extra views cost render work of their own.
 *
 * This prototype interpolates the CAMERA only (the cheapest meaningful
 * version, and the dominant source of perceived motion in this engine):
 * between the previous frame's camera and the current one, an extra frame
 * at `alpha` is rendered. Entities are deliberately left at their true
 * transforms for the first measurement, so the result isolates the cost and
 * visual effect of an interpolated pass.
 *
 * All entry points are no-ops unless FRAME_INTERP is defined, so default
 * builds are unaffected.
 */
#ifndef INTERP_INCLUDED
#define INTERP_INCLUDED

#ifdef FRAME_INTERP

/* Call before the real camera render: snapshots the current camera. */
void interp_frame_begin(void);
/* True when this simulation frame captured a camera eligible for an extra present. */
int  interp_pending(void);
/* Snapshot models after MainRoutines updates their current transforms. */
void interp_capture_models(void);
/* Present the pending interpolated frame and restore game transforms. */
void interp_present_current(void);
void interp_present(void);
void interp_present_window_tick(void);
int  interp_present_fps(void);
void interp_begin_extra_present(void);
void interp_end_extra_present(void);
int  interp_replay_active(void);
int  interp_consume_outer_present_skip(void);
/* Call after the in-between render: publishes this frame as "previous". */
void interp_frame_end(void);
/* 1 once a previous frame exists (i.e. from the second frame onwards). */
int  interp_ready(void);
/* Write lerp(prev, cur, alpha) into the live camera and rebuild the view. */
void interp_apply(float alpha);
/* Restore the true (current) camera and rebuild the view. */
void interp_restore(void);
/* Drop temporal history on level/camera discontinuities. */
void interp_reset(void);
/* Configured alpha for the in-between frame (INTERP_ALPHA, default 0.5). */
float interp_alpha(void);

#else

#define interp_frame_begin()  ((void)0)
#define interp_pending()      (0)
#define interp_capture_models() ((void)0)
#define interp_present_current() ((void)0)
#define interp_present()       ((void)0)
#define interp_present_window_tick() ((void)0)
#define interp_present_fps() (0)
#define interp_begin_extra_present() ((void)0)
#define interp_end_extra_present() ((void)0)
#define interp_replay_active() (0)
#define interp_consume_outer_present_skip() (0)
#define interp_frame_end()    ((void)0)
#define interp_ready()        (0)
#define interp_apply(a)       ((void)0)
#define interp_restore()      ((void)0)
#define interp_reset()        ((void)0)
#define interp_alpha()        (0.0F)

#endif

#endif
