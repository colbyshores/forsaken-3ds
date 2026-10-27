# Wave-7 — ModelDisp split (full remaster sweep, mono)

Hardware: New 3DS XLRJ1080045448, 268 MHz OG clock, L1 16K/16K, no L2,
mono. Build: `profiling/netload-capture`, `PERF_CAPTURE=1 AUTOTEST_REMASTER=1`.
Artifact: `perf-2026-10-27-wave7.log`; analysis table:
`perf-2026-10-27-wave7-analysis.txt`.

## powerdown / ramqan

Use the final cumulative per-level snapshot with the new ModelDisp stages
(stages 48–50), not earlier snapshots from the same append-only capture.

| level | cam | opaque | ModelDisp | InterpFrames | lighting | ExecuteMxaloadHeader | models/frame |
|---|---:|---:|---:|---:|---:|---:|---:|
| powerdown | 11.63 | 6.59 | 6.30 | 0.12 | 0.02 | 0.14 | 5.91 |
| ramqan | 11.36 | 3.19 | 2.65 | 0.00 | 0.00 | 0.00 | 0 |

Powerdown opaque is almost entirely `ModelDisp` (6.30 ms/frame, 96% of
opaque). However the `MDLINTERP`/`MDLLIGHT`/`MDLEXEC` sub-stage totals are
only 0.29 ms/frame combined (the stage gate is around the visible model
path; the rest of ModelDisp work is matrix/LOD/state setup and/or accounting
mismatch). The workload draws about 5.9 model submissions/frame. InterpFrames
is therefore **not** the dominant powerdown cost: only 0.12 ms/frame. The
previous assumption that morph interpolation was the obvious next win was
wrong; its apparent cost is small here.

For ramqan the sub-stages are zero because the profile did not include any
eligible `ModelDisp` model execution in that snapshot (0 `MDLEXEC` calls);
its 2.65 ms/frame ModelDisp wrapper time is elsewhere in the function and
is not represented by the current sub-stage gates.

The totals do not reconcile: `ModelDisp` wrapper (6.136 ms/frame) exceeds
its three measured sub-stages (~0.285 ms/frame) by ~5.85 ms/frame. Treat
this as an instrumentation coverage gap, not as an attribution. Likely work
is in ModelDisp pre-loop/dispatch or a wrong gate region; a local profiler
pass is needed before making any GPU-morph or CPU rewrite proposal.

## Overall wave-7 table

`perf-2026-10-27-wave7-analysis.txt` contains the captured per-level stage
breakdown. FPS is a measured scene rate, not a 71 Hz budget claim; these
were mono New 3DS captures at the OG 268 MHz clock.

## Decision

No new runtime optimization emerged. `InterpFrames` is not a large fraction
of the powerdown camera budget, so a GPU morph rewrite is not justified by
this data. Preserve the existing morph implementation. The important result
is narrowing the next measurement target: instrument the whole `ModelDisp`
function by sub-region (entry/lookup/setup, flags/lighting selection,
execute-submission) with balanced gates and validate their sum against the
wrapper before proposing a change.
