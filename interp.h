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
/* Call after the real render: publishes this frame's camera as "previous". */
void interp_frame_end(void);
/* 1 once a previous frame exists (i.e. from the second frame onwards). */
int  interp_ready(void);
/* Write lerp(prev, cur, alpha) into the live camera and rebuild the view. */
void interp_apply(float alpha);
/* Restore the true (current) camera and rebuild the view. */
void interp_restore(void);
/* Configured alpha for the in-between frame (INTERP_ALPHA, default 0.5). */
float interp_alpha(void);

#else

#define interp_frame_begin()  ((void)0)
#define interp_frame_end()    ((void)0)
#define interp_ready()        (0)
#define interp_apply(a)       ((void)0)
#define interp_restore()      ((void)0)
#define interp_alpha()        (0.0F)

#endif

#endif
