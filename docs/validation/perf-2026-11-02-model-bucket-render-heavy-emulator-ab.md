# MODEL_BUCKET render-heavy emulator A/B — ramqan measured, powerdown load-limited

Prepared: 2026-11-02. Ralph-pi iteration 010.

## Purpose and result

This iteration fills the render-heavy scene-coverage gap left by the Item 7
`MODEL_BUCKET` measurements in `ralph-pi-log.md` iter 006 and iter 008. It
changes no engine code and does not change the shipped default: `MODEL_BUCKET`
remains opt-in.

A matched Mandarine A/B on **ramqan** shows that `MODEL_BUCKET=1` removes about
**4.2 ms/frame** of camera/model-dispatch CPU and about **145x** of model-body
executions. Most of that CPU reduction reappears as `C3D_FrameBegin` wait in
this emulator. The residual directional total-frame change was about
**-1.31 ms/frame / +0.049 FPS** (6.091 to 6.140 FPS) across two pairs.

This is **directional emulator evidence, not a hardware pass**. It does not
replace the required human in-game visual-equivalence check, so it does not
justify changing the default.

The **powerdown** pair could not produce gameplay counters: both builds
reached `Mload: num_groups=66` and then raised `SeriousError` before
`MainGame` ran. This is a failed measurement, not a null performance result.
The boot log's last global-model preload checkpoint shows only about 572 KB
of linear memory left, consistent with Mandarine's smaller emulated
linear-memory pool versus the 22–24 MB hardware pool. No explicit allocation
error was logged.

## Environment and scope

- Console: none; Mandarine emulator on the private display `:131`.
- Guest clock/cache policy: `OG_PROFILE=1`, reported as 268 MHz / L2-off.
  Mandarine runs the guest CPU at host speed and does not model PICA200
  frame-acquire timing.
- Display: mono (`render_3d=0`).
- Renderer: Citro3D; picaGL is not linked.
- Workload: `AUTOTEST_REMASTER=1`, one level, 8 seconds per level after
  entry, `PERF_CAPTURE=1`.
- Levels: `powerdown` is mission slot 8; `ramqan` is mission slot 19. Slot 20
  is `temple`; the trace marker is authoritative for every artifact.
- Pair validity: for each scene, the 100 compiler command lines are identical
  after sorting and removing `-DMODEL_BUCKET` from the candidate. Machine
  output is in `build_3ds/ralph-pi/iter10-scenes/verification.log` (exit 0).
- No source or data-layout change was made. The final engine-tree diff
  fingerprint remains
  `fdeac6532e6b97a4139cccae417f4708b275c5023eaf365e4b825a98c9d65451`,
  identical to the iter 009 hardware handoff.

## Matched ramqan A/B

Frame time uses the repository analyzer's convention: the worst `stage=0`
frame is subtracted and the result is normalized by the captured frame count.
Stage times are normalized the same way. `MDLENTRY/game frame` uses the
gameplay-frame count (`stage=1`) so the model-body execution rate is directly
comparable.

| Pair / build | Frame ms | FPS | `cam` ms | `opaque` ms | `opqmodel` ms | `fsbegin` ms | `fsend` ms | MDLENTRY/game frame |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| pair 1, default-off | 164.537 | 6.078 | 15.981 | 7.712 | 6.156 | 4.850 | 0.001 | 2434.6 |
| pair 1, bucket-on | 163.267 | 6.125 | 11.815 | 3.545 | 1.993 | 8.575 | 0.001 | 17.3 |
| pair 2, default-off | 163.831 | 6.104 | 15.966 | 7.705 | 6.143 | 4.775 | 0.001 | 2431.7 |
| pair 2, bucket-on | 162.487 | 6.154 | 11.729 | 3.449 | 1.903 | 8.693 | 0.001 | 16.3 |

Paired deltas (bucket-on minus default-off):

| Pair | Frame ms | FPS | `cam` ms | `opaque` ms | `opqmodel` ms | `fsbegin` ms | MDLENTRY ratio |
|---|---:|---:|---:|---:|---:|---:|---:|
| 1 | -1.269 | +0.047 | -4.167 | -4.167 | -4.162 | +3.724 | 0.0071 |
| 2 | -1.344 | +0.050 | -4.237 | -4.256 | -4.240 | +3.918 | 0.0067 |
| mean | **-1.307** | **+0.049** | **-4.202** | **-4.212** | **-4.201** | **+3.821** | about **1/145** |

Interpretation:

1. The bucket path is active and has the intended effect in this render-heavy
   scene: model-body executions fall from roughly 2433 to 17 per gameplay
   frame, and opaque model dispatch falls by about 4.2 ms/frame.
2. The emulator transfers about 3.8 ms/frame of the 4.2 ms CPU saving into
   `stage=3 fsbegin` (frame-acquire/GPU queue wait). The remaining directional
   total-frame gain is about 1.3 ms/frame, or 0.049 FPS.
3. This does not clear a 60 FPS claim and does not establish that the same
   transfer occurs on PICA200. It does show that the optimization is causally
   effective on a render-heavy scene, not only on vol2.

## Powerdown load failure

Both matched binaries failed before gameplay:

| Build | PERF evidence | Trace evidence |
|---|---|---|
| default-off | `stage=1 count=0`, `stage=11 count=0`, five non-game frames | `Mload: num_groups=66`, then `AUTOTEST: SeriousError on LevelNum=8 (powerdown)` |
| bucket-on | `stage=1 count=0`, `stage=11 count=0`, five non-game frames | same failure point |

The two failure artifacts prove that the candidate did not cause the load
failure, but they contain no performance comparison. Powerdown therefore
remains **unmeasured in Mandarine** and requires the human hardware pair from
`docs/validation/ralph-pi-hardware-handoff.md`.

## Evidence

Ramqan artifacts and analyzer output:

- `build_3ds/ralph-pi/evidence/perf-20261102-125000-iter10-ramqan-default.log`
  (`.analysis.txt`, `.trace`, `.boot`)
- `build_3ds/ralph-pi/evidence/perf-20261102-125100-iter10-ramqan-bucket.log`
  (`.analysis.txt`, `.trace`, `.boot`)
- `build_3ds/ralph-pi/evidence/perf-20261102-125200-iter10-ramqan-default2.log`
  (`.analysis.txt`, `.trace`, `.boot`)
- `build_3ds/ralph-pi/evidence/perf-20261102-125300-iter10-ramqan-bucket2.log`
  (`.analysis.txt`, `.trace`, `.boot`)

Powerdown load-failure artifacts:

- `build_3ds/ralph-pi/evidence/perf-20261102-124000-iter10-powerdown-default.log`
  (`.analysis.txt`, `.trace`, `.boot`)
- `build_3ds/ralph-pi/evidence/perf-20261102-125400-iter10-powerdown-bucket.log`
  (`.analysis.txt`, `.trace`, `.boot`)

Binary checksum manifest and compiler-line comparison:

- `build_3ds/ralph-pi/iter10-scenes/verification.log` (exit 0)
- default-off final tree `forsaken3ds.3dsx` md5
  `eba28c890c6a123804d85a555c757b2e`
- ramqan default-off md5 `7e7e192175f8029cf228f70de1ecd1e3`
- ramqan bucket-on md5 `bc919e0446a96287c2e3d3cf892b3407`
- powerdown default-off md5 `1c6cdb64d2bac1abf10d1d83e2451e86`
- powerdown bucket-on md5 `5304f688d857a944dfdc920560724bd2`

## Exact commands and exit codes

Builds:

```sh
export DEVKITARM=/opt/devkitpro/devkitARM DEVKITPRO=/opt/devkitpro
make -f Makefile.3ds BUILD=build_3ds/ralph-pi/iter10-scenes/ramqan19-default EDITION=remaster AUTOTEST_REMASTER=1 AUTOTEST_FIRST_LEVEL=19 AUTOTEST_MAX_LEVELS_TO_SWEEP=1 AUTOTEST_MS_PER_LEVEL=8000 PERF_CAPTURE=1 OG_PROFILE=1 RENDERER=citro3d MODEL_BUCKET=0 SLIM=1 -j"$(nproc)"
# exit=0
make -f Makefile.3ds BUILD=build_3ds/ralph-pi/iter10-scenes/ramqan19-bucket EDITION=remaster AUTOTEST_REMASTER=1 AUTOTEST_FIRST_LEVEL=19 AUTOTEST_MAX_LEVELS_TO_SWEEP=1 AUTOTEST_MS_PER_LEVEL=8000 PERF_CAPTURE=1 OG_PROFILE=1 RENDERER=citro3d MODEL_BUCKET=1 SLIM=1 -j"$(nproc)"
# exit=0
make -f Makefile.3ds BUILD=build_3ds/ralph-pi/iter10-scenes/powerdown-default EDITION=remaster AUTOTEST_REMASTER=1 AUTOTEST_FIRST_LEVEL=8 AUTOTEST_MAX_LEVELS_TO_SWEEP=1 AUTOTEST_MS_PER_LEVEL=8000 PERF_CAPTURE=1 OG_PROFILE=1 RENDERER=citro3d MODEL_BUCKET=0 SLIM=1 -j"$(nproc)"
# exit=0
make -f Makefile.3ds BUILD=build_3ds/ralph-pi/iter10-scenes/powerdown-bucket EDITION=remaster AUTOTEST_REMASTER=1 AUTOTEST_FIRST_LEVEL=8 AUTOTEST_MAX_LEVELS_TO_SWEEP=1 AUTOTEST_MS_PER_LEVEL=8000 PERF_CAPTURE=1 OG_PROFILE=1 RENDERER=citro3d MODEL_BUCKET=1 SLIM=1 -j"$(nproc)"
# exit=0
make -f Makefile.3ds EDITION=remaster AUTOTEST_REMASTER=1 AUTOTEST_MAX_LEVELS_TO_SWEEP=1 AUTOTEST_MS_PER_LEVEL=8000 PERF_CAPTURE=1 OG_PROFILE=1 RENDERER=citro3d SLIM=1 -j"$(nproc)"
# exit=0; final default-off tree
```

Gates:

```sh
tools/perf/check.sh
# exit=0: gates_vfp, audit_arm11_math, audit_r11_idiv, audit_cxx_class all PASS
```

Matched captures and analysis (each capture and each analyzer command exited
0):

```sh
EV="build_3ds/ralph-pi/evidence/perf-20261102-125000-iter10-ramqan-default.log"; tools/perf/emu_sweep.sh --dsx build_3ds/ralph-pi/iter10-scenes/ramqan19-default/forsaken3ds.3dsx --out "$EV"
python3 tools/perf/analyze_sweep.py "$EV" "$EV.analysis.txt"
EV="build_3ds/ralph-pi/evidence/perf-20261102-125100-iter10-ramqan-bucket.log"; tools/perf/emu_sweep.sh --dsx build_3ds/ralph-pi/iter10-scenes/ramqan19-bucket/forsaken3ds.3dsx --out "$EV"
python3 tools/perf/analyze_sweep.py "$EV" "$EV.analysis.txt"
EV="build_3ds/ralph-pi/evidence/perf-20261102-125200-iter10-ramqan-default2.log"; tools/perf/emu_sweep.sh --dsx build_3ds/ralph-pi/iter10-scenes/ramqan19-default/forsaken3ds.3dsx --out "$EV"
python3 tools/perf/analyze_sweep.py "$EV" "$EV.analysis.txt"
EV="build_3ds/ralph-pi/evidence/perf-20261102-125300-iter10-ramqan-bucket2.log"; tools/perf/emu_sweep.sh --dsx build_3ds/ralph-pi/iter10-scenes/ramqan19-bucket/forsaken3ds.3dsx --out "$EV"
python3 tools/perf/analyze_sweep.py "$EV" "$EV.analysis.txt"
EV="build_3ds/ralph-pi/evidence/perf-20261102-124000-iter10-powerdown-default.log"; tools/perf/emu_sweep.sh --dsx build_3ds/ralph-pi/iter10-scenes/powerdown-default/forsaken3ds.3dsx --out "$EV"
python3 tools/perf/analyze_sweep.py "$EV" "$EV.analysis.txt"
EV="build_3ds/ralph-pi/evidence/perf-20261102-125400-iter10-powerdown-bucket.log"; tools/perf/emu_sweep.sh --dsx build_3ds/ralph-pi/iter10-scenes/powerdown-bucket/forsaken3ds.3dsx --out "$EV"
python3 tools/perf/analyze_sweep.py "$EV" "$EV.analysis.txt"
```

## Limits and alignment

- No console was contacted; there is no hardware timing, stereo run, or visual
  equivalence result in this iteration.
- Mandarine's host-speed CPU, smaller linear-memory pool, and non-PICA200
  frame-acquire behavior make the numbers directional only.
- The powerdown artifacts are load-failure evidence, not performance data.
- No engine source or data layout changed, so no new alignment case was
  introduced. The existing bucket arrays remain naturally aligned
  `u_int16_t` arrays (`models.c:1274-1323,1387-1432`).
- `MODEL_BUCKET` remains opt-in pending the human powerdown/ramqan visual pass.
