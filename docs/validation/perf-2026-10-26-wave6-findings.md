# Wave-6 findings — ModelDisp is the render hotspot; bullets are 100% primary

Hardware: New 3DS XLRJ1080045448, 268 MHz OG clock, L1 16K/16K, no L2, mono.
Build: `profiling/netload-capture` + opaque-loop sub-stages + bull prim/sec
split. Artifact: `perf-2026-10-26-wave6.log`, md5
`21500fb667846d057c7455e204f7fc13`.

## 1. Opaque loop decomposes to ONE call: ModelDisp

Per-frame ms (last per-level dump; opaque sub-stages sum to ~95% of opaque):

| level | cam | opaque | oqtex | oqclip | oqlin | **oqmdl** | oqpol | resid |
|---|---|---|---|---|---|---|---|---|
| powerdown | 11.57 | 6.55 | 0.01 | 0.04 | 0.01 | **6.10** | 0.05 | 0.35 |
| ramqan | 11.22 | 3.05 | 0.03 | 0.04 | 0.01 | **2.54** | 0.05 | 0.39 |
| thermal | 5.48 | 2.57 | 0.01 | 0.02 | 0.00 | **2.32** | 0.03 | 0.20 |
| temple | 5.45 | 1.81 | 0.01 | 0.02 | 0.00 | **1.55** | 0.04 | 0.20 |
| fishy | 3.31 | 0.82 | 0.00 | 0.01 | 0.00 | **0.64** | 0.02 | 0.14 |

`ModelDisp` (models.c:1248) per visible model: matrix build + LOD select +
`InterpFrames` (per-vertex morph interpolation) + per-model lighting
(`XLightMxaloadHeader`/`UpdateMxaModel`) + `ExecuteMxaloadHeader` display
submission. powerdown's 6.10 ms/frame is the single largest render cost in
the game. InterpFrames' FMOD/floorf debt is already burned down
(fast_floorf, new3d.h:114) — remaining cost is per-vertex memory traffic +
submission, not libm. Sub-call split (Interp vs lighting vs exec) not yet
measured — needs a focused powerdown run with ModelDisp sub-stages.

## 2. Bullets are 100% primary

`BULLETS` nests exactly: ramqan 3.29 = `BULLPRIM` 3.29 + `BULLSEC` 0.00.
`ProcessSecondaryBullets` is free; all bullet cost is
`ProcessPrimaryBullets` (swept-collision primary beams/lasers with
per-bullet BackgroundCollide windows — protected hit set, R3 exempt).

## 3. Wave-5 cam sub-stage note

Wave-5 already carried vis/opaque/trans (stages 7/9/10); the analyzer just
hadn't printed them. powerdown opaque was already visible there (6.48).

## 4. Next

- Fidelity harness (fidelity_3ds.c, FIDELITY_DUMP=1): per-tick state
  checksum, framelag pinned to 1.0F, tick-based autotest dwell — proves
  bit-identical gameplay for the squared-distance node candidate
  (NODE_SQDIST=1) before any ship decision.
- Optional wave-7: ModelDisp sub-stage split on focused powerdown
  (InterpFrames vs lighting vs ExecuteMxaloadHeader).
