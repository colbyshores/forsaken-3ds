# Current-head completion-evidence audit — native renderer

Date: 2026-11-02. Loop: `ralph-pi`, branch `experimental/native-renderer`.

Purpose: re-check every substantive completion clause in
`docs/validation/ralph-pi-spec.md` section 8 against the current tree and
retained artifacts. This is an evidence audit, not a new performance
experiment and not a hardware pass.

## 1. Tree and artifact identity

| Field | Value |
|---|---|
| Audited source HEAD | `5c3a92efcd489c11e38e579cd59cfae21892303c` |
| Engine-tree diff fingerprint | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` (clean) |
| Engine paths audited | `.gitignore Makefile.3ds models.c perf_capture.h perf_capture_3ds.c render_c3d.c` |
| Primary `forsaken3ds.3dsx` md5 | `eba28c890c6a123804d85a555c757b2e` |
| Primary `forsaken3ds.3dsx` sha256 | `fb6e80b46aedb29211933a84277c7b1b312bb09e4f598f4d6f8186501243887e` |
| Shipped model-bucket policy | opt-in (`MODEL_BUCKET ?= 0`, `Makefile.3ds:39-42`) |

The primary binary is byte-identical to the default-off binaries used by the
banked frame-budget and Item 7 captures. The audit changed no engine source,
structure, or data layout.

## 2. Completion-clause matrix

| Spec clause | Banked evidence | Current audit |
|---|---|---|
| 1 — Item 3 verdict and stale TODO removed | Archive iter 001; `render_c3d.c:15-24,369-430,2772-2775,2884-2901`; `Makefile.3ds:329-337` selects `-lcitro3d` without picaGL for the Citro3D renderer | Holds. The renderer calls Citro3D directly and submits triangles with `C3D_DrawArrays`; the stale GL-based/TODO banner is absent. The remaining per-draw scratch conversion is Item 6 and was measured separately. |
| 2a — Item 6 verdict | Archive iter 002: three matched indexed-cache pairs; approximately 0.64 ms/frame less camera CPU was offset by approximately 0.65 ms/frame more `fsbegin` wait, with unchanged total frame time/FPS; candidate reverted | Holds as a measured-and-rejected verdict for the indexed `C3D_DrawElements` cache form. The reviewer corrected the reported background-stage reduction to about 0.65 rather than 0.33 ms/frame; that correction does not change the null result. |
| 2b — Item 7 verdict | Archive iters 006 and 010: matched default-off/bucket-on pairs; bucket removes about 98.8% of model-list iterations in vol2 and about 4.2 ms/frame of camera CPU in ramqan, with no material total-frame gain in vol2 and only about +0.049 FPS in ramqan; bucket remains opt-in. Iter 011 found zero dispatch-sequence mismatches over 2353/2342 group checks. | Holds. The CPU optimization exists but is not a demonstrated OG-proxy FPS win, so it is not shipped by default. Final-framebuffer visual equivalence remains a human hardware check. |
| 3 — final-tree gates | Current `tools/perf/check.sh` run in §3 | Holds: exit 0; all four audits PASS. |
| 4 — frame-budget assessment | `docs/validation/perf-2026-11-02-final-default-off-frame-budget.md`; archive iter 008; seven retained default-off captures | Holds. Mean proxy frame time excluding the worst frame was 19.640 ms (50.9 FPS), 2.970 ms over the 16.67 ms budget; removing `fsbegin`/`fsend` waits left 9.438 ms/frame, 7.232 ms inside budget. This is emulator-scoped, mono, vol2, and explicitly not a hardware pass. |
| 5 — no unresolved high-severity reviewer finding/regression | Archive reviewer verdicts after iter 005: iter 002 PASS (with the background-stage number corrected), iter 003 FAIL; iter 006 corrected iter 003 by restoring the opt-in default. Iter 008 records that the iter 007 clause-4 objection was resolved. | Holds substantively: no unresolved high-severity finding is recorded, and the failed default-on policy is not present. The archive still contains no explicit reviewer verdict line for medium-risk iter 006, so the loop's pre-completion independent-review policy remains the procedural step before any DONE claim. |

## 3. Current-tree verification

Exact primary build (exit 0; the engine inputs were already up to date):

```sh
export DEVKITARM=/opt/devkitpro/devkitARM DEVKITPRO=/opt/devkitpro
make -f Makefile.3ds EDITION=remaster AUTOTEST_REMASTER=1 \
     AUTOTEST_MAX_LEVELS_TO_SWEEP=1 AUTOTEST_MS_PER_LEVEL=8000 \
     PERF_CAPTURE=1 OG_PROFILE=1 RENDERER=citro3d SLIM=1 -j"$(nproc)"
```

Log: `build_3ds/ralph-pi/iter15/build.log` (empty, exit recorded in
`build_3ds/ralph-pi/iter15/build.exit`).

Gates (exit 0):

```sh
tools/perf/check.sh
```

Result: `gates_vfp`, `audit_arm11_math`, `audit_r11_idiv`, and
`audit_cxx_class` all PASS. Log: `build_3ds/ralph-pi/iter15/gates.log`.

The retained banked captures were also mechanically re-parsed at the current
tree (each exit 0):

```sh
python3 tools/perf/analyze_sweep.py \
  build_3ds/ralph-pi/evidence/perf-20261102-112725-iter8-final-default.log \
  build_3ds/ralph-pi/iter15/iter8-final-default.reanalysis.txt
python3 tools/perf/analyze_sweep.py \
  build_3ds/ralph-pi/evidence/perf-20261102-125200-iter10-ramqan-default2.log \
  build_3ds/ralph-pi/iter15/iter10-ramqan-default.reanalysis.txt
python3 tools/perf/analyze_sweep.py \
  build_3ds/ralph-pi/evidence/perf-20261102-125300-iter10-ramqan-bucket2.log \
  build_3ds/ralph-pi/iter15/iter10-ramqan-bucket.reanalysis.txt
```

They reproduce the banked directional summaries: vol2 default-off 51.0 FPS;
ramqan default-off 6.1 FPS with camera work 16.0 ms/frame; ramqan bucket-on
6.2 FPS with camera work 11.7 ms/frame.

No new emulator A/B was run because this iteration changes no engine behavior
and its binary is byte-identical to the measured tree. These re-analyses are
parser checks, not new performance samples.

## 4. Limits and remaining human step

No console was contacted. There is no real OG/New 3DS timing, stereo run, P6
environment record, or in-game visual-equivalence result. Mandarine runs the
guest CPU at host speed, uses a smaller linear-memory pool, and does not model
PICA200 frame-acquire timing faithfully; its numbers are directional only.

The remaining human action is the session specified by
`docs/validation/ralph-pi-hardware-handoff.md`: run the default-off and
bucket-on powerdown/ramqan cases, preserve each PERF/boot artifact, validate
with `tools/perf/analyze_hardware_capture.py`, and bank the visual checklist.
Keep `MODEL_BUCKET=0` until that visual result is banked.
