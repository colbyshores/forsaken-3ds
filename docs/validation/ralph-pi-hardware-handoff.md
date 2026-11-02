# Ralph-pi hardware-capture handoff — native renderer

Prepared: 2026-11-02. Loop: `ralph-pi`, branch
`experimental/native-renderer`.

Purpose: make the remaining human hardware pass reproducible. This handoff
contains a clean, manually navigated capture pair for the shipped default-off
renderer and the existing opt-in `MODEL_BUCKET=1` path. It does **not** contain
a hardware result: the console was not touched while preparing it.

## 1. Source and build state

| Field | Value |
|---|---|
| Branch | `experimental/native-renderer` |
| HEAD | `43aa7e72609482a1772f78947fb70ff75ad9edc7` |
| Engine-tree diff fingerprint | `git diff --binary -- .gitignore Makefile.3ds render_c3d.c \| sha256sum` = `fdeac6532e6b97a4139cccae417f4708b275c5023eaf365e4b825a98c9d65451` |
| Renderer | Citro3D (`RENDERER=citro3d`); picaGL is not linked |
| Edition | `EDITION=remaster` (level order/music only; it does not enable verbose trace) |
| Capture instrumentation | `PERF_CAPTURE=1`; output `sdmc:/forsaken_perf.txt` at clean teardown |
| Clock policy | `OG_PROFILE=1`: New 3DS boost disabled, 268 MHz/L2-off policy and 32/22 MB heap tiers; on OG hardware the console is natively 268 MHz/L2-off |
| Toolchain | devkitARM 15.2.0 (`arm-none-eabi-gcc -dumpversion` = `15.2.0`) |
| Debug/autotest/trace flags | none in either handoff binary |

The primary tree build and gates were run with the loop's exact commands:

```sh
export DEVKITARM=/opt/devkitpro/devkitARM DEVKITPRO=/opt/devkitpro
make -f Makefile.3ds EDITION=remaster AUTOTEST_REMASTER=1 \
     AUTOTEST_MAX_LEVELS_TO_SWEEP=1 AUTOTEST_MS_PER_LEVEL=8000 \
     PERF_CAPTURE=1 OG_PROFILE=1 RENDERER=citro3d SLIM=1 -j"$(nproc)"
# exit=0; log: build_3ds/ralph-pi/iter9-build.log

tools/perf/check.sh
# exit=0; log: build_3ds/ralph-pi/iter9-gates.log
```

The handoff pair was then built without `AUTOTEST_REMASTER`, so a person can
navigate to a named level and the timing run has no debug trace writes:

```sh
export DEVKITARM=/opt/devkitpro/devkitARM DEVKITPRO=/opt/devkitpro
make -f Makefile.3ds EDITION=remaster PERF_CAPTURE=1 OG_PROFILE=1 \
     RENDERER=citro3d MODEL_BUCKET=0 SLIM=1 -j"$(nproc)"
# exit=0; log: build_3ds/ralph-pi/iter9-hardware/default-off/build.log

make -f Makefile.3ds EDITION=remaster PERF_CAPTURE=1 OG_PROFILE=1 \
     RENDERER=citro3d MODEL_BUCKET=1 SLIM=1 -j"$(nproc)"
# exit=0; log: build_3ds/ralph-pi/iter9-hardware/bucket-on/build.log
```

## 2. Files to put on the SD card

These are slim builds: the two `.3dsx` files are small and share one current
romfs.

| Host artifact | SD destination | SHA-256 | MD5 | Bytes |
|---|---|---|---|---:|
| `build_3ds/ralph-pi/iter9-hardware/default-off/forsaken3ds.3dsx` | `sdmc:/3ds/forsaken/forsaken3ds-default-off.3dsx` | `7fd7c153aaafa09fb4dd63dd7680568fcb89ced5b2c77b8e51238262c2e2a9fc` | `1ceef734747cc9d9ed15c5988d51196b` | 3,530,096 |
| `build_3ds/ralph-pi/iter9-hardware/bucket-on/forsaken3ds.3dsx` | `sdmc:/3ds/forsaken/forsaken3ds-model-bucket.3dsx` | `458bed2cc593eb7ca4b0eda198403731e6163280e8d5f9630acbb9356f7c1864` | `a4626e953858a74d133d24c559dbe2ae` | 3,530,540 |
| `build_3ds/ralph-pi/iter9-hardware/forsaken.romfs` | `sdmc:/3ds/forsaken/forsaken.romfs` | `e8426b7b5fa7d68d96782492cc2b4d1cd8871d38e34a50c4a6b0880a9b8d6786` | `93b701405eb53b95ed04dfa1cc5b2d2c` | 311,608,372 |

The exact external-romfs path is required by the slim build
(`main_3ds.c:407-445`). Keep the two `.3dsx` files under distinct names;
launch them one at a time. Do not overwrite the shared romfs between the two
runs.

## 3. Pair validation performed without hardware

The machine-checked verification is in
`build_3ds/ralph-pi/iter9-hardware/verification.log` (exit 0):

1. The saved build stamps differ only in `BUCKET=0` versus `BUCKET=1`:
   `PERF=1`, `OGPROFILE=1`, `ED=remaster`, `SLIM=1`; `AT` (autotest),
   `DEBUG`, and verbose trace are absent.
2. All 101 compiler command lines are identical after removing
   `-DMODEL_BUCKET`. Neither command set contains `-D__3DS_DEBUG__`,
   `-DVERBOSE_TRACE`, or `-DAUTOTEST_REMASTER`.
3. Both ELFs contain the perf-capture implementation and the
   `sdmc:/forsaken_perf.txt` path. Neither contains the
   `sdmc:/forsaken_trace.txt` path, confirming that the release trace stub is
   linked.
4. `ModelDisp` is 0xbf0 bytes in the default-off ELF and 0xd98 bytes in the
   bucket-on ELF, confirming that the candidate contains the larger compiled
   bucket path. The bucket path uses naturally aligned `u_int16_t` arrays
   (`models.c:1274-1323,1387-1432`); this iteration changed no source or data
   layout, so it introduced no new alignment case.
5. The copied shared romfs is byte-identical to the `forsaken.romfs` produced
   by the build.

## 4. Human capture procedure

Record the P6 fields from `docs/validation/capture-protocol.md` §2 before
starting: console, actual clock/cache, stereo mode, branch/revision, binary
checksum, level/workload, and date. `sdmc:/forsaken_boot.log` should report
`clock=268MHz cache=L2-off` for an OG-profile run; retain it as supporting
evidence.

Use **one level per process/run** because this manual build intentionally has
no autotest level markers. For each build and level:

1. Launch the matching `.3dsx`, enter the named level, and hold a repeatable
   view/action sequence for at least 60 seconds of steady state.
2. Exit through the normal HOME/Close path so teardown writes
   `sdmc:/forsaken_perf.txt`.
3. Before starting another run, copy that file off the card with a unique name
   (for example `forsaken_perf-default-powerdown.txt` and
   `forsaken_perf-bucket-powerdown.txt`). The next process truncates and
   replaces it.
4. Copy the final artifacts verbatim into
   `docs/validation/perf-YYYY-MM-DD-*.log` and note the exact source SD
   filename in a sidecar/header.

Run these four required cases, using the same camera route and comparable
gameplay state for each pair:

| Case | Build | Level | Purpose |
|---|---|---|---|
| 1 | default-off | `powerdown` | shipped baseline and visual reference |
| 2 | bucket-on | `powerdown` | render-heavy A/B plus visual equivalence |
| 3 | default-off | `ramqan` | shipped baseline and visual reference |
| 4 | bucket-on | `ramqan` | render-heavy A/B plus visual equivalence |

If time permits, repeat the pair in the opposite order to expose ordering
effects. Keep stereo off for the matched timing pair, then record a separate
stereo run if available; stereo doubles CPU scene traversal.

### Visual sanity checklist for the bucket-on run

At matched viewpoints in both levels, compare against the default-off run for:

- missing, duplicated, ghosted, or wrongly clipped models;
- changed model draw/occlusion order at transparent or overlapping surfaces;
- incorrect portal-boundary visibility, cracks, or flickering model edges;
- HUD/weapon/transparent geometry remaining unchanged;
- no crash, hang, black frame, or failure to return cleanly through teardown.

A timing win does not substitute for this visual pass. Keep
`MODEL_BUCKET=0` as the shipped default until the bucket-on visual result is
banked.

## 5. Limits of this handoff

No console was contacted and no hardware timing or visual result is claimed.
`OG_PROFILE=1` is a clock/heap/tier policy, not a model of PICA200 timing; a
human OG run is still required to determine whether the emulator's large
`fsbegin` wait exists on hardware. The prior corrected hardware A/B remains
in `docs/validation/perf-2026-10-30-model-bucket-findings.md`; this handoff
does not replace its missing in-game visual sanity run.
