/* framedump.h — triggerable framebuffer dump library for 3DS games.
 *
 * Purpose: let an autonomous test driver capture proof that a game is
 * rendering with the lid closed, where LCD screenshots cannot work
 * (no foreground app, panels blank). The game saves its OWN picture
 * data to its memory card — nothing outside the game can reach in,
 * so the game reports out.
 *
 * Two ways to trigger a dump:
 *
 *   1. Remote trigger (the test-driver path). framedump_poll() is
 *      called once per main-loop frame (main.c, next to
 *      PERF_FRAME_END()); the first poll brings up the listener on
 *      FRAMEDUMP_PORT (default 17493, override with framedump_init(port)
 *      before the first poll).
 *      The driver captures with one line of shell:
 *
 *          printf 'DUMP boss1\n' | nc <3ds-ip> <port>
 *
 *      The game replies "OK <setname>" and writes
 *      sdmc:/framedump_<seq>_<tag>_top.bmp (+ _bottom.bmp) plus one
 *      hash line in sdmc:/framedump.log. "PING" replies "PONG" and
 *      dumps nothing (liveness check).
 *
 *   2. In-code trigger. Call framedump_request("tag") from anywhere
 *      (AI event, level marker, test hook). No network needed.
 *
 * Build: compiled in only with -DFRAMEDUMP (see Makefile.3ds
 * FRAMEDUMP=1, same pattern as FIDELITY_DUMP). Default builds are
 * unaffected. The per-frame cost when idle is one non-blocking
 * accept() that returns at once with no client waiting.
 *
 * Validation (no console needed): tests/test_framedump.c exercises
 * the portable core plus a real loopback trigger exchange on Linux.
 */
#ifndef FRAMEDUMP_INCLUDED
#define FRAMEDUMP_INCLUDED

#ifdef FRAMEDUMP

/* Start the trigger listener on the given TCP port. Returns 0 when the
 * listener is live, -1 when networking is unavailable — the game keeps
 * running either way; in-code triggers still work. */
int framedump_init(unsigned short port);

/* Poll the listener once per frame. Safe to call when init failed. */
void framedump_poll(void);

/* Dump both screens now under the given tag (letters/digits/_/- only,
 * anything else becomes '_'). Returns the dump sequence number, or -1
 * when storage failed. Safe to call from anywhere, including AI code. */
int framedump_request(const char *tag);

#else

#define framedump_init(p) (0)
#define framedump_poll() ((void)0)
#define framedump_request(t) (-1)

#endif /* FRAMEDUMP */

#endif /* FRAMEDUMP_INCLUDED */
