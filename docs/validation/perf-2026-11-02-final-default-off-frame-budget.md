# Final default-off frame-budget assessment — OG-profile proxy

Date: 2026-11-02. Loop: `ralph-pi`, `experimental/native-renderer`.

Purpose: bank a frame-budget assessment for the **shipped default-off tree**
(`MODEL_BUCKET` opt-in), closing the gap left by iter 004, whose assessment was
taken on the iter 003 default-on tree before iter 006 reverted the default.
This is required by `docs/validation/ralph-pi-spec.md` §8 clause 4.

## 1. Tree under test

| Item | Value |
|---|---|
| Engine delta vs `HEAD` | iter 001 comment fixes in `render_c3d.c:9-13`, `render_c3d.c:2772-2775` only |
| `MODEL_BUCKET` default | off (`Makefile.3ds:39-42`); bucket path compiled only with `MODEL_BUCKET=1` |
| Renderer | Citro3D (`RENDERER=citro3d`), picaGL not linked |
| Build flags | `EDITION=remaster AUTOTEST_REMASTER=1 AUTOTEST_MAX_LEVELS_TO_SWEEP=1 AUTOTEST_MS_PER_LEVEL=8000 PERF_CAPTURE=1 OG_PROFILE=1 RENDERER=citro3d SLIM=1` |
| Captured binary | `forsaken3ds.3dsx`, md5 `eba28c890c6a123804d85a555c757b2e` (see §5) |

The rebuilt binary is byte-identical to iter 006's final default-off binary and
iter 007's smoke binary, so this assessment describes the shipped default
configuration, not an experimental one.

## 2. Measurement environment

| Field | Value |
|---|---|
| Host | Mandarine emulator (Citra-derived), host-speed guest CPU |
| Emulated profile | `OG_PROFILE=1` proxy: N3DS clocked down to 268 MHz, L2 off, OG heap/texture tiers |
| Level | `vol2` (single level swept, `AUTOTEST_MAX_LEVELS_TO_SWEEP=1`) |
| Duration | 8 s per level (`AUTOTEST_MS_PER_LEVEL=8000`), ~487 frames per capture |
| Stereoscopic mode | **off** (`render_3d=0`, mono) |
| Tick rate used by `analyze_sweep.py` | 268111840 Hz |
| Frame budget | 16.67 ms/frame (60 FPS) |

**This is not a hardware pass.** Mandarine runs the guest CPU at host speed, has
~5 MB linear heap versus 22–24 MB on hardware, lenient clamping, and different
f24 precision. Per `docs/validation/capture-protocol.md` §6 these numbers are
directional only and clear no `AGENTS.md` P6 claim. Stereo is not covered, and
stereo doubles CPU scene traversal in this engine (`AGENTS.md` §6).

## 3. Captures used

All are default-off (`MODEL_BUCKET` unset) on the same binary.

| Capture | fps | frame ms (worst excluded) |
|---|---|---|
| `perf-20261102-090712-iter6-final-default.log` | 51.0 | 19.618 |
| `perf-20261102-090914-iter6-final-default2.log` | 51.0 | 19.614 |
| `perf-20261102-101949-iter6-final-default.log` | 51.0 | 19.612 |
| `perf-20261102-102107-iter6-final-default2.log` | 51.1 | 19.577 |
| `perf-20261102-104802-iter7-final-smoke.log` | 51.0 | 19.598 |
| `perf-20261102-112725-iter8-final-default.log` | 51.0 | 19.607 |
| `perf-20261102-112810-iter8-final-default2.log` | 50.4 | 19.857 |

Artifacts live in `build_3ds/ralph-pi/evidence/`, each with a `.analysis.txt`
produced by `tools/perf/analyze_sweep.py`.

## 4. Budget decomposition (mean of the 7 captures)

| Quantity | ms/frame | vs 16.67 ms budget |
|---|---|---|
| Total frame, worst frame excluded | **19.640** | **+2.970 over** |
| `stage=3 fsbegin` (`C3D_FrameBegin` queue/frame-acquire wait) | 10.202 | — |
| `stage=6 fsend` | 0.001 | — |
| **Total frame excluding `fsbegin`+`fsend`** | **9.438** | **7.232 under** |
| `MainGame` excluding waits | 7.922 | 8.748 under |
| `MainGameRender` excluding waits | 5.848 | 10.822 under |

Largest steady CPU stages (mean ms/frame, waits excluded):

| Stage | ms/frame |
|---|---|
| `stage=5 cam` (`RenderCurrentCamera`) | 5.531 |
| `stage=9 opaque` | 2.028 |
| `stage=4 mROUT` (`MainRoutines`) | 1.788 |
| `stage=10 trans` (transparent) | 1.389 |
| `stage=18 bullets` | 1.352 |
| `stage=41 bullprim` | 1.292 |
| `stage=8 bg` | 0.777 |
| `stage=55 mdlwrap` / `stage=46 opqmodel` | 0.454 each |
| `stage=7 vis` | 0.287 |

Frame-time histogram, representative capture
(`perf-20261102-112725-iter8-final-default.log`, 487 frames): 15 frames ≤ 16 ms,
457 frames in 16–24 ms, 8 in 24–32 ms, 7 outliers above 32 ms (level-load and
teardown spikes).

## 5. Interpretation

1. **The CPU-side work is not the binding constraint on this proxy.** Simulation
   plus render preparation, with queue waits removed, is 9.438 ms/frame —
   7.232 ms inside the 60 FPS budget. `MainGame` alone is 7.922 ms.
2. **The measured frame is 2.970 ms over budget because of queue/present
   waiting, not CPU work.** `stage=3 fsbegin` is 10.202 ms/frame, more than half
   the measured frame. `stage=6 fsend` is ~0 because `FSEndScene()` is a no-op
   (`render_c3d.c:1552`); the real `C3D_FrameEnd(0)` happens later in
   `render_flip()` / `pglSwapBuffers()` (`render_c3d.c:426-431`,
   `render_c3d.c:1443-1448`), so present/pacing cost lands outside the
   instrumented stages.
3. **This is consistent with the iter 002 and iter 006 A/B results.** Both
   measured CPU reductions (~0.64 ms/frame for the Item 6 indexed cache,
   ~0.70 ms/frame for `MODEL_BUCKET`) reappeared almost exactly as additional
   `fsbegin` wait with unchanged total frame time and unchanged analyzer FPS.
   On this proxy, in the vol2 mono scene, shaving CPU from render preparation
   does not convert into FPS; the frame is gated elsewhere.
4. **Consequence for the mission.** Further per-entity CPU trims of the Item 7
   kind are not expected to move OG-proxy FPS in this scene. The remaining
   lever that could is the frame-acquire/present path itself, which is exactly
   the part Mandarine models least faithfully. That is a hardware question, not
   an emulator question.

## 6. Reproducibility

Build (exit 0; log empty because the tree was already up to date):

```
export DEVKITARM=/opt/devkitpro/devkitARM DEVKITPRO=/opt/devkitpro
make -f Makefile.3ds EDITION=remaster AUTOTEST_REMASTER=1 \
     AUTOTEST_MAX_LEVELS_TO_SWEEP=1 AUTOTEST_MS_PER_LEVEL=8000 \
     PERF_CAPTURE=1 OG_PROFILE=1 RENDERER=citro3d SLIM=1 -j"$(nproc)"
```

Gates (exit 0): `tools/perf/check.sh` → `gates_vfp`, `audit_arm11_math`,
`audit_r11_idiv`, `audit_cxx_class` all PASS.

Binary checksums:

```
eba28c890c6a123804d85a555c757b2e  forsaken3ds.3dsx
eba28c890c6a123804d85a555c757b2e  build_3ds/iter6-current-final/forsaken3ds.3dsx
aa7f59875ecb002144afb27faf15b317  build_3ds/iter6-current-bucket/forsaken3ds.3dsx
```

Captures (each exit 0):

```
EV="build_3ds/ralph-pi/evidence/perf-20261102-112725-iter8-final-default.log"
tools/perf/emu_sweep.sh --dsx forsaken3ds.3dsx --out "$EV"
python3 tools/perf/analyze_sweep.py "$EV" "$EV.analysis.txt"
```

and the same pair for `perf-20261102-112810-iter8-final-default2.log`.

## 7. Remaining gap (human step)

Not covered by any artifact here:

- Real OG hardware at 268 MHz with L1 16K/16K and no L2, clock/cache settings
  recorded per `capture-protocol.md` §2.
- Stereo mode (doubles CPU scene traversal).
- The render-heavy `powerdown` / `ramqan` scenes where the prior hardware A/B
  showed `MODEL_BUCKET` gains.
- In-game visual sanity for `MODEL_BUCKET=1`, which is why it stays opt-in.
- Whether Mandarine's `fsbegin` attribution matches PICA200 frame-acquire
  behaviour. If the hardware wait is smaller, the CPU headroom measured here
  could convert into FPS on hardware; if it is larger, it will not.
