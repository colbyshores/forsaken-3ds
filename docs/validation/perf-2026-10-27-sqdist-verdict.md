# NODE_SQDIST verdict — REJECTED (no fidelity proof possible + zero measured gain)

Hardware: New 3DS XLRJ1080045448, 268 MHz OG clock, L1 16K/16K, no L2, mono.
Candidate: `NODE_SQDIST=1` — squared-distance compare in
`UpdateNearestNodeSpecial` (node.c), replacing one `sqrtf` per candidate node
in both traversal loops. Flag-gated, default OFF; never merged to
`experimental/performance_contract`.

Artifacts: `perf-2026-10-27-fishy-sqdist-baseline.log` (fresh same-session
baseline), `perf-2026-10-27-fishy-sqdist-candidate.log` (fresh candidate
rebuild; the stale `/tmp`-staged binaries from the previous day's tree state
failed to chainload three times — fresh rebuilds of the identical flag set
boot and run).

## 1. Fidelity gate: harness noise blocks certification

Fidelity harness (`FIDELITY_DUMP=1`, framelag pinned to 1.0 F, tick-driven
dwell 4000 ticks/level, per-tick FNV-1a state hash) works — 24/29 levels
were byte-stable across three same-binary full sweeps. But:

- 10/29 levels are nondeterministic run-to-run even with an identical
  binary: starship, bio-sphere, nukerf, ramqan, temple, nuken64, shipn64,
  aztec64, blackhole, genstation (the set widened from 5 to 10 when the
  per-tick component-hash compute changed frame timing).
- On every divergent level, ALL components (ship/enem/prim/sec) diverge
  together at the first checkpoint (tick 250) — consistent with a single
  upstream cause (RNG stream shift or a wall-clock leak into gameplay),
  not per-subsystem drift.
- The candidate's original fidelity pair differed from baseline on exactly
  the then-known-noisy levels, and the checkpointed baseline runs
  reproduced the candidate's hashes on 3 of 5 — the candidate never
  produced a divergence outside the harness's own noise band.

So the harness cannot certify the candidate as behavior-identical, and the
observed differences are indistinguishable from noise. Root cause of the
noise is open (prime suspect: a real-time dependency reaching gameplay
state; `timer.c` timers are wall-clock based, `SDL_GetTicks` = real time).
Not fixed this program; documented as a known limitation of the gate.

## 2. Performance A/B: zero gain

Focused fishy runs (`AUTOTEST_FIRST_LEVEL=28`, `MAX_LEVELS_TO_SWEEP=1`,
`PERF_CAPTURE=1`, teardown dump):

| build | frames | shipnode ms/frame | shippre | mROUT |
|---|---|---|---|---|
| baseline | 3483 | **4.5133** | 4.5202 | 8.9658 |
| NODE_SQDIST | 3487 | **4.5203** | 4.5270 | 8.9313 |

0.16% delta, well inside run-to-run noise. The `NodeInGroup` traversal is
memory-bound (scattered node list, 16 KB L1D, no L2), not compute-bound;
removing one correctly-pipelined `vsqrt.f32` per candidate node saves
nothing. This confirms the wave-4f hypothesis and closes the fishy
shipnode question: the 4.4–4.5 ms is the ported algorithm's inherent cost.

## 3. Decision

`NODE_SQDIST` stays default-OFF on `profiling/netload-capture`; it is NOT
cherry-picked to `experimental/performance_contract`. The candidate code
remains as a documented, flag-gated reference for whoever eventually fixes
the harness noise.

## 4. Program state after this verdict

Every optimization candidate raised by waves 1b–7 has now been resolved:
- HUD dedup B: A/B regression, reverted.
- fishy node-link reorder: bit-identical, ~0 gain, kept as strictly-less-work.
- hunk fragmentation: proven clean.
- DisplayBackground/ModelDisp: GPU-submission-bound, no safe win identified
  (wave-7 split captured separately).
- NODE_SQDIST: rejected here.

The remaining large costs (powerdown/ramqan cam 11.3–11.5 ms/frame mono,
powerdown ModelDisp 6.1 ms/frame, fishy shipnode 4.4 ms) are engine-design
or ported-math territory under AGENTS.md §5/§9 — out of scope for
autonomous optimization without fidelity review that the current harness
cannot yet provide.

## 5. Operational findings (device + harness)

- **Direct-boot crash on mid-list slots.** Focused runs that boot directly
  into slot 8 (powerdown) or slot 10 (starship) die within ~40 s of boot —
  before any level marker — while direct boot into slot 28 (fishy) and full
  sweeps from slot 0 run fine, including those same levels mid-sweep. The
  wave-7 powerdown split therefore moved to a full sweep. This is a real
  engine bug in the direct-boot path (autotest booting straight into a
  mid-list level), independent of the perf program; unfixed, documented.
- **`sdmc:/forsaken_perf.txt` is append-only** (`O_APPEND`, perf_capture_3ds.c:
  110) — artifacts accumulate across runs, so any analysis must take the last
  complete sweep's sections, and "stage-id present" is NOT proof of
  freshness. The focused pipeline's baseline-md5 guard (077a54b) covers the
  common failure; a length/section check would be stronger.
- **Stale `/tmp`-staged binaries failed to chainload three times** while
  fresh rebuilds of the identical flag set booted. Lesson: always rebuild
  from the current tree immediately before pushing; do not trust binaries
  staged hours earlier.
