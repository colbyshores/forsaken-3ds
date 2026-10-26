# Wave-4 findings — ProcessShips attribution + fishy node hotspot

Hardware: New 3DS XLRJ1080045448, CPU boost 268 MHz (OG clock), L1 16K/16K,
no L2, mono. Build: `profiling/netload-capture`, `EDITION=remaster SLIM=1
PERF_CAPTURE=1 AUTOTEST_REMASTER=1`. Engine ticks 71 Hz; ms = ticks/268111840*1000.

Artifacts: `perf-2026-10-26-wave4.log` (full sweep, md5
`8992d9a3f59d42d61c74e818f66fdb71`), `perf-2026-10-26-wave4-analysis.txt`,
`perf-2026-10-26-wave4-fishy-focus.log` (focused fishy, `AUTOTEST_FIRST_LEVEL=28
AUTOTEST_MAX_LEVELS_TO_SWEEP=1`), `perf-2026-10-26-wave4-fishy-rays.log`
(ray-counter diagnostic).

## 1. ProcessShips breakdown (fishy, 4.67 ms/frame total)

Focused fishy run, per-frame means:

| stage | ms/frame | verdict |
|---|---|---|
| shippre (incl. shipnode) | 4.44 | |
| **shipnode** (`UpdateNearestNodeSpecial`) | **4.41** | the hotspot |
| shipdmg | ~0.00 | clear |
| shipmode | ~0.02 | clear |
| shipcollide | 0.16 | clear |
| shippost | 0.20 | clear |

`UpdateNearestNodeSpecial` (node.c, player-only, `WhoIAm==i`) is ~94% of
ProcessShips and ~50% of MainRoutines on fishy.

## 2. What shipnode is NOT: LOS rays

Wrapped both `AI_ClearLOS` call sites with a ray counter (stage 40):
**zero rays fire** on fishy. The `Temp < Distance` gate almost never passes
(the current nearest node is genuinely nearest), so the cost is the
`NodeInGroup[Group]` traversal itself: per-node integer NetMask gate +
`DistanceVector2Vector` (sqrtf) + pointer chase over a scattered node list
on a 16 KB L1D with no L2 — memory-bound.

The bit-identical integer-gate reorder (NetMask/Group checked before the
sqrtf in the link loop, commit `8c98018`) measured **~0 gain** (4.415 →
4.410 ms) — kept as strictly-less-work, not as a win. Not cherry-picked to
`experimental/performance_contract`.

Cutting the traversal (squared-distance compare, spatial skip, throttling)
changes which node wins at ULP/tie boundaries and bots read `NearestNode` —
ported gameplay math per AGENTS.md §5. **Documented as inherent cost; not
fixed.**

## 3. Cross-level picture (full sweep, wave-4)

- Render side dominates the worst levels: cam = 11.2 (ramqan) / 11.5
  (powerdown) vs 2.5–7 elsewhere; bg 2.3–3.7 on those two.
- Simulation side: fishy mROUT 8.7 (half of it shipnode), temple 5.8
  (enem 1.7 + bull 3.5), ramqan bull 3.4.
- The wave-4 fps column is not comparable across waves (40-stage
  instrumentation build); use frame counts and stage deltas.

## 4. HUD dedup (change B) — REJECTED

Control A/B at the same sweep slot: ramqan 3523 frames (control) vs 2016
(B); powerdown 3405 vs 2333. B is conclusively worse, mechanism
unexplained; reverted on `experimental/performance_contract`. Do not
revisit without a new hypothesis.

## 5. Open lead: fresh-heap vs mid-sweep gap

Control ramqan as first level after boot: 58.7 fps vs ~46 fps mid-sweep in
wave-2b. Hunk already bulk-frees TAG_LEVEL per level (oct2.c:2015/2070),
so the gap is not TAG_LEVEL cursor growth — candidates are TAG_STARTUP
growth, non-hunk malloc fragmentation, or allocator-external state.
Wave-5 (full sweep with `Hunk_Used`/`Hunk_Available` printed at every level
marker) tests this directly.
