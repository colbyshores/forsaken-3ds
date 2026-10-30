# MODEL_BUCKET repeat A/B — supported, but not a 5× win

Hardware: New 3DS, 268 MHz OG clock, L1I/L1D 16 KB, no L2, mono.
Builds are same source tree, identical flags except `MODEL_BUCKET=1`.
Baseline: `/tmp/unified_base_perf.txt`; bucket candidate:
`/tmp/unified_bucket_perf.txt`.

| level | baseline fps | bucket fps | ModelDisp ms/frame baseline → bucket | MDLENTRY iterations/frame baseline → bucket | cam ms/frame baseline → bucket |
|---|---:|---:|---:|---:|---:|
| powerdown | 16.0 | 20.7 | 10.82 → 5.77 (-47%) | 5849 → 56 | 16.47 → 11.24 |
| ramqan | 16.1 | 19.7 | 5.40 → 2.10 (-61%) | 3509 → 24 | 14.07 → 10.81 |
| fishy | 36.9 | 36.8 | 0.79 → 0.68 (-14%, negligible) | 143 → 8 | 3.43 → 3.32 |

## Artifact and instrumentation validity

The previous repeat A/B artifacts `ab2_runA_base.txt` / `ab2_runB_bucket.txt`
are invalid and must not be used for verdicts: the pre-fix baseline had
`MDLENTRY count=0` and `MDLWRAP count/frame≈6`, while the bucket build had
normal counts. A split loop introduced that instrumentation defect. The
loop was unified and both variants rebuilt from the same current tree; the
captures in the named `/tmp/unified_*` files are the corrected comparison.

Corrected runs agree with the earlier signal in direction and scale: powerdown
+4.7 fps, ramqan +3.6 fps, fishy unchanged. The number of MDLENTRY body
executions drops by ~100× on the levels with many visible groups. This is
causal evidence that the intended optimization path is active. However the
camera stage also falls substantially (about 32% on powerdown and 23% on
ramqan), likely because reducing scattered Models[] reads relieves the same
memory system; the two stage deltas are correlated and should not be added as
independent savings.

## Code audit / equivalence argument

`ModelVisBucket_Build` performs one source-list-ordered pass, evaluates the
same visibility predicate and clip-group expression once, and tail-appends
each visible model to its clip-group list. `ModelDisp(group)` iterates that
bucket in preserved list order and executes the unchanged model body. Thus
for a fixed camera snapshot the emitted per-group model sequence is identical
to the baseline (same predicate, clip group, linked-list order); candidate
removes only repeated tests and traversal of unrelated groups.

Known assumptions to validate on a debug display path:
- model Visible/Group/Flags/ClipGroup and the IsGroupVisible/VisibleOverlap
  inputs remain unchanged during a camera's opaque-group loop; if gameplay or
  render callbacks mutate them mid-loop the per-camera memo is stale.
- every group id indexing the bucket arrays is within `MAXGROUPS` (matching
  existing group-array assumptions).

`MODEL_BUCKET` remains default-off pending an in-game visual sanity run on the
render-heavy levels. It is not a simulation-state optimization, so the current
per-tick gameplay checksum does not prove render equivalence.
