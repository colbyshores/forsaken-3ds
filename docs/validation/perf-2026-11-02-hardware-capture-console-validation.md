# Hardware PERF console-class validation

Date: 2026-11-02. Ralph-pi iteration 013. Loop: `ralph-pi`, branch
`experimental/native-renderer`.

## Purpose and scope

Iteration 012 added `tools/perf/analyze_hardware_capture.py` for the pending
manual hardware captures. Its `--require-og-profile` check accidentally
combined two independent facts:

- `OG_PROFILE=1` is a build-time clock/cache/heap policy;
- `is_n3ds=1` identifies a New 3DS.

Consequently, a valid original/non-New 3DS run (`is_n3ds=0`) with the
OG-profile policy was rejected, while a New 3DS proxy was accepted without
making the console class explicit. This iteration separates those checks so
the human handoff can validate either console family without mislabeling it.

This is tooling-only. No engine code, default policy, struct, or data layout
changed, and no console was contacted. Parser success is **not** an
`AGENTS.md` P6 hardware pass.

## Change

`tools/perf/analyze_hardware_capture.py` now:

- treats `--require-og-profile` as the policy check only:
  `og_profile=1 clock=268MHz cache=L2-off tier=OG_PROFILE`;
- adds `--require-console og|n3ds`, using the boot record's `is_n3ds=0|1`;
- reports the detected console class and separate console/profile/verifier
  check statuses in text and JSON output;
- requires `--boot` whenever a console or profile check is requested;
- keeps verifier, profile, and console failures independently attributable.

`docs/validation/ralph-pi-hardware-handoff.md` now gives explicit validation
commands for original/non-New 3DS (`--require-console og`) and New 3DS
(`--require-console n3ds`) hardware, while retaining the warning that parser
success does not replace the P6 environment record or visual checklist.

## Verification

The following commands were run. The PERF fixtures and boot files are
synthetic/parser self-tests except where an existing emulator artifact is
explicitly named; none is a hardware capture.

```sh
python3 -m py_compile tools/perf/analyze_hardware_capture.py
# exit=0

python3 tools/perf/analyze_hardware_capture.py \
  --perf build_3ds/ralph-pi/iter13/console-selftest/manual-format-fixture.perf \
  --boot build_3ds/ralph-pi/iter13/console-selftest/og-boot-fixture.log \
  --require-og-profile --require-console og --block last-dump \
  --out build_3ds/ralph-pi/iter13/console-selftest/og-policy-console-analysis.txt \
  --json build_3ds/ralph-pi/iter13/console-selftest/og-policy-console.json
# exit=0; console class=OG/non-New-3DS; console-class check=PASS;
# og-profile policy check=PASS

python3 tools/perf/analyze_hardware_capture.py \
  --perf build_3ds/ralph-pi/iter13/console-selftest/manual-format-fixture.perf \
  --boot build_3ds/ralph-pi/iter13/console-selftest/og-boot-fixture.log \
  --require-og-profile --require-console n3ds --block last-dump
# exit=2 (expected negative: console class is OG/non-New-3DS,
# expected New 3DS)

python3 tools/perf/analyze_hardware_capture.py \
  --perf build_3ds/ralph-pi/iter13/console-selftest/manual-format-fixture.perf \
  --boot build_3ds/ralph-pi/iter13/console-selftest/n3ds-boot-fixture.log \
  --require-og-profile --require-console n3ds --block last-dump \
  --out build_3ds/ralph-pi/iter13/console-selftest/n3ds-policy-console-analysis.txt \
  --json build_3ds/ralph-pi/iter13/console-selftest/n3ds-policy-console.json
# exit=0; console class=New 3DS; console-class check=PASS;
# og-profile policy check=PASS

python3 tools/perf/analyze_hardware_capture.py \
  --perf build_3ds/ralph-pi/iter13/console-selftest/manual-format-fixture.perf \
  --boot build_3ds/ralph-pi/iter13/console-selftest/n3ds-boot-fixture.log \
  --require-og-profile --require-console og --block last-dump
# exit=2 (expected negative: console class is New 3DS,
# expected OG/non-New-3DS)

python3 tools/perf/analyze_hardware_capture.py \
  --perf build_3ds/ralph-pi/iter13/console-selftest/manual-format-fixture.perf \
  --require-console og --block last-dump
# exit=2 (expected negative: console/profile checks need --boot)

python3 tools/perf/analyze_hardware_capture.py \
  --perf build_3ds/ralph-pi/iter13/console-selftest/manual-format-verify-fixture.perf \
  --boot build_3ds/ralph-pi/iter13/console-selftest/n3ds-boot-fixture.log \
  --require-og-profile --require-console n3ds --require-verify --block last-dump \
  --out build_3ds/ralph-pi/iter13/console-selftest/n3ds-verifier-analysis.txt \
  --json build_3ds/ralph-pi/iter13/console-selftest/n3ds-verifier.json
# exit=0; verifier groups=2353 entries=3882 mismatches=0 first_group=-1;
# source=selected-block

python3 tools/perf/analyze_hardware_capture.py \
  --perf build_3ds/ralph-pi/evidence/perf-20261102-150411-iter12-ramqan-bucket-verify2.log \
  --boot build_3ds/ralph-pi/evidence/perf-20261102-150411-iter12-ramqan-bucket-verify2.log.boot \
  --require-verify --require-og-profile \
  --out build_3ds/ralph-pi/iter13/console-selftest/existing-artifact-analysis.txt \
  --json build_3ds/ralph-pi/iter13/console-selftest/existing-artifact.json
# exit=0; existing emulator artifact remains readable; console class=New 3DS;
# og-profile policy check=PASS; verifier groups=2353 entries=3882
# mismatches=0 first_group=-1; source=level-block
```

The required final-tree build and gates were also run:

```sh
export DEVKITARM=/opt/devkitpro/devkitARM DEVKITPRO=/opt/devkitpro
make -f Makefile.3ds EDITION=remaster AUTOTEST_REMASTER=1 \
     AUTOTEST_MAX_LEVELS_TO_SWEEP=1 AUTOTEST_MS_PER_LEVEL=8000 \
     PERF_CAPTURE=1 OG_PROFILE=1 RENDERER=citro3d SLIM=1 -j"$(nproc)"
# exit=0; log: build_3ds/ralph-pi/iter13-build.log

tools/perf/check.sh
# exit=0; log: build_3ds/ralph-pi/iter13-gates.log
```

The final `forsaken3ds.3dsx` md5 is
`eba28c890c6a123804d85a555c757b2e` and sha256 is
`fb6e80b46aedb29211933a84277c7b1b312bb09e4f598f4d6f8186501243887e`,
identical to iteration 012's default-off binary. This is expected because
only host tooling and documentation changed.

## Limits

- No console was contacted. There is no OG/New 3DS hardware timing, stereo,
  or visual-equivalence result.
- The synthetic OG boot fixture proves classification logic only; it is not
  evidence that an OG console was used.
- The parser cannot infer stereo mode, camera route, gameplay state, or
  visual equivalence. Those remain human P6/checklist items.
- No emulator A/B was run because this iteration changes no engine or
  renderer behavior and makes no performance claim.
- Alignment check: no engine source, struct, or data layout changed; there is
  no new alignment case.
