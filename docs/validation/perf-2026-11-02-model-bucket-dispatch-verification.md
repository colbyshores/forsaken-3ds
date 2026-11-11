# MODEL_BUCKET dispatch-sequence verification — OG-profile proxy

Date: 2026-11-02. Loop: `ralph-pi`, `experimental/native-renderer`.

## 1. Purpose

`MODEL_BUCKET` has a banked CPU win but remains opt-in because the prior
hardware A/B lacked an in-game visual-equivalence pass. The earlier code audit
argued that the bucket preserves the baseline model sequence, but that argument
was static.

This iteration adds an opt-in runtime verifier that compares the actual
`MODEL_BUCKET` chain for every visible group against the baseline guard
sequence, evaluated at the same point in `ModelDisp`. The goal is to test the
dynamic assumptions:

- the bucket contains exactly the models the baseline guard would draw;
- the order is identical;
- model visibility/group/clip state does not change between the bucket build
  and a later group call in the same camera traversal.

This is a correctness verifier, not a performance configuration.

## 2. Implementation

- `Makefile.3ds` adds `MODEL_BUCKET_VERIFY=1`, which requires `MODEL_BUCKET=1`.
- `models.c` adds `ModelVisBucket_Verify()`. For each `ModelDisp(group)` call it
  walks the source `Models[]` list and selects models satisfying the baseline
  predicate `ModelVisibleWithClip(i, InTitle, &clip) && clip == group`, then
  compares that sequence element-for-element with `VisBucketHead[group]` and
  `VisBucketNext[]`.
- `perf_capture_3ds.c` records cumulative group checks, expected entries,
  mismatches, and the first mismatch coordinates in `forsaken_perf.txt`.
- The verifier is compiled out unless both `PERF_CAPTURE=1` and
  `MODEL_BUCKET_VERIFY=1` are set. The default tree is unchanged.

## 3. Captured builds and commands

All commands were run from the repository root.

### Diagnostic verifier build

```sh
export DEVKITARM=/opt/devkitpro/devkitARM DEVKITPRO=/opt/devkitpro
make -f Makefile.3ds BUILD=build_3ds/ralph-pi/iter11/bucket-verify \
     EDITION=remaster AUTOTEST_REMASTER=1 AUTOTEST_FIRST_LEVEL=19 \
     AUTOTEST_MAX_LEVELS_TO_SWEEP=1 AUTOTEST_MS_PER_LEVEL=8000 \
     PERF_CAPTURE=1 OG_PROFILE=1 RENDERER=citro3d \
     MODEL_BUCKET=1 MODEL_BUCKET_VERIFY=1 SLIM=1 -j"$(nproc)"
```

Exit: `0`.

Binary:

```text
md5    2c5b2c6a97da9b748ecaec35329f66b5
sha256 698ff638d6ece5124688409b85a5355d16160ea27aa89a63ce3fed3524315b06
path   build_3ds/ralph-pi/iter11/bucket-verify/forsaken3ds.3dsx
```

### Verifier captures

```sh
EV="build_3ds/ralph-pi/evidence/perf-20261102-141500-iter11-ramqan-bucket-verify.log"
tools/perf/emu_sweep.sh --dsx build_3ds/ralph-pi/iter11/bucket-verify/forsaken3ds.3dsx --out "$EV"
python3 tools/perf/analyze_sweep.py "$EV" "$EV.analysis.txt"

EV="build_3ds/ralph-pi/evidence/perf-20261102-141600-iter11-ramqan-bucket-verify2.log"
tools/perf/emu_sweep.sh --dsx build_3ds/ralph-pi/iter11/bucket-verify/forsaken3ds.3dsx --out "$EV"
python3 tools/perf/analyze_sweep.py "$EV" "$EV.analysis.txt"
```

All four commands exited `0`.

### Final default tree

```sh
export DEVKITARM=/opt/devkitpro/devkitARM DEVKITPRO=/opt/devkitpro
make -f Makefile.3ds EDITION=remaster AUTOTEST_REMASTER=1 \
     AUTOTEST_MAX_LEVELS_TO_SWEEP=1 AUTOTEST_MS_PER_LEVEL=8000 \
     PERF_CAPTURE=1 OG_PROFILE=1 RENDERER=citro3d SLIM=1 -j"$(nproc)"
```

Exit: `0`.

Default binary:

```text
md5    eba28c890c6a123804d85a555c757b2e
sha256 fb6e80b46aedb29211933a84277c7b1b312bb09e4f598f4d6f8186501243887e
path   build_3ds/ralph-pi/iter11/default/forsaken3ds.3dsx
```

This is byte-identical to the iter 010 final default-off capture binary.

### Gates and default smoke

```sh
tools/perf/check.sh
EV="build_3ds/ralph-pi/evidence/perf-20261102-141700-iter11-default-smoke.log"
tools/perf/emu_sweep.sh --dsx forsaken3ds.3dsx --out "$EV"
python3 tools/perf/analyze_sweep.py "$EV" "$EV.analysis.txt"
```

All exited `0`. `tools/perf/check.sh` reports `gates_vfp`, `audit_arm11_math`,
`audit_r11_idiv`, and `audit_cxx_class` PASS.

## 4. Results

Environment: Mandarine emulator, `OG_PROFILE=1` proxy, ramqan slot 19,
8 s/level, mono (`render_3d=0`). This is directional emulator evidence and not
a hardware pass.

| Capture | Frames | Group checks | Expected entries | Mismatches |
|---|---:|---:|---:|---:|
| `perf-20261102-141500-iter11-ramqan-bucket-verify.log` | 245 | 2353 | 3881 | 0 |
| `perf-20261102-141600-iter11-ramqan-bucket-verify2.log` | 244 | 2342 | 4095 | 0 |

Raw verifier lines:

```text
PERF: model_bucket_verify groups=2353 entries=3881 mismatches=0 first_group=-1 first_expected=0 first_actual=0
PERF: model_bucket_verify groups=2342 entries=4095 mismatches=0 first_group=-1 first_expected=0 first_actual=0
```

The expected-entry count exactly matched the bucket path's `MDLENTRY` stage
count in both runs (`stage=51 count=3881` and `stage=51 count=4095`). Thus the
verifier checked every bucket entry actually traversed by `ModelDisp`.

The default smoke capture contains no `model_bucket_verify` line, and the
default `.3dsx` contains no verifier string. Its analyzer FPS was 51.0 on
vol2, consistent with the prior default-off captures.

## 5. Interpretation

Across two independent ramqan runs, the bucket chain was element-for-element
identical to the baseline-visible sequence for every group call. This directly
tests the dynamic equivalence concern and finds no missing, extra, reordered, or
stale model dispatch.

This does **not** replace the required human in-game visual sanity pass. The
verifier proves the model dispatch sequence, not the final PICA framebuffer.
`MODEL_BUCKET` therefore remains opt-in.

The verifier build is intentionally slower than the normal bucket build because
it re-walks `Models[]` for every group. Its stage timings are not a performance
measurement and must not be compared with the normal bucket A/B.

## 6. Alignment check

No struct layout or data layout changed. The verifier reads the existing
`u_int16_t` bucket arrays and `Models[]` links, all naturally aligned. It adds
only static counters and a diagnostic traversal; no new casts, pointer
arithmetic, or `memcpy` pattern was introduced.
