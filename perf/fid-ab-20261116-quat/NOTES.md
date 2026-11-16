# Phase 2 fidelity A/B — QUAT/QUATLERP class conversion (2026-11-16)

Console: physical Old 3DS old3ds-lan167 192.168.4.167, OG clock, lid
closed_blanked, forced_stereo bench. Firmware 9f3632fe047c+dirty on every
status (unchanged across the batch; matches the hwval-20261116-merge5 and
fid-ab-20261116 batches, so the harness stayed validated — no Phase A
re-smoke required). Session be9bb750.

## Builds (identical flags, differing only by the QUAT conversion)

- Baseline: experimental/cpp-phase2 @ 6d8c2b3 (Cleanup 5 tip; QUAT free
  functions). fidq-base.3dsx 3536484 B.
- Candidate: experimental/cpp-phase2 @ 623916d (QUAT/QUATLERP classes,
  252 call sites migrated). fidq-tip.3dsx 3536468 B.
- Flags: EDITION=remaster RENDERER=citro3d SLIM=1 ATM_PROFILING=1
  ATM_BENCH_LEVEL=ramqan ATM_BENCH_FRAMES=240 ATM_BENCH_WARMUP=30
  ATM_BENCH_STEREO=1 FIDELITY_DUMP=1. Harness machinery (fidelity_3ds.c,
  atm_prof_3ds.c, guarded atm_prof.h/fidelity.h) copied from the prior
  scratch trees into git worktrees /tmp/fidq-base, /tmp/fidq-tip; Makefile
  wiring block inserted verbatim from /tmp/fid-base-src. romfs/ symlinked
  from the main tree (worktrees lack untracked data). Scratch only —
  nothing harness-related committed.
- Scenario: ramqan slot=19 resolve=exact stereo=forced_stereo, 240 frames,
  warmup 30, clean exit (status=complete both runs). QUAT is exercised
  pervasively on this scenario: enemy/ship orientation every frame
  (MakeEuler, Multiply, ToMatrix, FromDirAndUp, From2Vectors, Slerp,
  Step/QuatInterpolate paths all live in the ramqan bench).

## Runs (boot-count proven)

- Preflight boot 629, recovery_ready=yes, stub 17491 open.
- Base run (boot 629 session): status=complete frames=240 avg_us=74301
  max_us=110786 level_load_ms=30299 checksum=b39d8438. transfers.log
  status=ok bytes_out=3536484.
- Reboot 629->630, stub open.
- Tip run (boot 630): status=complete frames=240 avg_us=74301
  max_us=110786 checksum=b39d8438. transfers.log status=ok
  bytes_out=3536468.
- Final reboot 630->631, recovery_ready=yes, stub 17491 OPEN (listening).

## Verdict: PASS — fidelity byte-identical

`cmp base-fidelity.txt tip-fidelity.txt` -> identical. All four FIDC lines
match (tick 60/120/180/240, all ship/enem/prim/sec sub-hashes). tick240
hash=594bab3c also matches the hwval-20261116-merge5 and fid-ab-20261116
references. Timing profile identical between runs (avg_us 74301, max_us
110786 both). atm_result checksum b39d8438 identical both runs.

## Caveats

- atm del returned deleted:false for both 3dslink-pushed payloads (3dslink
  files are not in the ATM staged registry; absence not provable via
  artifacts — same limitation as the prior batches). No crash dumps or run
  records deleted.
- Scratch worktrees /tmp/fidq-base, /tmp/fidq-tip and build dirs are
  throwaway; `git worktree remove` pending housekeeping.
- KB path used for this iteration: disk fallback /mnt/nas/Vaults/port-docs
  (obsidian/cognee MCP endpoints unreachable, connection refused); the
  canonical procedure source was this repo's perf/fid-ab-20261116/NOTES.md.
