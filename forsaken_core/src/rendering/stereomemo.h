/* stereomemo.h — one-frame memo of converted vertex ranges, keyed by
 * batch identity plus a fill epoch, for single-pass stereo
 * (STEREO_REPLAY path).
 *
 * Pure C99 + stdint/stdbool only: no Citro3D, no platform headers, so the
 * table logic is host-testable (tests/test_stereomemo.c). The render
 * backend owns arming (eye 1 records, eye 2 looks up); entries never
 * outlive a frame — a generation counter invalidates them, no clearing.
 *
 * Key stability contract (2026-11-13 revision): (renderObject, texgroup
 * id, ortho, prim) alone is NOT a stable identity. Shared exec-buffer
 * slots (RenderBufs[2] for polys/FmPolys across visible groups, and
 * friends) are refilled per batch with texgroups restarting at 0, so the
 * same key addresses different vertex content within one eye pass — eye
 * 1's later groups overwrite earlier groups' records, and eye 2's
 * lookups hit the wrong group's bytes (proven on hardware: 78052/106018
 * verify mismatches, first word divergent). Every fill therefore stamps
 * its content identity (pass, group, resume index — see SMEMO_PASS_* in
 * render.h) and every entry carries it; a lookup hits only when the
 * current fill's epoch matches, so aliasing degrades to a safe
 * live-convert miss instead of a false hit. The resume point must
 * include the poly a fill starts from, not just its texture page: a
 * fill that runs out of vertex space mid-page resumes on the same page,
 * and a page-only stamp let eye 2 draw another chunk's sprites
 * (corrupted, misplaced explosion and shot quads; fixed 2026-11-17).
 * Converted vertices are eye-independent (world-space pos, baked
 * color, raw UV — lighting is shader-side) EXCEPT where the fill reads
 * the eye position: FM_FLAG_MOVEOUT FmPolys are pulled toward
 * CurrentCamera.Pos, so those fills stamp SMEMO_PASS_NO_REUSE and
 * are never recorded or reused. Fills that hit MAX_TEXTURE_GROUPS
 * mid-fill (one texgroup per sprite quad, busy scenes) add
 * SMEMO_CHUNK_STEP per chunk. Only stamped fills, level walls and water
 * are reused at all (render_c3d memo_reusable): model vertex buffers are
 * rewritten per on-screen copy, TransExe copies are keyed by a reused
 * slot, and the missile inset runs with the memo suspended
 * (c3d_memo_suspend). Otherwise an epoch-confirmed reuse
 * with eye 2's live uniforms and draw order reuses the same bytes
 * eye 1 submitted. It is NOT
 * unconditionally pixel-identical to a live eye-2 convert: batches
 * whose source UVs advance once per frame (texture animation) can
 * epoch-match while content stepped, leaving a one-step UV lag in
 * eye 2 (measured ~9% of frame-15 pixels on animated surfaces;
 * static content is byte-identical). R22 human A/B reviewed this on
 * hardware 2026-11-13 and accepted it; the memo is default-on
 * (STEREO_REPLAY ?= 1, opt-out available). Stable renderObjects
 * (models, water, per-slot execs) never stamp: their fields stay put
 * within a frame, which compares equal.
 */
#ifndef STEREOMEMO_INCLUDED
#define STEREOMEMO_INCLUDED

#include <stdint.h>
#include <stdbool.h>


/* New frame: invalidates all entries (generation bump, no clearing). */
void smemo_begin_frame(void);

/* Mid-frame invalidation: call when shared vertex content actually
 * changes (texture-animation step, water integration) so the other
 * eye never reuses pre-change bytes. */
void smemo_invalidate(void);

/* R05 persistent-geometry generation: bumped ONLY by smemo_invalidate
 * and smemo_persist_reset (level load), never per frame. The render
 * backend keys retained converted buffers by it; a bump makes them
 * reconvert (into the same allocations) instead of drawing stale. */
unsigned smemo_persist_gen(void);
void smemo_persist_reset(void);
/* Monotonic reset count: the backend frees retained buffers when it
 * observes this increase (level unload/load), lazily at frame begin. */
unsigned smemo_persist_resets(void);

/* Record eye 1's converted range for this batch identity plus its fill
 * epoch (pass, group, resume index). Offsets are element counts (never
 * byte addresses); the table only ever narrows them implicitly into int
 * ranges, and the epoch is plain integer compares, so no float or
 * pointer conversion ever appears at a call site. */
void smemo_record(const void *renderObject, int sub,
                   long offset, int count,
                   uint32_t epass, uint32_t epgroup, uint32_t eresume);

/* Eye 2 hit: fills the offset/count outs with the recorded range, but
 * only when the recorded fill epoch matches the current fill's epoch.
 * Otherwise — unknown key, stale generation, or aliased content
 * (different group/pass/resume sharing one slot) — miss: eye 2
 * converts live (the safe fallback — never stale data). */
bool smemo_lookup(const void *renderObject, int sub,
                   long *offset, int *count,
                   uint32_t epass, uint32_t epgroup, uint32_t eresume);

/* Pack (texgroup id, orthographic, primitive) into the sub-key.
 * Pure int arithmetic — texgroup ids are small non-negative array
 * indices, ortho/prim are 0/1, so no conversion ever appears. */
static inline int smemo_sub(int texgroup, int ortho, int prim)
{
	return (texgroup << 2) | (ortho << 1) | prim;
}


#endif
