# Wave-5 findings — hunk allocator clean; fresh-heap lead closed

Hardware: New 3DS XLRJ1080045448, 268 MHz OG clock, L1 16K/16K, no L2,
mono (cam stage count = 0.99x frames — confirmed 1 camera traversal/frame).
Build: `profiling/netload-capture` + hunk stats in level marker.
Artifact: `perf-2026-10-26-wave5.log`, md5 `88616a0221e0ade89c9812a37d27cd8f`.

## 1. Hunk allocator: no leak, no cross-level fragmentation

`Hunk_Used`/`Hunk_Available` printed at every level marker across the full
29-level sweep:

- TAG_LEVEL usage at each level start: 10.3–24.9 KB, no upward trend.
- TAG_STARTUP usage: 0 for the entire sweep.
- Hunk_Available: flat at ~8.37 MB of 8 MB arena from level 1 to 29.

`Hunk_FreeAll(TAG_LEVEL)` on level transition works as designed
(hunk_alloc.h, oct2.c:2015/2070). **The fresh-heap vs mid-sweep fps gap
(ramqan 58.7 fresh vs ~46 mid-sweep) is NOT hunk fragmentation.**
Remaining candidates (non-hunk malloc fragmentation, allocator-external
state) are speculative; the gap is also partly confounded by different
instrumentation builds per wave. Lead closed as not-actionable at current
evidence level.

## 2. Stereo clarification

Wave-1b ran stereo ON (2.1x camera calls); waves 2b/4/5 run mono
(1.0x). Mono cam numbers are the shipping-relevant ones: ramqan 11.3,
powerdown 11.5 ms/frame vs 2.5–7 elsewhere. Wave-1b stereo figures must
not be compared to later waves.

## 3. DisplayBackground (visi.c:1432) — no safe win

Remaining bg cost (ramqan 3.7, powerdown 2.3 ms/frame mono) is per-visible
group: ClipGroup + XLight1Group (already GPU-shader-backed, AGENTS.md §6)
+ ExecuteSingleGroupMloadHeader display-list submission (already behind
the ProcTex re-bind cache). Cost scales with visible geometry; the two
worst levels are the two most visually open levels. No redundant per-frame
work found that could be removed without changing rendered output.
Stereo display-list replay is a measured, shipped tradeoff (§6) — not
reopened.

## 4. Program-level summary (waves 1b–5)

| Candidate | Verdict |
|---|---|
| fishy `shipnode` 4.4 ms | `NodeInGroup` traversal, zero LOS rays; inherent ported algorithm; reorder = 0 gain; documented, not shipped |
| HUD dedup B | A/B regression (-43%/-31% same-slot); reverted |
| hunk fragmentation | clean (this wave) |
| DisplayBackground bg | GPU-submission-bound, already optimized; no safe win |
| ramqan/powerdown cam 11.3/11.5 mono | spread across vis/bg/opaque/trans, no single hotspot (wave-1b) |

Net shipped runtime optimizations from waves 1b–5: **none survived A/B**.
The measurement program's value: three hypotheses killed with evidence
(B, node reorder, hunk), the fishy hotspot root-caused and bounded, and
the remaining big costs (cam on ramqan/powerdown, shipnode on fishy)
identified as engine-design/ported-math territory that per AGENTS.md §5
must not be rewritten without fidelity review — out of scope for
autonomous optimization.
