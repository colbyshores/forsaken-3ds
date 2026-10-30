# Temporal frame interpolation — feasibility against RT64-3DS

Reference studied: `oneup03/rt64-3ds` (local clone `/tmp/rt64-3ds`), files
`src/renderer/rt64_3ds_interp.{h,cpp}`, `rt64_3ds_dl.cpp`,
`shaders/rt64_3ds.v.pica`.

## What RT64-3DS actually does

It is **temporal** interpolation, not vertex morphing: the game's frame N and
N-1 are both recorded, the transforms in N are *matched* against N-1
(`interp::match`, per transform group id / RDRAM address / first-vertex-load
key), a rigid-body lerp is computed per component (INTERPOLATE / SKIP / AUTO,
with quaternion slerp and projection handling), and the display list is
re-executed with the interpolated transforms to produce an in-between frame
rendered at display rate. Where a transform's *vertices* moved, the
interpreter lerps them on the CPU during the replay pass
(`rt64_3ds_dl.cpp:1599-1603`). The vertex shader does transform and shading
only (`rt64_3ds.v.pica`) — the interpolation itself is not a shader effect.

Its cost model is explicit in the README: the in-between work is **built on
the spare cores** (the second Old-3DS core / the New-3DS extras). That is what
makes a second render pass affordable there.

## What Forsaken would need, and why it is not a drop-in

| Requirement | Forsaken today |
|---|---|
| Record per-frame draw transforms + loaded vertices | nothing recorded; `render_c3d.c` submits per group from live state |
| Match current vs previous transforms | no identity/key system; matrices are rebuilt per frame in `models.c`/`visi.c` |
| Re-execute the render traversal at weight w | `RenderCurrentCamera` costs 4.5–11.5 ms/frame **mono** (waves 4–7); a second pass costs the same again |
| Spare-core capacity for that second pass | engine is single-threaded for rendering; `P2` says rendering state has one owner (main thread); the only worker is audio (`P4`) |
| GPU-side replay | AGENTS.md §6 records that **stereo display-list replay was disabled as a measured pessimisation** — this tree's precedent says replay costs more than re-traversing, at least in the stereo case |

Two hard consequences:

1. **A CPU second pass roughly doubles render cost.** On ramqan/powerdown
   (cam 11.3/11.5 ms/frame, 19–21 fps) a second traversal would land near
   10 fps. The simulation can't be smoothed by making each displayed frame
   more expensive than the frames it interpolates.
2. **The GPU-replay shortcut is contradicted by this tree's own measurement.**
   Display-list replay was tried for stereo and disabled; the requirement for
   a cheap in-between (re-submit geometry, change only transform uniforms)
   runs straight into the fact that this renderer bakes transforms and
   per-vertex lighting into CPU-side vertex data each frame.

## What would have to be true for it to work here

- The perframe CPU vertex-transform + lighting work must move to spare cores
  (RT64-3DS's own answer), i.e. a multi-core renderer architecture inside the
  `P2`/`P4` constraints: recording on the main thread, transform/lighting on
  workers, submission on the main thread, with `R1` (FPSCR) per worker and
  bounded stacks per `P4`.
- Or the renderer must be restructured so a displayed frame is a *cheap
  replay* of retained geometry with fresh matrices — which is a renderer
  rewrite, and directly conflicts with the measured stereo-replay result
  unless the replay path is made substantially cheaper than today's.

## Proposed decisive measurement (cheap, reversible)

Before any of that: measure the **cost of a second render traversal with
identical state** on the worst levels. That is the floor for any CPU-side
interpolation scheme.

`FRAME_INTERP_PROBE=1` (build flag, default off) runs `RenderCurrentCamera`
twice per rendered frame with unchanged state and reports the second pass via
a dedicated perf stage (`PERF_S_INTERPPROBE`). Outcomes:

- Second pass ≈ first pass (within ~10%): CPU interpolation is dead on this
  hardware for the heavy levels — document and stop.
- Second pass materially cheaper: there is headroom worth designing against,
  and the worker-core architecture becomes the next project.

This probe changes no rendering output (the second pass draws the identical
frame; only the last submission survives), so it is safe and reversible, and
it is gated behind a flag that never ships.
