# Temporal interpolation: fishy extra-present prototype

Hardware: New 3DS, 268 MHz OG clock, L1I/L1D 16 KB, no L2, forced mono.
Build flags: `FRAME_INTERP=1 FRAME_INTERP_FORCE_MONO=1
FRAME_INTERP_EXTRA_PRESENT=1 PERF_CAPTURE=1 AUTOTEST_FIRST_LEVEL=28
AUTOTEST_MAX_LEVELS_TO_SWEEP=1`.
Artifact: `perf-2026-10-30-extra-present-fishy.log`.

## Measurement

| metric | value |
|---|---:|
| RenderLoop iterations (simulation/render frames) | 3171 |
| mean simulation/render FPS | 30.31 |
| current camera traversal | 3.525 ms/frame |
| extra interpolated traversal | 2.601 ms/frame |
| total camera work | 6.970 ms/frame |
| theoretical two-present cadence at measured game rate | ~60.6 presents/s, capped by display at 60 Hz |

An actual 60 Hz P-counter overlay was added to the build after this capture,
but the artifact does not contain that readout as structured data. It is
important not to claim a directly measured 60 Hz output rate from this file;
the supported result is that the device ran ~30.3 simulation/render updates/s
and the extra pass cost ~2.6 ms, making two presents per game update
technically plausible on this light fishy scene.

This is **not** doubled simulation throughput. Input, AI, physics, and the
simulation tick still execute once per update; the second presentation is an
interpolated camera/model pose with no game-state advance. For heavy levels,
the cost may exceed the time saved by presenting more often, and the Old 3DS
has less headroom than this New 3DS run. No claim of 60 Hz across levels or
both hardware classes is supported yet.

## Build and display-path caveats

- Interpolation is forced mono. Stereo remains unsupported by the prototype.
- Camera and visible model positions/rotations are interpolated. Model rotation
  uses quaternion slerp; world translations are lerped. Gameplay state is
  restored before continuing.
- `FRAME_INTERP_EXTRA_PRESENT` currently uses the renderer's normal
  `platform_render_present`/`C3D_FrameEnd` path for the synthetic frame, then
  skips the ordinary outer-loop present to avoid presenting the same buffer
  twice. The P-counter runs in `RenderLoop` and reports outer-loop presents/s.
- The FPS measurement is an in-engine average; loading/transition hitches
  distort mean FPS. Per-level steady-state pacing and visible artifact rate
  need separate validation.
