# Ralph-pi hardware-capture handoff — native renderer

Prepared: 2026-11-02. Refreshed: 2026-11-02 (iteration 14). Loop: `ralph-pi`,
branch `experimental/native-renderer`.

Purpose: make the remaining human hardware pass reproducible. This handoff
contains clean, manually navigated capture binaries for the shipped default-off
renderer, the existing opt-in `MODEL_BUCKET=1` path, and a third
`MODEL_BUCKET_VERIFY=1` diagnostic build. The verifier compares the bucket's
actual model-dispatch sequence against the baseline-visible sequence and
records the result in `forsaken_perf.txt`. It does **not** contain a hardware
result: the console was not touched while preparing it.

## 1. Source and build state

| Field | Value |
|---|---|
| Branch | `experimental/native-renderer` |
| Build-source HEAD | `9710973cfe405dd4d2e306a9b9d4d5e2fe5597a5` (`git rev-parse HEAD` at the iteration-14 build) |
| Engine-tree diff fingerprint | `git diff --binary HEAD -- .gitignore Makefile.3ds models.c perf_capture.h perf_capture_3ds.c render_c3d.c \| sha256sum` = `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` (clean engine tree at build time) |
| Renderer | Citro3D (`RENDERER=citro3d`); picaGL is not linked |
| Edition | `EDITION=remaster` (level order/music only; it does not enable verbose trace) |
| Capture instrumentation | `PERF_CAPTURE=1`; output `sdmc:/forsaken_perf.txt` at clean teardown |
| Clock policy | `OG_PROFILE=1`: New 3DS boost disabled, 268 MHz/L2-off policy and 32/22 MB heap tiers; on OG hardware the console is natively 268 MHz/L2-off |
| Toolchain | devkitARM 15.2.0 (`arm-none-eabi-gcc -dumpversion` = `15.2.0`) |
| Debug/autotest/trace flags | none in any of the three handoff binaries |

The primary tree build and gates were run with the loop's exact commands:

```sh
export DEVKITARM=/opt/devkitpro/devkitARM DEVKITPRO=/opt/devkitpro
make -f Makefile.3ds EDITION=remaster AUTOTEST_REMASTER=1 \
     AUTOTEST_MAX_LEVELS_TO_SWEEP=1 AUTOTEST_MS_PER_LEVEL=8000 \
     PERF_CAPTURE=1 OG_PROFILE=1 RENDERER=citro3d SLIM=1 -j"$(nproc)"
# exit=0; log: build_3ds/ralph-pi/iter14/primary-build.log

tools/perf/check.sh
# exit=0; log: build_3ds/ralph-pi/iter14/gates.log
```

The three handoff binaries were then built without `AUTOTEST_REMASTER`, so a
person can navigate to a named level and the timing run has no debug trace
writes:

```sh
export DEVKITARM=/opt/devkitpro/devkitARM DEVKITPRO=/opt/devkitpro
make -f Makefile.3ds BUILD=build_3ds/ralph-pi/iter14/hardware/default-off \
     EDITION=remaster PERF_CAPTURE=1 OG_PROFILE=1 RENDERER=citro3d \
     MODEL_BUCKET=0 SLIM=1 -j"$(nproc)"
# exit=0; log: build_3ds/ralph-pi/iter14/default-off-build.log

make -f Makefile.3ds BUILD=build_3ds/ralph-pi/iter14/hardware/bucket-on \
     EDITION=remaster PERF_CAPTURE=1 OG_PROFILE=1 RENDERER=citro3d \
     MODEL_BUCKET=1 SLIM=1 -j"$(nproc)"
# exit=0; log: build_3ds/ralph-pi/iter14/bucket-on-build.log

make -f Makefile.3ds BUILD=build_3ds/ralph-pi/iter14/hardware/bucket-verify \
     EDITION=remaster PERF_CAPTURE=1 OG_PROFILE=1 RENDERER=citro3d \
     MODEL_BUCKET=1 MODEL_BUCKET_VERIFY=1 SLIM=1 -j"$(nproc)"
# exit=0; log: build_3ds/ralph-pi/iter14/bucket-verify-build.log
```

`MODEL_BUCKET_VERIFY=1` requires `MODEL_BUCKET=1`; the Makefile rejects an
invalid combination. The verifier deliberately repeats the old `Models[]`
walk and is a correctness diagnostic, not a performance configuration.

## 2. Files to put on the SD card

These are slim builds: the three `.3dsx` files are small and share one current
romfs.

| Host artifact | SD destination | SHA-256 | MD5 | Bytes |
|---|---|---|---|---:|
| `build_3ds/ralph-pi/iter14/hardware/default-off/forsaken3ds.3dsx` | `sdmc:/3ds/forsaken/forsaken3ds-default-off.3dsx` | `7fd7c153aaafa09fb4dd63dd7680568fcb89ced5b2c77b8e51238262c2e2a9fc` | `1ceef734747cc9d9ed15c5988d51196b` | 3,530,096 |
| `build_3ds/ralph-pi/iter14/hardware/bucket-on/forsaken3ds.3dsx` | `sdmc:/3ds/forsaken/forsaken3ds-model-bucket.3dsx` | `458bed2cc593eb7ca4b0eda198403731e6163280e8d5f9630acbb9356f7c1864` | `a4626e953858a74d133d24c559dbe2ae` | 3,530,540 |
| `build_3ds/ralph-pi/iter14/hardware/bucket-verify/forsaken3ds.3dsx` | `sdmc:/3ds/forsaken/forsaken3ds-model-bucket-verify.3dsx` | `582aef37f7392d60e5cfc44ecc2970fd3ac6c73bb8aa3789c429f586fc5acbf7` | `fe38f39205dfe29a94d54a1c6a2250c1` | 3,531,624 |
| `build_3ds/ralph-pi/iter9-hardware/forsaken.romfs` | `sdmc:/3ds/forsaken/forsaken.romfs` | `e8426b7b5fa7d68d96782492cc2b4d1cd8871d38e34a50c4a6b0880a9b8d6786` | `93b701405eb53b95ed04dfa1cc5b2d2b` | 311,608,372 |

The exact external-romfs path is required by the slim build
(`main_3ds.c:407-445`). Keep the three `.3dsx` files under distinct names;
launch them one at a time. Do not overwrite the shared romfs between runs.

## 3. Triple validation performed without hardware

The machine-checked verification is in
`build_3ds/ralph-pi/iter14/hardware/verification.log` (exit 0):

1. The three saved build stamps differ only in `BUCKET`/`BVERIFY`:
   `PERF=1`, `OGPROFILE=1`, `ED=remaster`, `SLIM=1`; autotest and verbose-trace
   settings are absent.
2. All 100 compiler command lines match by source after removing
   `-DMODEL_BUCKET` and `-DMODEL_BUCKET_VERIFY` and normalizing the separate
   object-directory path. No command contains `-D__3DS_DEBUG__`,
   `-DVERBOSE_TRACE`, `-DAUTOTEST_REMASTER`, or `-DDEATH_SIM`.
3. All three artifacts contain the perf-capture implementation and the
   `sdmc:/forsaken_perf.txt` path. None contains the
   `sdmc:/forsaken_trace.txt` path or an `AUTOTEST:` string. Only the verifier
   artifact contains the `model_bucket_verify` result string.
4. All three iteration-14 binaries are byte-identical to the corresponding
   iteration-12 binaries (and the default-off/bucket-on pair remains
   byte-identical to iteration 09), proving that the current HEAD refresh did
   not change either shipping/candidate configuration or the verifier.
5. The bucket path uses naturally aligned `u_int16_t` arrays
   (`models.c:1274-1323,1387-1432`); this iteration changed no source or data
   layout and introduced no new alignment case.
6. The copied shared romfs is byte-identical to the `forsaken.romfs` produced
   by the build.

## 4. Human capture procedure

Record the P6 fields from `docs/validation/capture-protocol.md` §2 before
starting: console, actual clock/cache, stereo mode, branch/revision, binary
checksum, level/workload, and date. `sdmc:/forsaken_boot.log` should report
`clock=268MHz cache=L2-off` for an OG-profile run; retain it as supporting
evidence.

Use **one level per process/run** because these manual builds intentionally
have no autotest level markers. For each build and level:

1. Launch the matching `.3dsx`, enter the named level, and hold a repeatable
   view/action sequence for at least 60 seconds of steady state.
2. Exit through the normal HOME/Close path so teardown writes
   `sdmc:/forsaken_perf.txt`.
3. Copy that file off the card with a unique name (for example
   `forsaken_perf-default-powerdown.txt`,
   `forsaken_perf-bucket-powerdown.txt`, and
   `forsaken_perf-bucket-verify-powerdown.txt`). The perf writer opens the
   file in append mode for autotest chain-load compatibility, so **delete the
   old `forsaken_perf.txt` from the SD card before the next process**. If it
   was not deleted, analyze only the final complete `PERF: dump begin` …
   `PERF: dump end` block.
4. Copy the final artifacts verbatim into
   `docs/validation/perf-YYYY-MM-DD-*.log` and note the exact source SD
   filename in a sidecar/header.

### Validate each copied artifact before quoting it

Use the matching boot log with `tools/perf/analyze_hardware_capture.py`.
`--require-og-profile` checks the 268 MHz/L2-off `OG_PROFILE=1` policy;
`--require-console` separately identifies the console family. Use `og` on
original/non-New 3DS hardware and `n3ds` on New 3DS hardware:

```sh
# Original/non-New 3DS hardware
python3 tools/perf/analyze_hardware_capture.py \
  --perf forsaken_perf-default-powerdown.txt \
  --boot forsaken_boot-default-powerdown.log \
  --require-og-profile --require-console og \
  --out capture-default-powerdown.analysis.txt

# New 3DS hardware running the OG-profile clock/cache policy
python3 tools/perf/analyze_hardware_capture.py \
  --perf forsaken_perf-default-ramqan.txt \
  --boot forsaken_boot-default-ramqan.log \
  --require-og-profile --require-console n3ds \
  --out capture-default-ramqan.analysis.txt
```

For a bucket-verifier artifact, add `--require-verify`; the command must exit
0 and report zero mismatches. A parser exit 0 validates the artifact's shape
and recorded environment only: it does **not** replace the P6 environment
record, the visual checklist, or the human attribution of the actual console,
clock/cache, and stereo mode.

Run these four required timing/visual cases, using the same camera route and
comparable gameplay state for each pair:

| Case | Build | Level | Purpose |
|---|---|---|---|
| 1 | default-off | `powerdown` | shipped baseline and visual reference |
| 2 | bucket-on | `powerdown` | render-heavy A/B plus visual equivalence |
| 3 | default-off | `ramqan` | shipped baseline and visual reference |
| 4 | bucket-on | `ramqan` | render-heavy A/B plus visual equivalence |

After each bucket-on run, repeat the same level and camera route with the
bucket-verify binary. These two additional diagnostic runs must end with a
line of this form:

```text
PERF: model_bucket_verify groups=<N> entries=<N> mismatches=0 first_group=-1 first_expected=0 first_actual=0
```

`groups` and `entries` must both be greater than zero. A missing verifier line,
`mismatches` greater than zero, or a non-`-1` first mismatch is a failed
diagnostic. Preserve the complete perf file for each verifier run.

If time permits, repeat the default/bucket pair in the opposite order to
expose ordering effects. Keep stereo off for the matched timing pair, then
record a separate stereo run if available; stereo doubles CPU scene traversal.

### Visual sanity checklist for the bucket-on run

At matched viewpoints in both levels, compare against the default-off run for:

- missing, duplicated, ghosted, or wrongly clipped models;
- changed model draw/occlusion order at transparent or overlapping surfaces;
- incorrect portal-boundary visibility, cracks, or flickering model edges;
- HUD/weapon/transparent geometry remaining unchanged;
- no crash, hang, black frame, or failure to return cleanly through teardown.

A timing win or a zero-mismatch dispatch audit does not substitute for this
visual pass. Keep `MODEL_BUCKET=0` as the shipped default until the bucket-on
visual result is banked.

## 5. Limits of this handoff

No console was contacted and no hardware timing or visual result is claimed.
`OG_PROFILE=1` is a clock/heap/tier policy, not a model of PICA200 timing; a
human OG run is still required to determine whether the emulator's large
`fsbegin` wait exists on hardware. The prior corrected hardware A/B remains
in `docs/validation/perf-2026-10-30-model-bucket-findings.md`; this handoff
does not replace its missing in-game visual sanity run. The verifier proves
model dispatch sequence equivalence, not final framebuffer pixels.
