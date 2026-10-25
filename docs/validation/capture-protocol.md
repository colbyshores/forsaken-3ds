# Hardware capture protocol — Forsaken 3DS perf contract

Prepared 2026-10-25 so the hardware session is a single pass. Every item here
is already staged on `experimental/performance_contract`; this document only
tells the on-device session what to run and what to bring back.

Contract references: AGENTS.md §2 (corpus), §6 (do-not-reopen), §P6 (evidence).

## 0. Deliverables of the session (in priority order)

| # | Deliverable | Unblocks |
|---|---|---|
| 1 | Stage/frame-time capture → `docs/validation/perf-YYYY-MM-DD-capture.log` | corpus `provisional:false`, R7/R2/R9 review |
| 2 | A/B timing: current build vs floor/Random_Range-revert build | the two `measured n/a` perf claims (R6 wave, R11 wave) |
| 3 | P6 environment record (below) | validity of everything above |
| 4 | (Optional) stereo-mode capture | stereo traversal cost claim (§6) |

R4 `spline()` retype is NOT part of this session: a float retype changes
spline output for 1.28% of the parameter domain (exhaustive host measurement,
see commit 21b5b99) and needs deterministic gameplay-position logging that
does not exist yet. It stays baselined.

## 1. Pre-session state (verify before travel)

- Branch `experimental/performance_contract`, `tools/perf/check.sh` → ALL GATES PASS.
- Instrumented build: `make -f Makefile.3ds PERF_CAPTURE=1 SLIM=1`
  (verified to link and run 2026-10-25). Do NOT add `EDITION=remaster` or
  `DEBUG=1` to a capture build: both force `-DVERBOSE_TRACE`, whose
  per-line fsync (5–10 ms each, P5) would poison the timing being measured,
  and `__3DS_DEBUG__` additionally enables autoboot + god mode. Default
  builds are unaffected: with the flag off, `perf_capture_3ds.o` is empty
  and no perf symbols exist.
- The dump goes to `sdmc:/forsaken_perf.txt` through perf_capture_3ds.c's
  own fd — no trace/serial sink needed.
- Make does not track CFLAGS changes: when toggling `PERF_CAPTURE`,
  `touch perf_capture_3ds.c main.c oct2.c` (or clean-build) or the stale
  object is relinked silently.
- Bring: SD card with the `.3dsx` plus the SD romfs, and both build
  binaries (A/B below).

## 2. P6 environment record — fill in, do not skip

```
console:        [OG 268 MHz | New 3DS (boost enabled at main_3ds.c:312-316)]
clock/cache:    [as reported; note L1 16K/16K, no L2 on OG]
stereo mode:    [off | on]   (stereo doubles CPU scene traversal, §6)
build revision: [git rev-parse HEAD]
toolchain:      devkitARM 15.2.0, libctru FZ|DN verified (commit 21b5b99)
scene/workload: [level + what was on screen]
```

A New-clocked run is not an OG pass. Record both if you have both.

## 3. Capture procedure (deliverable 1)

1. Boot the `PERF_CAPTURE=1` build.
2. Play/hold each scenario for ≥ 60 s of steady state:
   - a busy gameplay level (most entities + effects on screen — e.g. a
     late-vol SP level or a deathmatch sim if the autotest harness can hold
     one),
   - a quiet level (baseline floor),
   - title screen (cold-path reference).
3. Exit normally (HOME → Close, or Start+Select — both are the clean
   `aptMainLoop()` exit path, not a crash). The teardown emits one
   `PERF: dump begin` … `PERF: dump end` block into
   `sdmc:/forsaken_perf.txt` — one SD write sequence, never per-frame (P5).
   If the file is missing after exit, the run did not reach teardown
   (check `sdmc:/forsaken_boot.log`, which is always-on).
4. Copy `forsaken_perf.txt` off the card verbatim →
   `docs/validation/perf-YYYY-MM-DD-capture.log`.

Log format (ticks are `svcGetSystemTick()`, 2^15 Hz = 32768 Hz):

```
PERF: stage=0 count=N total=T max=M     (0=frame, 1=MainGame, 2=MainGameRender)
PERF: hist ms<=K count=N                (frame-time histogram, 1 ms buckets)
PERF: frames=N
```

## 4. Corpus update procedure (offline, after the session)

1. ms-per-frame per stage = `total / count / 32768 * 1000`. Histogram buckets are 33 ticks (~1.007 ms).
2. A function qualifies HOT if it runs ≥1×/frame AND is top-15 by total, or
   ≥256×/frame (AGENTS.md §2). Stage counters cover frame structure; for
   per-function granularity beyond MainGame/MainGameRender, note it as a
   follow-up rather than guessing.
3. Edit `tools/perf/contracts/hotpath_corpus.json`: set `provisional:false`,
   set `captured` to the log filename + date, re-derive `seed` from the
   profile. Keep `cold_always` unless the profile contradicts it.
4. Re-run `tools/perf/check.sh`. New findings from the re-derived corpus are
   the next audit wave — do not bless them silently.

## 5. A/B perf claims (deliverable 2)

Build B = current HEAD with the two perf changes reverted (temp branch):
`new3d.h` `fast_floorf` → `floorf` in `FMOD`/`FDIV`, and `Random_Range`
float quotient → `% Max`. Same scenario, same console, same session, ≥60 s
each, alternating order. Compare `stage=1/2 total` per frame. Record both
numbers in the log header; then update the two `measured n/a` pins with
`measured <value>` only if the delta is real. A null result is a valid
result — record it, do not re-rerun until a win appears.

## 6. What the session does NOT need

- No per-frame trace or stage gates (P5: 5–10 ms per SD write).
- No Citra numbers — Citra is not OG hardware; it cannot clear any claim.
- No R4 retype (see §0).
- No new exemptions — pins are an offline review activity.
