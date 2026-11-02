# Hardware PERF artifact parser/validator

Date: 2026-11-02. Ralph-pi iteration 012. Loop: `ralph-pi`, branch
`experimental/native-renderer`.

## Purpose and scope

The manual hardware handoff binaries write a normal teardown PERF artifact:

```text
PERF: dump begin
PERF: stage=0 count=...
...
PERF: frames=N
PERF: dump end
```

They intentionally have **no** `PERF: === level=... ===` marker. The existing
`tools/perf/analyze_sweep.py` keys its rows on level markers, so it silently
ignores that shape. This is an analysis gap for the pending human OG/New 3DS
session, not a renderer or performance change.

This iteration adds `tools/perf/analyze_hardware_capture.py`, a standalone
PERF artifact parser/validator. It is **not** a performance measurement and
**not** an AGENTS.md P6 hardware pass. It checks that a copied PERF artifact
has a usable, complete gameplay block and, when requested, that the opt-in
`MODEL_BUCKET_VERIFY=1` result line is present and zero-mismatch.

## What the parser checks

- selects the gameplay level block for emulator sweep artifacts or the last
  complete teardown dump for manual hardware artifacts (`--block auto`);
- requires nonzero gameplay (`stage=0`) and `MainGame` frames unless
  `--allow-teardown-dump` is used for a non-gameplay diagnostic;
- checks that the `frame` stage count matches `PERF: frames=`;
- optionally requires the OG-profile boot fields
  `is_n3ds=1 og_profile=1 clock=268MHz cache=L2-off tier=OG_PROFILE`
  (`--require-og-profile --boot ...`);
- optionally requires the bucket verifier line with
  `groups>0`, `entries>0`, `mismatches=0`, `first_group=-1`,
  `first_expected=0`, `first_actual=0` (`--require-verify`);
- emits a per-stage ms/frame and count/frame table, the analyzer's
  worst-frame-excluded frame ms/FPS convention, the frame histogram, and an
  optional `--json` summary.

Exit codes: `0` mechanical checks pass, `2` validation failure.

## Why the OG-profile fields

`OG_PROFILE=1` is still only a proxy for clock/heap/tier policy. Requiring the
boot fields does not turn the capture into a hardware pass; it catches a
mislabeled artifact before anyone quotes its timing. The P6 environment fields
still have to be recorded per `docs/validation/capture-protocol.md` §2, and
stereoscopic mode must be stated.

## Parser self-tests

All of the following were run against existing evidence or a synthetic manual
format fixture. No new emulator sweep was run because this iteration changes
no engine behavior.

```sh
tools/perf/analyze_hardware_capture.py \
  --perf build_3ds/ralph-pi/evidence/perf-20261102-150411-iter12-ramqan-bucket-verify2.log \
  --boot build_3ds/ralph-pi/evidence/perf-20261102-150411-iter12-ramqan-bucket-verify2.log.boot \
  --require-verify --require-og-profile \
  --out build_3ds/ralph-pi/iter012/parser-selftest/verify2-analysis.txt \
  --json build_3ds/ralph-pi/iter012/parser-selftest/verify2.json
# exit=0; groups=2353 entries=3882 mismatches=0

tools/perf/analyze_hardware_capture.py \
  --perf build_3ds/ralph-pi/evidence/perf-20261102-141700-iter11-default-smoke.log \
  --boot build_3ds/ralph-pi/evidence/perf-20261102-141700-iter11-default-smoke.log.boot \
  --require-og-profile \
  --out build_3ds/ralph-pi/iter012/parser-selftest/default-smoke-analysis.txt \
  --json build_3ds/ralph-pi/iter012/parser-selftest/default-smoke.json
# exit=0; frame ms 19.590, fps 51.047 (existing artifact re-read)

# Synthetic manual-format fixture made from the existing default-smoke
# gameplay block: dump begin/end only, no level marker. NOT a hardware capture.
tools/perf/analyze_hardware_capture.py \
  --perf build_3ds/ralph-pi/iter012/parser-selftest/manual-format-fixture.perf \
  --boot build_3ds/ralph-pi/evidence/perf-20261102-141700-iter11-default-smoke.log.boot \
  --require-og-profile --block last-dump \
  --out build_3ds/ralph-pi/iter012/parser-selftest/manual-format-fixture-analysis.txt \
  --json build_3ds/ralph-pi/iter012/parser-selftest/manual-format-fixture.json
# exit=0; proves the no-level-marker handoff shape is parsed

# Synthetic manual-format verifier fixture made from the existing verifier
# gameplay block: dump begin/end with the verify line inside the dump. NOT a
# hardware capture.
tools/perf/analyze_hardware_capture.py \
  --perf build_3ds/ralph-pi/iter012/parser-selftest/manual-format-verify-fixture.perf \
  --boot build_3ds/ralph-pi/evidence/perf-20261102-150411-iter12-ramqan-bucket-verify2.log.boot \
  --require-verify --require-og-profile --block last-dump \
  --out build_3ds/ralph-pi/iter012/parser-selftest/manual-format-verify-fixture-analysis.txt \
  --json build_3ds/ralph-pi/iter012/parser-selftest/manual-format-verify-fixture.json
# exit=0; verifier source=selected-block, mismatches=0

tools/perf/analyze_hardware_capture.py \
  --perf build_3ds/ralph-pi/evidence/perf-20261102-141700-iter11-default-smoke.log \
  --require-verify
# exit=2 (expected negative: verifier line is absent without --require-verify
# being satisfied)

# Appended/stale-file selection check: two copies of the same artifact; the
# parser selects the last gameplay level block (lines 146-223).
cat build_3ds/ralph-pi/evidence/perf-20261102-141700-iter11-default-smoke.log \
    build_3ds/ralph-pi/evidence/perf-20261102-141700-iter11-default-smoke.log \
    > build_3ds/ralph-pi/iter012/parser-selftest/appended-twice.perf
tools/perf/analyze_hardware_capture.py \
  --perf build_3ds/ralph-pi/iter012/parser-selftest/appended-twice.perf \
  --boot build_3ds/ralph-pi/evidence/perf-20261102-141700-iter11-default-smoke.log.boot \
  --require-og-profile \
  --out build_3ds/ralph-pi/iter012/parser-selftest/appended-twice-analysis.txt \
  --json build_3ds/ralph-pi/iter012/parser-selftest/appended-twice.json
# exit=0; selected the later level block
```

## Limits

- No console was contacted and no hardware timing, stereo, or visual result
  exists in this iteration.
- The parser does not compare two captures; use it to validate each artifact
  first, then compare the reported per-stage tables (or the existing analyzer
  output for emulator level sweeps).
- `OG_PROFILE=1` remains a proxy, not PICA200 timing; the parser's boot check
  does not change that.
- The working tree also contained a pre-existing, uncommitted refresh of
  `docs/validation/ralph-pi-hardware-handoff.md` from another iteration. This
  iteration did not modify that file.
