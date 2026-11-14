# Ralph-pi task: profile and optimize Forsaken on physical Old 3DS using ATM

## Mandatory first actions, every fresh iteration

Before any code change, build, deploy, or test:
1. Read `perf/OPTIMIZATION-LOG.md` if present, `perf/BLOCKED.md`, `perf/LOOP-DONE.md`, and `git status --short --branch`.
2. Run `atm status`, `atm devmode`, `atm artifacts`, and `atm journal` using the controller below. Save the exact outputs in the iteration log. **Transient unreachability is retried, not filed**: if a command returns `unreachable` / `Connection refused`, wait 60 seconds and retry up to 3 times before treating it as a blocker. A refused or timed-out preflight is polled a few times over ~5 minutes before it becomes a blocker; never file `perf/BLOCKED.md` on the first transient failure.
3. Disk state selects the first incomplete phase. Do exactly ONE phase or ONE Phase-D iteration, write results, then end your turn. The outer `tools/ralph-pi.sh` relaunches a fresh agent.
4. If `perf/LOOP-DONE.md` exists, confirm and stop. If `perf/BLOCKED.md` exists, stop without retrying. Only if all retries are exhausted and the console remains unreachable, unresponsive, or reports `recovery_failure`/`incomplete_recovery`, write exact command/output to `perf/BLOCKED.md` and stop. Do not improvise a reboot, cleanup, ack, or retry.
5. Never commit or push. Keep successful code changes uncommitted for operator review.

## Objective

Use the actual physical Original 3DS at `192.168.4.167`, enrolled as `old3ds-lan167`, to profile and improve Forsaken. The ATM project is now named `netload_stub`; use that canonical path. Emulator, host, New 3DS, and OG-profile proxy results are not Old 3DS evidence. The first optimization target is duplicated stereo traversal. Do not fall back to the obsolete `192.168.4.52`/raw-3dslink netload_stub workflow.

## ATM docs and controller

Read the Obsidian runbook `10-projects/netload_stub/atm-autonomous-agent-runbook.md`, then its linked ATM notes: network API, deployment/launch, debugger, profiling/telemetry, developer mode, artifact management, troubleshooting. These docs and the controller are authoritative; do not invent commands/endpoints.

```sh
ATM=(python3 /mnt/nas/Development/netload_stub/tools/atmctl.py --ip 192.168.4.167 --device-id old3ds-lan167)
"${ATM[@]}" status
"${ATM[@]}" devmode
"${ATM[@]}" artifacts
"${ATM[@]}" journal
```

Verify all identity and state live. ATM's `recovery_stopped` means the documented three-consecutive-crash circuit breaker has tripped; it is not by itself network unavailability. A host-requested `atm run` is permitted only when the device is reachable, `operational=yes`, `recovery_ready=yes`, and no `recovery_failure`/`incomplete_recovery` exists. Preserve prior crash evidence. A run must be judged from its JSON stage evidence, journal, manifest, hashes, and retrieved results—not mere deploy/launch acknowledgement. `CRASH_FATAL`/`CRASH_ERRDISP` mean dump recovery occurred, not that the workload passed. Stop on `INCOMPLETE_RECOVERY`, `RECOVERY_FAILURE`, timeout, or unexplained state change.

ATM measures no generic CPU/GPU utilization or memory headroom. Frame timing must be produced by this application. `osGetTime` is 1 ms granularity and percentiles are bucket estimates; state this in all reports.

## Phase A — validate the hardware loop (first if missing)

Create `perf/OPTIMIZATION-LOG.md` with live device/firmware identity and smoke result. Run the existing workload:

```sh
"${ATM[@]}" run /mnt/nas/Development/netload_stub/atm-deploy/workloads/atm_workload_ok.3dsx ./perf/smoke --build-id smoke-<unique-id> --scenario atm-workload-ok
```

Require outcome `COMPLETED`, matching device/build/firmware, successful deploy/launch/run evidence, and verified retrieval manifest including `/3ds/atm_result.txt`. If a command fails, consult runbook troubleshooting and stop with exact evidence in `perf/BLOCKED.md`; do not retry blindly. Existing successful smoke output may count only if its manifest, run record, journal, build, and device identity satisfy these requirements and there is no later contradictory state.

## Phase B — instrument Forsaken (one-time)

Link shared `atm_core.c` and header from `/mnt/nas/Development/netload_stub/sysmodules/rosalina/` into a profiling-only Forsaken build. Call `atm_frame_add()` once per frame, bounded O(1), no allocation/I/O on the hot path. Add a fixed deterministic benchmark (same level, inputs, settings, frame count; never interactive play) behind profiling flags; write `atm_frame_format_summary()` and bounded app counters to `/3ds/atm_result.txt` and exit cleanly. Keep each matching `.elf` beside its `.3dsx`; wrong-build symbols invalidate debugging. Preserve release behavior outside profiling flags.

## Phase C — baseline

Run the same deterministic Old 3DS scenario three times into fresh `perf/baseline-<build-id>-<run>/` directories using `atm run`. Record per run: device ID, firmware ID, build ID and payload SHA-256, scenario, frame count, avg/p50/p95/p99/max, over-budget count, ATM outcome, and verified artifact paths/hashes. Three-run spread is the noise floor. Commit nothing.

## Phase D — one measured optimization per iteration

Start with stereo traversal. Verify the code before assuming the earlier claim: locate left/right callbacks, display-list recording/replay, and identify which gameplay, visibility/PVS, traversal, and draw-collection work runs per eye versus once per frame. Bank this source audit before implementing changes.

Then instrument bounded counters: per-eye traversal invocation/count and accumulated time, draw submissions per eye, and recorded/replayed list size. Establish identical deterministic mono and stereo baselines on the device. Candidate changes are separate iterations: first hoist measured eye-invariant gameplay/visibility work so it runs once per frame; only consider replay if measured savings exceed its overhead. Do not trade image quality or game behavior without explicitly recording a proposal rather than implementing it.

Every Phase-D iteration: read live ATM state; state a measured hypothesis and source; make exactly one minimal change; build; run the same scenario three times into fresh ATM run directories; compare median-of-three and p95/p99 against baseline/noise; take correctness screenshots in a separate non-timed run. Stereo changes require an `is3d=yes` manifest with both left and right eye images, correct disparity, and no eye-state leakage. Keep only gains beyond noise with correctness passed; otherwise revert. Append exact commands, outcomes, build/firmware IDs, artifact paths/hashes, before/after numbers, and keep/revert decision to `perf/OPTIMIZATION-LOG.md` before ending.

Use GDB only to localize a bottleneck, never for timed measurements. Require `svc_gdb=active:*` and use the exact `.elf` matching the deployed `.3dsx`; detach cleanly. Obsidian/Cognee/internet are for hypotheses, not evidence. Store only measured facts with build and firmware IDs.

## Stop conditions and final deliverables

Stop after 3 consecutive no-gain iterations, 15 Phase-D iterations, incomplete recovery/hard hang, or only trade-off/mass-rewrite ideas remain. For normal completion write `perf/LOOP-DONE.md`; for hardware/recovery blockers write `perf/BLOCKED.md` instead. Final summary lists measured improvement, survivors, dead ends, remaining unmeasured/blocked work, and all kept uncommitted source changes.

## Hard constraints

- Physical Old 3DS ATM runs are the primary and required profiling evidence.
- Never substitute emulator/New 3DS/host timings or interactive play for deterministic Old 3DS measurements.
- No SD/network I/O or unbounded allocation in hot paths; no casual threads.
- Never modify ATM/Luma firmware/controller or SD gate files.
- Never acknowledge/delete artifacts except via atmctl's documented verified flow.
- Never claim a result without exact build ID, firmware ID, scenario, and retrieved evidence.
- No commits/pushes without explicit operator approval.

Begin by determining the first incomplete phase from disk and live ATM state. Do exactly that phase/iteration, bank evidence, then end your turn.
