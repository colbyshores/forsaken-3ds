# ATM Optimization Log — Forsaken, Original 3DS

## Phase A — harness validation

- Device ID: old3ds-lan167
- Device IP: 192.168.4.167
- Firmware: 22c4bb74bc64+dirty
- Current session: 788e0bbd
- Boot count/state: 51 / ok
- Devmode: armed=yes, active=yes; svc_atm=active:devmode; svc_gdb=active:devmode; svc_wifi=active:atm
- Operational/recovery_ready: yes / yes
- Lid: closed_blanked
- ATM log reported recovery=none at start. The smoke run's latest status now reports recovery=recovery_stopped and auto_dump_present=not_ok; this is a significant state transition and is recorded in perf/BLOCKED.md. No further device actions were taken.

### Smoke result

- Run: run-788e0bbd-1
- Workload: atm_workload_ok.3dsx
- Firmware/build ID: 22c4bb74bc64+dirty / smoke
- Retrieved result: 240 frames, duration_ms=3790, avg_us=15791, p50=18000, p95=19000, p99=22000, max=22000, over_budget=125.
- Artifact manifest: perf/smoke/retrieval_manifest.json; workload result and marker were retrieved_verified.
- Journal: state=exited, launch_result=0x00000000, failure=none, exception_path=none.
- Despite a valid workload result, the smoke did NOT cleanly validate the harness: current console recovery is stopped and auto_dump_present=not_ok after multiple earlier crash artifacts. Do not proceed to profiling until operator/tooling triage resolves this contradiction.
- Timing note: osGetTime granularity is 1 ms and percentiles are bucket estimates. This is the harness workload's sleep-paced timing, not a Forsaken performance result and not CPU/GPU utilization.

## Fresh iteration — mandatory live preflight blocked

- Disk selection at iteration start: Phase A remains incomplete; `perf/LOOP-DONE.md` was absent and `perf/BLOCKED.md` was absent before this iteration.
- Required docs read: the Obsidian ATM autonomous-agent runbook and its linked network API, deployment/launch, debugger, profiling/telemetry, developer mode, artifact management, and troubleshooting notes.
- Required repository state read: `git status --short --branch` reported branch `experimental/native-renderer` with existing modified/untracked files; this iteration did not alter them.
- Live result: the enrolled physical Original 3DS at `192.168.4.167` (`old3ds-lan167`) was unreachable because HTTP connections were refused. Per the task stop condition, this iteration stopped without a build, deploy, workload, retry, reboot, cleanup, or acknowledgement. Exact evidence is recorded in `perf/BLOCKED.md`.

### Exact mandatory live-check command

```sh
ATM=(python3 /mnt/nas/Development/netload_stub/tools/atmctl.py --ip 192.168.4.167 --device-id old3ds-lan167)
printf '%s\n' '=== atm status ==='; "${ATM[@]}" status
printf '%s\n' '=== atm devmode ==='; "${ATM[@]}" devmode
printf '%s\n' '=== atm artifacts ==='; "${ATM[@]}" artifacts
printf '%s\n' '=== atm journal ==='; "${ATM[@]}" journal
```

### Exact mandatory live-check output

```text
=== atm status ===
{"error": "unreachable", "message": "http / failed: [Errno 111] Connection refused"}
=== atm devmode ===
{"error": "unreachable", "message": "http / failed: [Errno 111] Connection refused"}
=== atm artifacts ===
{"error": "unreachable", "message": "http /luma/atm/artifacts.txt failed: [Errno 111] Connection refused"}
=== atm journal ===
{"error": "unreachable", "message": "http /luma/atm/run_record_a.txt failed: [Errno 111] Connection refused"}
```

Combined exit code: `3` (atmctl unreachable/timeout class). No new performance measurement was produced. All timing claims remain constrained by `osGetTime` 1 ms granularity and bucket-estimated percentiles.

## Fresh iteration — 2026-11-04 13:40 UTC — live preflight blocked

- Repository state at start: branch `experimental/native-renderer`; existing modified/untracked files were observed and left untouched.
- Required docs read: in-repo ATM runbook `docs/atm-autonomous-agent-runbook.md` and linked notes `network-api-reference`, `deployment-and-launch`, `debugger-guide`, `profiling-and-telemetry`, `developer-mode`, `artifact-management`, and `troubleshooting`.
- Disk selection: Phase A remains incomplete. `perf/LOOP-DONE.md` and canonical `perf/BLOCKED.md` were absent at iteration start; the existing log marks the prior smoke as not harness-validating because it ended with `recovery=recovery_stopped` and `auto_dump_present=not_ok`.

### Exact mandatory live-check command

```sh
ATM=(python3 /mnt/nas/Development/netload_stub/tools/atmctl.py --ip 192.168.4.167 --device-id old3ds-lan167)
printf '%s\n' '=== atm status ==='; "${ATM[@]}" status
printf '%s\n' '=== atm devmode ==='; "${ATM[@]}" devmode
printf '%s\n' '=== atm artifacts ==='; "${ATM[@]}" artifacts
printf '%s\n' '=== atm journal ==='; "${ATM[@]}" journal
```

### Exact mandatory live-check output

```text
=== atm status ===
{"error": "unreachable", "message": "http / failed: [Errno 111] Connection refused"}
exit_code=3
=== atm devmode ===
{"error": "unreachable", "message": "http / failed: [Errno 111] Connection refused"}
exit_code=3
=== atm artifacts ===
{"error": "unreachable", "message": "http /luma/atm/artifacts.txt failed: [Errno 111] Connection refused"}
exit_code=3
=== atm journal ===
{"error": "unreachable", "message": "http /luma/atm/run_record_a.txt failed: [Errno 111] Connection refused"}
exit_code=3
```

- Combined exit code: `3` (`unreachable/timeout`). Because the console was unreachable, no build, deploy, workload, retry, reboot, cleanup, or acknowledgement was attempted. Exact evidence was written to `perf/BLOCKED.md`, and this iteration stopped there.
- No new performance measurement was produced. All timing claims remain constrained by `osGetTime` 1 ms granularity and bucket-estimated percentiles.

## Fresh iteration — 2026-11-04 15:47:28 UTC — mandatory live preflight blocked

### Start-of-iteration state

- Required docs read before the stop: `docs/atm-autonomous-agent-runbook.md` and linked notes `network-api-reference.md`, `deployment-and-launch.md`, `debugger-guide.md`, `profiling-and-telemetry.md`, `developer-mode.md`, `artifact-management.md`, and `troubleshooting.md` from `/mnt/nas/Development/netload_stub/`.
- `perf/LOOP-DONE.md` and canonical `perf/BLOCKED.md` were absent at iteration start; disk selection was therefore a fresh preflight.
- Exact `git status --short --branch` output:

```text
## experimental/native-renderer
 M .gitignore
 M AGENTS.md
 M docs/validation/ralph-pi-log.md
 M docs/validation/ralph-pi-spec.md
 M perf/OPTIMIZATION-LOG.md
?? perf/BLOCKED-20261104-132541.md
?? perf/BLOCKED-20261104-154611.md
?? perf/BLOCKED-ARCHIVED-20261104-152204.md
?? perf/BLOCKED-ARCHIVED-20261104-154318.md
?? perf/BLOCKED-OLD-TRIAGED-20261104.md
?? perf/BLOCKED-RECOVERY-STATE-20261104.md
?? perf/smoke-recovery-check-20261104/
?? tools/perf/ralph_loop.sh
?? tools/ralph-pi.sh
```

### Exact mandatory live-check command

```sh
ATM=(python3 /mnt/nas/Development/netload_stub/tools/atmctl.py --ip 192.168.4.167 --device-id old3ds-lan167)
printf '%s\n' '===== atm status ====='; "${ATM[@]}" status; printf '%s\n' "===== atm status exit: $? ====="
printf '%s\n' '===== atm devmode ====='; "${ATM[@]}" devmode; printf '%s\n' "===== atm devmode exit: $? ====="
printf '%s\n' '===== atm artifacts ====='; "${ATM[@]}" artifacts; printf '%s\n' "===== atm artifacts exit: $? ====="
printf '%s\n' '===== atm journal ====='; "${ATM[@]}" journal; printf '%s\n' "===== atm journal exit: $? ====="
```

### Exact mandatory live-check output

```text
===== atm status =====
{"error": "unreachable", "message": "http / failed: [Errno 111] Connection refused"}
===== atm status exit: 3 =====
===== atm devmode =====
{"error": "unreachable", "message": "http / failed: [Errno 111] Connection refused"}
===== atm devmode exit: 3 =====
===== atm artifacts =====
{"error": "unreachable", "message": "http /luma/atm/artifacts.txt failed: [Errno 111] Connection refused"}
===== atm artifacts exit: 3 =====
===== atm journal =====
{"error": "unreachable", "message": "http /luma/atm/run_record_a.txt failed: [Errno 111] Connection refused"}
===== atm journal exit: 3 =====
```

### Stop decision

- All four mandatory live checks refused the HTTP connection with ATM controller exit code `3` (`unreachable/timeout`). Live device, firmware, operational, recovery, artifact, and journal identity could not be verified.
- Per the task stop condition, this iteration stopped immediately. No build, deploy, run, retry, reboot, cleanup, acknowledgement, screenshot, GDB attachment, or source optimization was attempted.
- Exact evidence was written to canonical `perf/BLOCKED.md`. No new performance measurement was produced; `osGetTime` remains 1 ms granularity and percentiles remain bucket estimates.

## Fresh iteration — 2026-11-04 15:36 UTC — mandatory live preflight blocked

### Start-of-iteration state

- Disk selection: Phase A remains incomplete. `perf/OPTIMIZATION-LOG.md` was present; canonical `perf/BLOCKED.md` and `perf/LOOP-DONE.md` were absent at iteration start.
- Required docs read before any device action: `docs/atm-autonomous-agent-runbook.md` and linked notes `network-api-reference.md`, `deployment-and-launch.md`, `debugger-guide.md`, `profiling-and-telemetry.md`, `developer-mode.md`, `artifact-management.md`, and `troubleshooting.md` from `/mnt/nas/Development/netload_stub/`.
- Exact `git status --short --branch` output:

```text
## experimental/native-renderer
 M .gitignore
 M AGENTS.md
 M docs/validation/ralph-pi-log.md
 M docs/validation/ralph-pi-spec.md
 M perf/OPTIMIZATION-LOG.md
?? perf/BLOCKED-20261104-132541.md
?? perf/BLOCKED-ARCHIVED-20261104-152204.md
?? perf/BLOCKED-OLD-TRIAGED-20261104.md
?? perf/BLOCKED-RECOVERY-STATE-20261104.md
?? perf/smoke-recovery-check-20261104/
?? tools/perf/ralph_loop.sh
?? tools/ralph-pi.sh
```

### Exact mandatory live-check command

```sh
ATM=(python3 /mnt/nas/Development/netload_stub/tools/atmctl.py --ip 192.168.4.167 --device-id old3ds-lan167)
printf '%s\n' '=== atm status ==='; "${ATM[@]}" status; printf 'exit_code=%s\n' "$?"
printf '%s\n' '=== atm devmode ==='; "${ATM[@]}" devmode; printf 'exit_code=%s\n' "$?"
printf '%s\n' '=== atm artifacts ==='; "${ATM[@]}" artifacts; printf 'exit_code=%s\n' "$?"
printf '%s\n' '=== atm journal ==='; "${ATM[@]}" journal; printf 'exit_code=%s\n' "$?"
```

### Exact mandatory live-check output

```text
=== atm status ===
{"error": "unreachable", "message": "http / failed: [Errno 111] Connection refused"}
exit_code=3
=== atm devmode ===
{"error": "unreachable", "message": "http / failed: [Errno 111] Connection refused"}
exit_code=3
=== atm artifacts ===
{"error": "unreachable", "message": "http /luma/atm/artifacts.txt failed: [Errno 111] Connection refused"}
exit_code=3
=== atm journal ===
{"error": "unreachable", "message": "http /luma/atm/run_record_a.txt failed: [Errno 111] Connection refused"}
exit_code=3
```

### Stop decision

- Every mandatory live endpoint refused the HTTP connection, so live device identity, firmware identity, operational state, recovery state, artifact inventory, and journal state could not be verified.
- Per the task stop condition, this iteration stopped immediately. No build, deploy, run, workload, retry, reboot, cleanup, acknowledgement, screenshot, GDB attachment, or source optimization was attempted.
- Exact evidence was written to canonical `perf/BLOCKED.md`. No new performance measurement was produced; `osGetTime` remains 1 ms granularity and percentiles remain bucket estimates.

## Fresh iteration — 2026-11-04 16:06:46 UTC — mandatory live preflight blocked

### Start-of-iteration state

- Disk selection: Phase A remains incomplete. `perf/OPTIMIZATION-LOG.md` was present; canonical `perf/BLOCKED.md` and `perf/LOOP-DONE.md` were absent at iteration start.
- Required docs read before the device checks: `/mnt/nas/Development/netload_stub/docs/atm-autonomous-agent-runbook.md` and linked ATM notes `network-api-reference.md`, `deployment-and-launch.md`, `debugger-guide.md`, `profiling-and-telemetry.md`, `developer-mode.md`, `artifact-management.md`, and `troubleshooting.md`.
- Exact `git status --short --branch` output:

```text
## experimental/native-renderer
 M .gitignore
 M AGENTS.md
 M docs/validation/ralph-pi-log.md
 M docs/validation/ralph-pi-spec.md
 M perf/OPTIMIZATION-LOG.md
?? perf/AGENT-FAILED.md
?? perf/BLOCKED-20261104-132541.md
?? perf/BLOCKED-20261104-154611.md
?? perf/BLOCKED-ARCHIVED-20261104-152204.md
?? perf/BLOCKED-ARCHIVED-20261104-154318.md
?? perf/BLOCKED-ARCHIVED-20261104-155716.md
?? perf/BLOCKED-ARCHIVED-20261104-160423.md
?? perf/BLOCKED-OLD-TRIAGED-20261104.md
?? perf/BLOCKED-RECOVERY-STATE-20261104.md
?? perf/smoke-recovery-check-20261104/
?? tools/perf/ralph_loop.sh
?? tools/ralph-pi.sh
```

### Exact mandatory live-check command

```sh
ATM=(python3 /mnt/nas/Development/netload_stub/tools/atmctl.py --ip 192.168.4.167 --device-id old3ds-lan167)
printf '%s\n' '=== atm status ==='; "${ATM[@]}" status; printf 'exit_code=%s\n' "$?"
printf '%s\n' '=== atm devmode ==='; "${ATM[@]}" devmode; printf 'exit_code=%s\n' "$?"
printf '%s\n' '=== atm artifacts ==='; "${ATM[@]}" artifacts; printf 'exit_code=%s\n' "$?"
printf '%s\n' '=== atm journal ==='; "${ATM[@]}" journal; printf 'exit_code=%s\n' "$?"
```

### Exact mandatory live-check output

```text
=== atm status ===
{"error": "unreachable", "message": "http / failed: [Errno 111] Connection refused"}
exit_code=3
=== atm devmode ===
{"error": "unreachable", "message": "http / failed: [Errno 111] Connection refused"}
exit_code=3
=== atm artifacts ===
{"error": "unreachable", "message": "http /luma/atm/artifacts.txt failed: [Errno 111] Connection refused"}
exit_code=3
=== atm journal ===
{"error": "unreachable", "message": "http /luma/atm/run_record_a.txt failed: [Errno 111] Connection refused"}
exit_code=3
```

### Stop decision

- All four mandatory live checks refused the HTTP connection with controller exit code `3` (`unreachable/timeout`). Live device/firmware identity, operational/recovery readiness, artifacts, and journal could not be verified.
- Per the task stop condition, this iteration stopped immediately. No build, deploy, launch, run, retry, reboot, cleanup, acknowledgement, screenshot, GDB attachment, or source optimization was attempted.
- Exact evidence was written to canonical `perf/BLOCKED.md`. Prior crash/recovery evidence was preserved. No new performance measurement was produced; `osGetTime` remains 1 ms granularity and percentiles remain bucket estimates.

## Fresh iteration — 2026-11-04 16:17:11 UTC — mandatory live preflight blocked

### Start-of-iteration state

- Required docs read before any device action: `/mnt/nas/Development/netload_stub/docs/atm-autonomous-agent-runbook.md` and linked ATM notes `network-api-reference.md`, `deployment-and-launch.md`, `debugger-guide.md`, `profiling-and-telemetry.md`, `developer-mode.md`, `artifact-management.md`, and `troubleshooting.md`.
- Disk selection: Phase A remains incomplete; `perf/LOOP-DONE.md` and canonical `perf/BLOCKED.md` were absent at iteration start.
- Exact `git status --short --branch` output:

```text
## experimental/native-renderer
 M .gitignore
 M AGENTS.md
 M docs/validation/ralph-pi-log.md
 M docs/validation/ralph-pi-spec.md
 M perf/OPTIMIZATION-LOG.md
?? perf/AGENT-FAILED.md
?? perf/BLOCKED-20261104-132541.md
?? perf/BLOCKED-20261104-154611.md
?? perf/BLOCKED-ARCHIVED-20261104-152204.md
?? perf/BLOCKED-ARCHIVED-20261104-154318.md
?? perf/BLOCKED-ARCHIVED-20261104-155716.md
?? perf/BLOCKED-ARCHIVED-20261104-160423.md
?? perf/BLOCKED-ARCHIVED-20261104-160806.md
?? perf/BLOCKED-OLD-TRIAGED-20261104.md
?? perf/BLOCKED-RECOVERY-STATE-20261104.md
?? perf/smoke-recovery-check-20261104/
?? tools/perf/ralph_loop.sh
?? tools/ralph-pi.sh
```

### Exact mandatory live-check command

```sh
ATM=(python3 /mnt/nas/Development/netload_stub/tools/atmctl.py --ip 192.168.4.167 --device-id old3ds-lan167)
printf '%s\n' '=== atm status ==='; "${ATM[@]}" status; printf 'exit_code=%s\n' "$?"
printf '%s\n' '=== atm devmode ==='; "${ATM[@]}" devmode; printf 'exit_code=%s\n' "$?"
printf '%s\n' '=== atm artifacts ==='; "${ATM[@]}" artifacts; printf 'exit_code=%s\n' "$?"
printf '%s\n' '=== atm journal ==='; "${ATM[@]}" journal; printf 'exit_code=%s\n' "$?"
```

### Exact mandatory live-check output

```text
=== atm status ===
{"error": "unreachable", "message": "http / failed: [Errno 111] Connection refused"}
exit_code=3
=== atm devmode ===
{"error": "unreachable", "message": "http / failed: [Errno 111] Connection refused"}
exit_code=3
=== atm artifacts ===
{"error": "unreachable", "message": "http /luma/atm/artifacts.txt failed: [Errno 111] Connection refused"}
exit_code=3
=== atm journal ===
{"error": "unreachable", "message": "http /luma/atm/run_record_a.txt failed: [Errno 111] Connection refused"}
exit_code=3
```

### Stop decision

- All four mandatory live checks refused the HTTP connection with controller exit code `3` (`unreachable/timeout`). Live device/firmware identity, operational/recovery readiness, artifacts, and journal could not be verified.
- Per the task stop condition, this iteration stopped immediately. No build, deploy, launch, run, retry, reboot, cleanup, acknowledgement, screenshot, GDB attachment, or source optimization was attempted.
- Exact evidence was written to canonical `perf/BLOCKED.md`; prior crash/recovery evidence was preserved. No new performance measurement was produced; `osGetTime` remains 1 ms granularity and percentiles remain bucket estimates.

## Fresh iteration — 2026-11-04 17:01:01 UTC — mandatory live preflight blocked

### Start-of-iteration state

- Disk selection: Phase A remains incomplete. `perf/OPTIMIZATION-LOG.md` was present; canonical `perf/BLOCKED.md` and `perf/LOOP-DONE.md` were absent at iteration start.
- Required docs read before the stop: `/mnt/nas/Development/netload_stub/docs/atm-autonomous-agent-runbook.md` and linked ATM notes `network-api-reference.md`, `deployment-and-launch.md`, `debugger-guide.md`, `profiling-and-telemetry.md`, `developer-mode.md`, `artifact-management.md`, and `troubleshooting.md`.
- Exact `git status --short --branch` output at iteration start:

```text
## experimental/native-renderer
 M .gitignore
 M AGENTS.md
 M docs/validation/ralph-pi-log.md
 M docs/validation/ralph-pi-spec.md
 M perf/OPTIMIZATION-LOG.md
?? perf/BLOCKED-20261104-132541.md
?? perf/BLOCKED-20261104-154611.md
?? perf/BLOCKED-ARCHIVED-20261104-152204.md
?? perf/BLOCKED-ARCHIVED-20261104-154318.md
?? perf/BLOCKED-ARCHIVED-20261104-155716.md
?? perf/BLOCKED-ARCHIVED-20261104-160423.md
?? perf/BLOCKED-ARCHIVED-20261104-160806.md
?? perf/BLOCKED-ARCHIVED-20261104-165836.md
?? perf/BLOCKED-OLD-TRIAGED-20261104.md
?? perf/BLOCKED-RECOVERY-STATE-20261104.md
?? perf/smoke-recovery-check-20261104/
?? tools/perf/ralph_loop.sh
?? tools/ralph-pi.sh
```

### Exact mandatory live-check command

```sh
ATM=(python3 /mnt/nas/Development/netload_stub/tools/atmctl.py --ip 192.168.4.167 --device-id old3ds-lan167)
printf '%s\n' '=== atm status ==='; "${ATM[@]}" status; printf 'exit_code=%s\n' "$?"
printf '%s\n' '=== atm devmode ==='; "${ATM[@]}" devmode; printf 'exit_code=%s\n' "$?"
printf '%s\n' '=== atm artifacts ==='; "${ATM[@]}" artifacts; printf 'exit_code=%s\n' "$?"
printf '%s\n' '=== atm journal ==='; "${ATM[@]}" journal; printf 'exit_code=%s\n' "$?"
```

### Exact mandatory live-check output

```text
=== atm status ===
{"error": "unreachable", "message": "http / failed: [Errno 111] Connection refused"}
exit_code=3
=== atm devmode ===
{"error": "unreachable", "message": "http / failed: [Errno 111] Connection refused"}
exit_code=3
=== atm artifacts ===
{"error": "unreachable", "message": "http /luma/atm/artifacts.txt failed: [Errno 111] Connection refused"}
exit_code=3
=== atm journal ===
{"error": "unreachable", "message": "http /luma/atm/run_record_a.txt failed: [Errno 111] Connection refused"}
exit_code=3
```

### Stop decision

- All four mandatory live checks refused their HTTP connections with controller exit code `3` (`unreachable/timeout`). Live device/firmware identity, operational/recovery readiness, Developer Mode state, artifacts, and journal could not be verified.
- Per the task stop condition, this iteration stopped immediately. No build, deploy, launch, run, retry, reboot, cleanup, acknowledgement, screenshot, GDB attachment, or source optimization was attempted.
- Exact evidence was written to canonical `perf/BLOCKED.md`; prior crash/recovery evidence was preserved. No new performance measurement was produced; `osGetTime` remains 1 ms granularity and percentiles remain bucket estimates.

## Fresh iteration — 2026-11-04 17:11:59 UTC — mandatory live preflight blocked after retries

### Start-of-iteration state

- Disk selection: Phase A remains incomplete. `perf/OPTIMIZATION-LOG.md` was present; canonical `perf/BLOCKED.md` and `perf/LOOP-DONE.md` were absent at iteration start.
- Required docs read before the device checks: `/mnt/nas/Development/netload_stub/docs/atm-autonomous-agent-runbook.md` and linked ATM notes `network-api-reference.md`, `deployment-and-launch.md`, `debugger-guide.md`, `profiling-and-telemetry.md`, `developer-mode.md`, `artifact-management.md`, and `troubleshooting.md`.
- Exact `git status --short --branch` output at iteration start:

```text
## experimental/native-renderer
 M .gitignore
 M AGENTS.md
 M docs/validation/ralph-pi-log.md
 M docs/validation/ralph-pi-spec.md
 M perf/OPTIMIZATION-LOG.md
?? perf/BLOCKED-20261104-132541.md
?? perf/BLOCKED-20261104-154611.md
?? perf/BLOCKED-ARCHIVED-20261104-152204.md
?? perf/BLOCKED-ARCHIVED-20261104-154318.md
?? perf/BLOCKED-ARCHIVED-20261104-155716.md
?? perf/BLOCKED-ARCHIVED-20261104-160423.md
?? perf/BLOCKED-ARCHIVED-20261104-160806.md
?? perf/BLOCKED-ARCHIVED-20261104-165836.md
?? perf/BLOCKED-ARCHIVED-20261104-170431.md
?? perf/BLOCKED-OLD-TRIAGED-20261104.md
?? perf/BLOCKED-RECOVERY-STATE-20261104.md
?? perf/smoke-recovery-check-20261104/
?? tools/perf/ralph_loop.sh
?? tools/ralph-pi.sh
```

### Exact mandatory live-check command

```sh
ATM=(python3 /mnt/nas/Development/netload_stub/tools/atmctl.py --ip 192.168.4.167 --device-id old3ds-lan167)
printf '%s\n' '=== atm status ==='; "${ATM[@]}" status; printf 'exit_code=%s\n' "$?"
printf '%s\n' '=== atm devmode ==='; "${ATM[@]}" devmode; printf 'exit_code=%s\n' "$?"
printf '%s\n' '=== atm artifacts ==='; "${ATM[@]}" artifacts; printf 'exit_code=%s\n' "$?"
printf '%s\n' '=== atm journal ==='; "${ATM[@]}" journal; printf 'exit_code=%s\n' "$?"
```

### Exact retry command

```sh
ATM=(python3 /mnt/nas/Development/netload_stub/tools/atmctl.py --ip 192.168.4.167 --device-id old3ds-lan167)
for attempt in 2 3 4; do
  printf '%s\n' "=== retry attempt ${attempt} after 60s ==="
  sleep 60
  printf '%s\n' '=== atm status ==='; "${ATM[@]}" status; printf 'exit_code=%s\n' "$?"
  printf '%s\n' '=== atm devmode ==='; "${ATM[@]}" devmode; printf 'exit_code=%s\n' "$?"
  printf '%s\n' '=== atm artifacts ==='; "${ATM[@]}" artifacts; printf 'exit_code=%s\n' "$?"
  printf '%s\n' '=== atm journal ==='; "${ATM[@]}" journal; printf 'exit_code=%s\n' "$?"
done
```

### Exact mandatory live-check output

```text
=== atm status ===
{"error": "unreachable", "message": "http / failed: [Errno 111] Connection refused"}
exit_code=3
=== atm devmode ===
{"error": "unreachable", "message": "http / failed: [Errno 111] Connection refused"}
exit_code=3
=== atm artifacts ===
{"error": "unreachable", "message": "http /luma/atm/artifacts.txt failed: [Errno 111] Connection refused"}
exit_code=3
=== atm journal ===
{"error": "unreachable", "message": "http /luma/atm/run_record_a.txt failed: [Errno 111] Connection refused"}
exit_code=3
=== retry attempt 2 after 60s ===
=== atm status ===
{"error": "unreachable", "message": "http / failed: [Errno 111] Connection refused"}
exit_code=3
=== atm devmode ===
{"error": "unreachable", "message": "http / failed: [Errno 111] Connection refused"}
exit_code=3
=== atm artifacts ===
{"error": "unreachable", "message": "http /luma/atm/artifacts.txt failed: [Errno 111] Connection refused"}
exit_code=3
=== atm journal ===
{"error": "unreachable", "message": "http /luma/atm/run_record_a.txt failed: [Errno 111] Connection refused"}
exit_code=3
=== retry attempt 3 after 60s ===
=== atm status ===
{"error": "unreachable", "message": "http / failed: [Errno 111] Connection refused"}
exit_code=3
=== atm devmode ===
{"error": "unreachable", "message": "http / failed: [Errno 111] Connection refused"}
exit_code=3
=== atm artifacts ===
{"error": "unreachable", "message": "http /luma/atm/artifacts.txt failed: [Errno 111] Connection refused"}
exit_code=3
=== atm journal ===
{"error": "unreachable", "message": "http /luma/atm/run_record_a.txt failed: [Errno 111] Connection refused"}
exit_code=3
=== retry attempt 4 after 60s ===
=== atm status ===
{"error": "unreachable", "message": "http / failed: [Errno 111] Connection refused"}
exit_code=3
=== atm devmode ===
{"error": "unreachable", "message": "http / failed: [Errno 111] Connection refused"}
exit_code=3
=== atm artifacts ===
{"error": "unreachable", "message": "http /luma/atm/artifacts.txt failed: [Errno 111] Connection refused"}
exit_code=3
=== atm journal ===
{"error": "unreachable", "message": "http /luma/atm/run_record_a.txt failed: [Errno 111] Connection refused"}
exit_code=3
```

### Stop decision

- The initial preflight and all three allowed 60-second-spaced retries refused every ATM HTTP connection with controller exit code `3` (`unreachable/timeout`). Live device/firmware identity, operational/recovery readiness, Developer Mode state, artifacts, and journal could not be verified.
- All reachability retries were exhausted. Per the task stop condition, this iteration stopped and wrote exact evidence to canonical `perf/BLOCKED.md`. No build, deploy, launch, run, reboot, cleanup, acknowledgement, screenshot, GDB attachment, or source optimization was attempted; prior crash/recovery evidence was preserved.
- No new performance measurement was produced. `osGetTime` remains 1 ms granularity and reported percentiles remain bucket estimates.

## Fresh iteration — 2026-11-04 17:21:17 UTC — mandatory live preflight blocked (host-side egress rejection)

### Start-of-iteration state

- Disk selection: Phase A remains incomplete. `perf/OPTIMIZATION-LOG.md` was present; canonical `perf/BLOCKED.md` and `perf/LOOP-DONE.md` were absent at iteration start.
- Required docs read before the device checks: `/mnt/nas/Development/netload_stub/docs/atm-autonomous-agent-runbook.md`, `docs/atm/network-api-reference.md`, `docs/atm/deployment-and-launch.md`, `docs/atm/profiling-and-telemetry.md`, `docs/atm/troubleshooting.md`.
- Exact `git status --short --branch` output at iteration start:

```text
## experimental/native-renderer
 M .gitignore
 M AGENTS.md
 M docs/validation/ralph-pi-log.md
 M docs/validation/ralph-pi-spec.md
 M perf/OPTIMIZATION-LOG.md
?? perf/BLOCKED-20261104-132541.md
?? perf/BLOCKED-20261104-154611.md
?? perf/BLOCKED-ARCHIVED-20261104-152204.md
?? perf/BLOCKED-ARCHIVED-20261104-154318.md
?? perf/BLOCKED-ARCHIVED-20261104-155716.md
?? perf/BLOCKED-ARCHIVED-20261104-160423.md
?? perf/BLOCKED-ARCHIVED-20261104-160806.md
?? perf/BLOCKED-ARCHIVED-20261104-165836.md
?? perf/BLOCKED-ARCHIVED-20261104-170431.md
?? perf/BLOCKED-ARCHIVED-20261104-171556.md
?? perf/BLOCKED-OLD-TRIAGED-20261104.md
?? perf/BLOCKED-RECOVERY-STATE-20261104.md
?? perf/smoke-recovery-check-20261104/
?? tools/perf/ralph_loop.sh
?? tools/ralph-pi.sh
```

### Exact mandatory live-check command

```sh
ATM=(python3 /mnt/nas/Development/netload_stub/tools/atmctl.py --ip 192.168.4.167 --device-id old3ds-lan167)
printf '%s\n' '=== atm status ==='; "${ATM[@]}" status; printf 'exit_code=%s\n' "$?"
printf '%s\n' '=== atm devmode ==='; "${ATM[@]}" devmode; printf 'exit_code=%s\n' "$?"
printf '%s\n' '=== atm artifacts ==='; "${ATM[@]}" artifacts; printf 'exit_code=%s\n' "$?"
printf '%s\n' '=== atm journal ==='; "${ATM[@]}" journal; printf 'exit_code=%s\n' "$?"
```

### Exact mandatory live-check output

```text
=== atm status ===
{"error": "unreachable", "message": "http / failed: [Errno 111] Connection refused"}
exit_code=3
=== atm devmode ===
{"error": "unreachable", "message": "http / failed: [Errno 111] Connection refused"}
exit_code=3
=== atm artifacts ===
{"error": "unreachable", "message": "http /luma/atm/artifacts.txt failed: [Errno 111] Connection refused"}
exit_code=3
=== atm journal ===
{"error": "unreachable", "message": "http /luma/atm/run_record_a.txt failed: [Errno 111] Connection refused"}
exit_code=3
```

### Exact retry command (attempts 2/3/4, 60 s apart) and extra polls (attempts 5/6, 60 s apart)

```sh
ATM=(python3 /mnt/nas/Development/netload_stub/tools/atmctl.py --ip 192.168.4.167 --device-id old3ds-lan167)
for attempt in 2 3 4; do
  printf '%s\n' "=== retry attempt ${attempt} after 60s ==="
  sleep 60
  printf '%s\n' '=== atm status ==='; "${ATM[@]}" status; printf 'exit_code=%s\n' "$?"
  printf '%s\n' '=== atm devmode ==='; "${ATM[@]}" devmode; printf 'exit_code=%s\n' "$?"
  printf '%s\n' '=== atm artifacts ==='; "${ATM[@]}" artifacts; printf 'exit_code=%s\n' "$?"
  printf '%s\n' '=== atm journal ==='; "${ATM[@]}" journal; printf 'exit_code=%s\n' "$?"
done
```

Every poll (initial + 3 contract retries + 2 extra polls over ~5 minutes) returned the identical four `{"error": "unreachable", "message": "... Connection refused"}` lines with controller exit code `3`.

### Troubleshooting discriminator and host diagnostics (read-only)

Documented directed discovery probe (non-state-changing per `docs/atm/network-api-reference.md`):

```text
$ python3 /mnt/nas/Development/netload_stub/tools/atmctl.py --ip 192.168.4.167 --device-id old3ds-lan167 discover
{"error": "unreachable", "message": "discovery failed: [Errno 1] Operation not permitted"}
exit_code=3
```

Host egress matrix — every non-loopback TCP connect refused locally in 0.000 s, loopback TCP working:

```text
192.168.4.167:8080  ConnectionRefusedError 0.000s   (console HTTP)
192.168.4.167:17491 ConnectionRefusedError 0.000s   (console deploy)
192.168.4.167:4000  ConnectionRefusedError 0.000s   (console GDB)
192.168.4.167:4003  ConnectionRefusedError 0.000s   (console GDB break-on-start)
192.168.4.167:8081  ConnectionRefusedError 0.000s   (console telemetry)
192.168.4.167:8082  ConnectionRefusedError 0.000s   (console N3DL command)
192.168.4.1:80      ConnectionRefusedError 0.000s   (LAN router)
192.0.2.1:80        ConnectionRefusedError 0.000s   (TEST-NET-1, unroutable)
198.51.100.7:443    ConnectionRefusedError 0.000s
8.8.8.8:443         ConnectionRefusedError 0.000s
1.1.1.1:443         ConnectionRefusedError 0.000s
127.0.0.1:8731      CONNECTED 0.000s                (loopback TCP works)
127.0.0.1:111       CONNECTED 0.000s
127.0.0.53:53       CONNECTED 0.000s
ping 192.168.4.167  -> ping: sendmsg: Operation not permitted (3 packets, 100% loss)
ip neigh            -> 192.168.4.167 dev wlp6s0 lladdr a4:c0:e1:71:16:49 STALE
curl https://example.com -> http_code=000 exit=7 time=2.094076
curl http://192.168.4.167:8080/ -> http_code=000 exit=7 time=0.000118
ufw status verbose  -> ERROR: You need to be root to run this script
sudo -n true        -> sudo: a password is required
iptables -S         -> Permission denied (you must be root)
nft list ruleset    -> Operation not permitted (you must be root)
tailscale status    -> Logged out.
```

Interpretation: the refusal is generated locally on the agent host (a console that was powered off would time out, not refuse in 0 ms; TEST-NET and public addresses are refused identically), with outbound UDP at `EPERM` and ICMP `sendmsg: EPERM`. The agent runs unprivileged (`uid=994(piagent)`, `sudo -n` needs a password), so the blocking host rule cannot be inspected or lifted from inside this iteration, and changing host firewall state would be improvised cleanup outside the iteration contract. The console's own state is therefore unknown for this iteration.

### Stop decision

- Live device/firmware identity, operational/recovery readiness, Developer Mode state, artifacts, and journal could not be verified.
- No build, deploy, launch, workload run, reboot, cleanup, acknowledgement, screenshot, GDB attachment, or source optimization was attempted. Prior crash/recovery evidence was preserved.
- Exact evidence was written to canonical `perf/BLOCKED.md`; the immutable dated transcript is `perf/BLOCKED-20261104-172117.md`.
- No new performance measurement was produced. `osGetTime` remains 1 ms granularity and reported percentiles remain bucket estimates.
- Follow-up for the operator: restore LAN egress for the agent host (or use a host that can reach `192.168.4.167:8080`), then re-verify live identity and re-run the Phase-A smoke from scratch, because the firmware identity changed since the earlier log (`22c4bb74bc64+dirty`, boot_count 51 → `d303f0764adc+dirty`, boot_count 54) and the prior smoke ended with `recovery=recovery_stopped` / `auto_dump_present=not_ok`.

## Fresh iteration — 2026-11-04 19:11:42 UTC — mandatory live preflight blocked after retries

### Start-of-iteration state

- Disk selection: Phase A remains incomplete. `perf/OPTIMIZATION-LOG.md` was present; canonical `perf/BLOCKED.md` and `perf/LOOP-DONE.md` were absent at iteration start.
- Required docs read before the device checks: `/mnt/nas/Development/netload_stub/docs/atm-autonomous-agent-runbook.md` and linked ATM notes `network-api-reference.md`, `deployment-and-launch.md`, `debugger-guide.md`, `profiling-and-telemetry.md`, `developer-mode.md`, `artifact-management.md`, and `troubleshooting.md`.
- Exact `git status --short --branch` output at iteration start:

```text
## experimental/native-renderer
 M .gitignore
 M AGENTS.md
 M docs/validation/ralph-pi-log.md
 M docs/validation/ralph-pi-spec.md
 M perf/OPTIMIZATION-LOG.md
?? perf/AGENT-FAILED.md
?? perf/BLOCKED-20261104-132541.md
?? perf/BLOCKED-20261104-154611.md
?? perf/BLOCKED-20261104-172117.md
?? perf/BLOCKED-20261104-174455.md
?? perf/BLOCKED-ARCHIVED-20261104-152204.md
?? perf/BLOCKED-ARCHIVED-20261104-154318.md
?? perf/BLOCKED-ARCHIVED-20261104-155716.md
?? perf/BLOCKED-ARCHIVED-20261104-160423.md
?? perf/BLOCKED-ARCHIVED-20261104-160806.md
?? perf/BLOCKED-ARCHIVED-20261104-165836.md
?? perf/BLOCKED-ARCHIVED-20261104-170431.md
?? perf/BLOCKED-ARCHIVED-20261104-171556.md
?? perf/BLOCKED-ARCHIVED-20261104-173723.md
?? perf/BLOCKED-ARCHIVED-20261104-174926.md
?? perf/BLOCKED-OLD-TRIAGED-20261104.md
?? perf/BLOCKED-RECOVERY-STATE-20261104.md
?? perf/smoke-recovery-check-20261104/
?? tools/perf/ralph_loop.sh
?? tools/ralph-pi.sh
```

### Exact mandatory live-check command

```sh
ATM=(python3 /mnt/nas/Development/netload_stub/tools/atmctl.py --ip 192.168.4.167 --device-id old3ds-lan167)
"${ATM[@]}" status
printf '\n---DEVMODE---\n'
"${ATM[@]}" devmode
printf '\n---ARTIFACTS---\n'
"${ATM[@]}" artifacts
printf '\n---JOURNAL---\n'
"${ATM[@]}" journal
```

### Exact initial mandatory live-check output

```text
{"error": "unreachable", "message": "http / failed: [Errno 111] Connection refused"}

---DEVMODE---
{"error": "unreachable", "message": "http / failed: [Errno 111] Connection refused"}

---ARTIFACTS---
{"error": "unreachable", "message": "http /luma/atm/artifacts.txt failed: [Errno 111] Connection refused"}

---JOURNAL---
{"error": "unreachable", "message": "http /luma/atm/run_record_a.txt failed: [Errno 111] Connection refused"}
```

The combined initial preflight exited with controller exit code `3` (`unreachable/timeout`).

### Exact retry command and output

Three mandatory retries were performed 60 seconds apart, followed by two status-only reachability polls 60 seconds apart:

```sh
ATM=(python3 /mnt/nas/Development/netload_stub/tools/atmctl.py --ip 192.168.4.167 --device-id old3ds-lan167)
for attempt in 2 3 4; do
  printf '%s\n' "=== retry attempt ${attempt} after 60s ==="
  sleep 60
  printf '%s\n' '=== atm status ==='; "${ATM[@]}" status; printf 'exit_code=%s\n' "$?"
  printf '%s\n' '=== atm devmode ==='; "${ATM[@]}" devmode; printf 'exit_code=%s\n' "$?"
  printf '%s\n' '=== atm artifacts ==='; "${ATM[@]}" artifacts; printf 'exit_code=%s\n' "$?"
  printf '%s\n' '=== atm journal ==='; "${ATM[@]}" journal; printf 'exit_code=%s\n' "$?"
done
for attempt in 5 6; do
  printf '%s\n' "=== reachability poll ${attempt} after 60s ==="
  sleep 60
  printf '%s\n' '=== atm status ==='; "${ATM[@]}" status; printf 'exit_code=%s\n' "$?"
done
```

```text
=== retry attempt 2 after 60s ===
=== atm status ===
{"error": "unreachable", "message": "http / failed: [Errno 111] Connection refused"}
exit_code=3
=== atm devmode ===
{"error": "unreachable", "message": "http / failed: [Errno 111] Connection refused"}
exit_code=3
=== atm artifacts ===
{"error": "unreachable", "message": "http /luma/atm/artifacts.txt failed: [Errno 111] Connection refused"}
exit_code=3
=== atm journal ===
{"error": "unreachable", "message": "http /luma/atm/run_record_a.txt failed: [Errno 111] Connection refused"}
exit_code=3
=== retry attempt 3 after 60s ===
=== atm status ===
{"error": "unreachable", "message": "http / failed: [Errno 111] Connection refused"}
exit_code=3
=== atm devmode ===
{"error": "unreachable", "message": "http / failed: [Errno 111] Connection refused"}
exit_code=3
=== atm artifacts ===
{"error": "unreachable", "message": "http /luma/atm/artifacts.txt failed: [Errno 111] Connection refused"}
exit_code=3
=== atm journal ===
{"error": "unreachable", "message": "http /luma/atm/run_record_a.txt failed: [Errno 111] Connection refused"}
exit_code=3
=== retry attempt 4 after 60s ===
=== atm status ===
{"error": "unreachable", "message": "http / failed: [Errno 111] Connection refused"}
exit_code=3
=== atm devmode ===
{"error": "unreachable", "message": "http / failed: [Errno 111] Connection refused"}
exit_code=3
=== atm artifacts ===
{"error": "unreachable", "message": "http /luma/atm/artifacts.txt failed: [Errno 111] Connection refused"}
exit_code=3
=== atm journal ===
{"error": "unreachable", "message": "http /luma/atm/run_record_a.txt failed: [Errno 111] Connection refused"}
exit_code=3
=== reachability poll 5 after 60s ===
=== atm status ===
{"error": "unreachable", "message": "http / failed: [Errno 111] Connection refused"}
exit_code=3
=== reachability poll 6 after 60s ===
=== atm status ===
{"error": "unreachable", "message": "http / failed: [Errno 111] Connection refused"}
exit_code=3
```

### Stop decision

- All contract-allowed retries were exhausted; every mandatory endpoint returned `unreachable` / `Connection refused` with controller exit code `3`.
- Live device/firmware identity, operational/recovery readiness, Developer Mode state, artifacts, and journal could not be verified.
- No build, deploy, launch, run, reboot, cleanup, acknowledgement, screenshot, GDB attachment, or source optimization was attempted. Prior crash/recovery evidence was preserved.
- Exact evidence was written to canonical `perf/BLOCKED.md`. No new performance measurement was produced; `osGetTime` remains 1 ms granularity and percentiles remain bucket estimates.

## Fresh iteration — 2026-11-04 19:25:39 UTC — mandatory live preflight blocked (host-side egress rejection, retry exhausted)

### Start-of-iteration state

- Disk selection: Phase A remains incomplete. `perf/OPTIMIZATION-LOG.md` was present; canonical `perf/BLOCKED.md` and `perf/LOOP-DONE.md` were absent at iteration start (the 19:11 blocker had been archived as `perf/BLOCKED-ARCHIVED-20261104-191826.md` and explicitly cleared by the operator at 19:18:26 UTC).
- Required docs read before any device action: `/mnt/nas/Development/netload_stub/docs/atm-autonomous-agent-runbook.md` plus linked notes `network-api-reference.md`, `deployment-and-launch.md`, `debugger-guide.md`, `profiling-and-telemetry.md`, `developer-mode.md`, `artifact-management.md`, and `troubleshooting.md`.
- Exact `git status --short --branch` output at iteration start: branch `experimental/native-renderer` with the pre-existing modified/untracked set (`.gitignore`, `AGENTS.md`, `docs/validation/ralph-pi-log.md`, `docs/validation/ralph-pi-spec.md`, `perf/OPTIMIZATION-LOG.md` modified; `perf/BLOCKED-*`, `perf/AGENT-FAILED.md`, `perf/smoke-recovery-check-20261104/`, `tools/perf/ralph_loop.sh`, `tools/ralph-pi.sh` untracked). None were altered.

### Exact mandatory live-check command

```sh
ATM=(python3 /mnt/nas/Development/netload_stub/tools/atmctl.py --ip 192.168.4.167 --device-id old3ds-lan167)
printf '%s\n' '=== atm status ==='; "${ATM[@]}" status; printf 'exit_code=%s\n' "$?"
printf '%s\n' '=== atm devmode ==='; "${ATM[@]}" devmode; printf 'exit_code=%s\n' "$?"
printf '%s\n' '=== atm artifacts ==='; "${ATM[@]}" artifacts; printf 'exit_code=%s\n' "$?"
printf '%s\n' '=== atm journal ==='; "${ATM[@]}" journal; printf 'exit_code=%s\n' "$?"
```

### Exact initial mandatory live-check output

```text
=== atm status ===
{"error": "unreachable", "message": "http / failed: [Errno 111] Connection refused"}
exit_code=3
=== atm devmode ===
{"error": "unreachable", "message": "http / failed: [Errno 111] Connection refused"}
exit_code=3
=== atm artifacts ===
{"error": "unreachable", "message": "http /luma/atm/artifacts.txt failed: [Errno 111] Connection refused"}
exit_code=3
=== atm journal ===
{"error": "unreachable", "message": "http /luma/atm/run_record_a.txt failed: [Errno 111] Connection refused"}
exit_code=3
```

### Exact retry command and output

Three mandatory retries 60 s apart (all four commands each), then two additional 60 s reachability polls (status + devmode), covering ~5 minutes:

```sh
ATM=(python3 /mnt/nas/Development/netload_stub/tools/atmctl.py --ip 192.168.4.167 --device-id old3ds-lan167)
for attempt in 2 3 4; do
  printf '%s\n' "=== retry attempt ${attempt} after 60s ==="
  sleep 60
  printf '%s\n' '=== atm status ==='; "${ATM[@]}" status; printf 'exit_code=%s\n' "$?"
  printf '%s\n' '=== atm devmode ==='; "${ATM[@]}" devmode; printf 'exit_code=%s\n' "$?"
  printf '%s\n' '=== atm artifacts ==='; "${ATM[@]}" artifacts; printf 'exit_code=%s\n' "$?"
  printf '%s\n' '=== atm journal ==='; "${ATM[@]}" journal; printf 'exit_code=%s\n' "$?"
done
for attempt in 5 6; do
  printf '%s\n' "=== reachability poll ${attempt} after 60s ==="
  sleep 60
  printf '%s\n' '=== atm status ==='; "${ATM[@]}" status; printf 'exit_code=%s\n' "$?"
  printf '%s\n' '=== atm devmode ==='; "${ATM[@]}" devmode; printf 'exit_code=%s\n' "$?"
done
```

```text
=== retry attempt 2 after 60s ===
=== atm status ===
{"error": "unreachable", "message": "http / failed: [Errno 111] Connection refused"}
exit_code=3
=== atm devmode ===
{"error": "unreachable", "message": "http / failed: [Errno 111] Connection refused"}
exit_code=3
=== atm artifacts ===
{"error": "unreachable", "message": "http /luma/atm/artifacts.txt failed: [Errno 111] Connection refused"}
exit_code=3
=== atm journal ===
{"error": "unreachable", "message": "http /luma/atm/run_record_a.txt failed: [Errno 111] Connection refused"}
exit_code=3
=== retry attempt 3 after 60s ===
=== atm status ===
{"error": "unreachable", "message": "http / failed: [Errno 111] Connection refused"}
exit_code=3
=== atm devmode ===
{"error": "unreachable", "message": "http / failed: [Errno 111] Connection refused"}
exit_code=3
=== atm artifacts ===
{"error": "unreachable", "message": "http /luma/atm/artifacts.txt failed: [Errno 111] Connection refused"}
exit_code=3
=== atm journal ===
{"error": "unreachable", "message": "http /luma/atm/run_record_a.txt failed: [Errno 111] Connection refused"}
exit_code=3
=== retry attempt 4 after 60s ===
=== atm status ===
{"error": "unreachable", "message": "http / failed: [Errno 111] Connection refused"}
exit_code=3
=== atm devmode ===
{"error": "unreachable", "message": "http / failed: [Errno 111] Connection refused"}
exit_code=3
=== atm artifacts ===
{"error": "unreachable", "message": "http /luma/atm/artifacts.txt failed: [Errno 111] Connection refused"}
exit_code=3
=== atm journal ===
{"error": "unreachable", "message": "http /luma/atm/run_record_a.txt failed: [Errno 111] Connection refused"}
exit_code=3
=== reachability poll 5 after 60s ===
=== atm status ===
{"error": "unreachable", "message": "http / failed: [Errno 111] Connection refused"}
exit_code=3
=== atm devmode ===
{"error": "unreachable", "message": "http / failed: [Errno 111] Connection refused"}
exit_code=3
=== reachability poll 6 after 60s ===
=== atm status ===
{"error": "unreachable", "message": "http / failed: [Errno 111] Connection refused"}
exit_code=3
=== atm devmode ===
{"error": "unreachable", "message": "http / failed: [Errno 111] Connection refused"}
exit_code=3
=== retry loop complete ===
```

### Host-side discriminator (read-only)

```text
$ python3 .../atmctl.py --ip 192.168.4.167 --device-id old3ds-lan167 discover
{"error": "unreachable", "message": "discovery failed: [Errno 1] Operation not permitted"}
exit_code=3

192.168.4.167:8080  ConnectionRefusedError: [Errno 111] Connection refused 0.000s   (console HTTP)
192.168.4.167:17491 ConnectionRefusedError: [Errno 111] Connection refused 0.000s   (console deploy)
192.168.4.167:4000  ConnectionRefusedError: [Errno 111] Connection refused 0.000s   (console GDB)
192.168.4.1:80      ConnectionRefusedError: [Errno 111] Connection refused 0.000s   (LAN router)
198.51.100.7:443    ConnectionRefusedError: [Errno 111] Connection refused 0.000s   (TEST-NET-2)
8.8.8.8:443         ConnectionRefusedError: [Errno 111] Connection refused 0.000s
1.1.1.1:443         ConnectionRefusedError: [Errno 111] Connection refused 0.000s
127.0.0.1:8731      CONNECTED 0.000s                (loopback TCP works)
UDP 17491 send      PermissionError: [Errno 1] Operation not permitted
ping 192.168.4.167  1 packets transmitted, 0 received, +1 errors, 100% packet loss, time 0ms
ip neigh            192.168.4.167 dev wlp6s0 lladdr a4:c0:e1:71:16:49 STALE
curl http://192.168.4.167:8080/  http_code=000 exit=7 time=0.000092
id                  uid=994(piagent) gid=996(piagent) groups=996(piagent),1001(development)
sudo -n true / iptables -S / nft list ruleset -> sudo: a password is required
```

Every non-loopback TCP connect (console, router, TEST-NET, public) is refused in 0.000 s and outbound UDP is `EPERM`: the refusal is produced locally by the agent host's sandbox, not by the console. A powered-off console would time out or yield `EHOSTUNREACH`, and the console cannot refuse a connect to `8.8.8.8`. The agent is unprivileged, so the blocking rule cannot be inspected or lifted from inside the iteration.

Console-side state is unknown from this context; the operator's 19:18:26 UTC clearing record (archived blocker) states `atmctl status` returned `device=old3ds-lan167`, `operational=yes`, `recovery_ready=yes`, `recovery=none`, `boot_count=54`, devmode active, phase `listening` ~13 minutes before this iteration started, from a different host context.

### Stop decision

- All contract-allowed retries and polls exhausted; every mandatory endpoint returned `unreachable` / `Connection refused` with controller exit code `3`.
- No build, deploy, launch, workload run, screenshot, GDB attachment, reboot, cleanup, artifact acknowledgement, or source optimization was attempted. Prior crash/recovery evidence was preserved.
- Exact evidence written to canonical `perf/BLOCKED.md`; this iteration's log entry is banked here. No new performance measurement was produced; `osGetTime` remains 1 ms granularity and reported percentiles remain bucket estimates.
- Read-only egress watcher left running for the next iteration: `/tmp/atm_egress_watch_20261104.sh` polls `atmctl status` every 60 s and writes `perf/EGRESS-RECOVERED-<UTC>.md` with the first successful status JSON.

## Fresh iteration — 2026-11-04 21:07 UTC — **Phase A RE-VALIDATED on hardware (first green preflight in 12 iterations)**

### Start-of-iteration state and phase selection

- Disk selection: `perf/LOOP-DONE.md` absent, canonical `perf/BLOCKED.md` absent (the 19:25 blocker had been archived as `perf/BLOCKED-ARCHIVED-20261104-205520.md`). The only prior log entry for Phase A recorded a smoke that did **not** validate the harness (ended `recovery=recovery_stopped`, `auto_dump_present=not_ok`) and was taken on firmware `22c4bb74bc64+dirty`. **Phase A was therefore still the first incomplete phase.**
- Required docs read before any device action: Obsidian runbook `/mnt/nas/Vaults/port-docs/10-projects/netload_stub/atm-autonomous-agent-runbook.md` plus its linked notes `atm/network-api-reference.md`, `atm/deployment-and-launch.md`, `atm/debugger-guide.md`, `atm/profiling-and-telemetry.md`, `atm/developer-mode.md`, `atm/artifact-management.md`, `atm/troubleshooting.md`.
- Exact `git status --short --branch` at iteration start: branch `experimental/native-renderer`; pre-existing modified `.gitignore`, `AGENTS.md`, `docs/validation/ralph-pi-log.md`, `docs/validation/ralph-pi-spec.md`, `perf/OPTIMIZATION-LOG.md`; untracked `perf/BLOCKED-*`, `perf/AGENT-FAILED.md`, `perf/EGRESS-RECOVERED-*`, `perf/smoke-recovery-check-20261104/`, `tools/perf/ralph_loop.sh`, `tools/ralph-pi.sh`. None altered by this iteration except `perf/OPTIMIZATION-LOG.md` and the new `perf/smoke-20261104-d303f07-1/` + `perf/preflight-20261104-210514.txt` evidence.
- The read-only egress watcher left by the previous iteration produced `perf/EGRESS-RECOVERED-20261104-205256.md`; host egress to the console had recovered, so no retry ladder was needed.

### Exact mandatory live-check command

```sh
ATM=(python3 /mnt/nas/Development/netload_stub/tools/atmctl.py --ip 192.168.4.167 --device-id old3ds-lan167)
printf '%s\n' '=== atm status ==='; "${ATM[@]}" status; printf 'exit_code=%s\n' "$?"
printf '%s\n' '=== atm devmode ==='; "${ATM[@]}" devmode; printf 'exit_code=%s\n' "$?"
printf '%s\n' '=== atm artifacts ==='; "${ATM[@]}" artifacts; printf 'exit_code=%s\n' "$?"
printf '%s\n' '=== atm journal ==='; "${ATM[@]}" journal; printf 'exit_code=%s\n' "$?"
```

### Exact mandatory live-check output (summary; full text banked in `perf/preflight-20261104-210514.txt`)

All four commands returned **exit_code=0** on the first attempt — no retry ladder required.

```text
device=old3ds-lan167  firmware_id=d303f0764adc+dirty  session=7fc8d943
boot_kind=normal  boot_count=54  boot_count_state=ok
recovery=none  recovery_ready=yes  operational=yes  enabled=yes
auth_deployment=yes  auth_remote_control=yes
devmode_armed=yes  devmode_active=yes
svc_atm=active:devmode  svc_gdb=active:devmode  svc_wifi=active:atm
lid=closed_blanked  phase=listening  state=listening  busy=no
run_id=none  failure=none  last_error=none
auto_dump_present=not_ok  auto_dump_size=0
artifacts: 9 dumps (arm11 00000030-00000037, arm9 00000000) + status/errdisp/records/log/fwupdate + 30 staged payloads + 11 assets
journal (pre-run): seq=3 run-7f18f611-4 state=reboot_pending failure=remote_reboot_requested firmware_id=22c4bb74bc64+dirty
```

### Firmware change since the old Phase A entry (material for every later comparison)

The console was reflashed and rebooted between iterations. `/luma/atm/fwupdate.txt` reads `installed_sha256=c26c7cb09592159c667575be73d5f50619b11ed7cfe2fc86b4c7a9e3c3f05dd1 size=431616 source=boot-c26c7cb09592.firm`, and the pre-run journal slot still carried the `remote_reboot_requested` marker from session `7f18f611` while the running firmware was then `22c4bb74bc64+dirty`. The boot now reports `firmware_id=d303f0764adc+dirty`, `boot_kind=normal`, `boot_count=54`, `recovery=none`. `d303f07` is the netload_stub commit "Docs: netload_stub identity, agent onboarding, accuracy fixes", i.e. it includes `22c4bb7`/`e6dbe7d` "crash-safe redeploy, reboot-aware waits, send-tick fix". **All Phase B/C/D numbers must be compared only against `d303f0764adc+dirty` baselines; the 2026-11-03 smoke numbers are not comparable.**

### Resolution of the old Phase A contradiction

`auto_dump_present=not_ok` is **not** a fault here. Source `sysmodules/rosalina/source/atm/atm_poc.c:1556-1575` (`atm_check_auto_dump`) sets `not_ok` whenever `/luma/dumps/auto_dump_status.txt` does not read `status=ok`; the retrieved file reads `status=consumed`, i.e. the previous verified dump was already acknowledged and consumed. `recovery` is `none` and `recovery_ready=yes`, so the earlier `recovery_stopped` condition is gone.

### Phase A smoke run — PASS

```sh
ATM=(python3 /mnt/nas/Development/netload_stub/tools/atmctl.py --ip 192.168.4.167 --device-id old3ds-lan167)
"${ATM[@]}" run /mnt/nas/Development/netload_stub/atm-deploy/workloads/atm_workload_ok.3dsx \
  ./perf/smoke-20261104-d303f07-1 --build-id smoke-20261104-d303f07-1 --scenario atm-workload-ok
```

Controller exit code `0`. Fresh output directory used instead of the task's `./perf/smoke` because that directory holds immutable 2026-11-03 evidence from the superseded firmware; the deviation is deliberate and preserves prior evidence.

Stage evidence (all from the `atm run` JSON, banked at `perf/smoke-20261104-d303f07-1.run.json`):

| Stage | Evidence |
|---|---|
| `ready_before` | `phase=listening`, `session=7fc8d943` |
| `deploy` | `atm_workload_ok.3dsx`, 103968 bytes, SHA-256 `f681d0b02a162c8d7b1a7d223d0febeaf574ca55a8a25f49305ca27c43a17a8`, `build_id=smoke-20261104-d303f07-1`, `scenario=atm-workload-ok`, `no_launch=true`, responses `[0,0]` |
| `verify_staged` | `run-7fc8d943-1`, `state=staged`, `hash_check=expected`, `device_id=old3ds-lan167`, `firmware_id=d303f0764adc+dirty`, `failure=none` |
| `running` | `state=running`, `launch_result=0x00000000`, `failure=none` — the 2026-11-03 `launch_never_observed` hardware limitation did **not** reproduce |
| `terminal_journal` | `state=exited`, `launch_result=0x00000000`, `failure=none`, `exception_path=none`, `crash_class` empty |
| `retrieve` | 17 artifacts `retrieved_verified`, incl. `/3ds/atm_result.txt` (198 B, SHA-256 `baf5e4a8514d2ed79b6d733eb4afc88824765feb7ce8d101bada38100f5a85bf`) and `/3ds/atm_workload_last.txt` (39 B, `76389837893e2971bf9167c773819e6c89dbc3ac5b2771e24048e09024c5c138`); the 9 pre-existing crash dumps retrieved and acked through atmctl's verified flow |
| outcome | **`COMPLETED`**, `run_id=run-7fc8d943-1`, `verified_artifacts=17` |

Workload's own execution evidence (`perf/smoke-20261104-d303f07-1/atm_result.txt`):

```text
workload=atm_workload_ok
status=complete
frames=240 duration_ms=3695
checksum=94ee3dc5
@n3dl frames=240 avg_us=15395 p50_us=17000 p95_us=19000 p99_us=22000 max_us=21000 over_budget=108 min_us=12000
```

`atm_workload_last.txt` reads `workload=atm_workload_ok phase=started`, proving the intended process ran.

Post-run state: `device=old3ds-lan167`, `firmware_id=d303f0764adc+dirty`, `session=7fc8d943` (unchanged — no reboot), `boot_count=54` (unchanged — no crash), `recovery=none`, `recovery_ready=yes`, `operational=yes`, `phase=listening`, `state=listening`, `busy=no`, `failure=none`, `last_error=none`, journal `state=exited`. The console was left idle and listening; no reboot, cleanup, or manual intervention was needed.

**Timing honesty:** these are the harness workload's own sleep-paced numbers. `osGetTime` granularity is 1 ms and the reported percentiles are bucket-edge estimates. This is **not** a Forsaken performance result and says nothing about CPU/GPU utilization or memory headroom. For reference, the superseded 2026-11-03 run on the older firmware reported `avg_us=15791 p50=18000 p95=19000 p99=22000 max=22000 over_budget=125`; the delta is across firmware versions and is explicitly **not** a measured improvement.

### Phase A gate: PASS — Phase B is the next phase

Harness validated end-to-end on the physical Old 3DS: identity-validated status, deploy with device-computed SHA-256 matching expectation, journal-confirmed staging, journal-confirmed launch and clean exit, and a verified retrieval manifest containing the workload result file. Next iteration: **Phase B** — link `atm_core.c`/`atm_core.h` from `/mnt/nas/Development/netload_stub/sysmodules/rosalina/` into a profiling-only Forsaken build, call `atm_frame_add()` once per frame, add a fixed deterministic benchmark behind profiling flags, and write `atm_frame_format_summary()` to `/3ds/atm_result.txt`.

### Housekeeping notes for the next iteration

- SD staged-payload occupancy is growing: `atm artifacts` lists 30 `kind=staged` payloads (~7 MB total, incl. a stale 3 533 252-byte `/3ds/forsaken3ds.3dsx` from 2026-11-03) and 11 `.firm` assets. Capacity is **not** currently blocking (this deploy passed the 34 MiB reserve check). Per `AGENTS.md`, cleanup is `atm del <basename>` proven by listing absence, never the running/current staged build — the device refuses the currently staged launchable `.3dsx`, so a replacement must be staged first. Left untouched this iteration to avoid deleting anything the operator may still want.
- `perf/AGENT-FAILED.md` (exit 143 from the 19:00 ralph wrapper) is stale evidence from the egress-outage window; left in place.
- No commits or pushes were made. All changes remain uncommitted for operator review.

---

## Fresh iteration — 2026-11-04 21:10 UTC — **Phase B implemented; loop parked on a hardware-identity contradiction**

### Start-of-iteration state and phase selection

- Disk selection: `perf/LOOP-DONE.md` absent, canonical `perf/BLOCKED.md` absent. Phase A was re-validated green in the previous entry on firmware `d303f0764adc+dirty`. **Phase B was the first incomplete phase.**
- Exact `git status --short --branch` at start: branch `experimental/native-renderer`; modified `.gitignore`, `AGENTS.md`, `docs/validation/ralph-pi-log.md`, `docs/validation/ralph-pi-spec.md`, `perf/OPTIMIZATION-LOG.md`; untracked `perf/BLOCKED-*`, `perf/AGENT-FAILED.md`, `perf/EGRESS-RECOVERED-*`, `perf/preflight-20261104-210514.txt`, `perf/smoke-20261104-d303f07-1*`, `tools/perf/ralph_loop.sh`, `tools/ralph-pi.sh`.
- Required docs read before any device action: the Obsidian runbook plus `atm/network-api-reference.md`, `atm/deployment-and-launch.md`, `atm/profiling-and-telemetry.md`, `atm/debugger-guide.md`, `atm/developer-mode.md`, `atm/artifact-management.md`, `atm/troubleshooting.md`, and the shared core header `sysmodules/rosalina/include/atm/atm_core.h`.

### Mandatory live preflight — all four commands green on the first attempt

```sh
ATM=(python3 /mnt/nas/Development/netload_stub/tools/atmctl.py --ip 192.168.4.167 --device-id old3ds-lan167)
for c in status devmode artifacts journal; do "ATM[@]" $c; done   # full text: perf/preflight-20261104-211043.txt
```

```text
device=old3ds-lan167 firmware_id=d303f0764adc+dirty session=7fc8d943
boot_kind=normal boot_count=54 boot_count_state=ok recovery=none recovery_ready=yes
operational=yes enabled=yes devmode_armed=yes devmode_active=yes
svc_atm=active:devmode svc_gdb=active:devmode svc_wifi=active:atm
lid=closed_blanked phase=listening state=listening busy=no failure=none last_error=none
artifacts: 9 dumps + status/errdisp/records/log/fwupdate + 30 staged + 11 assets
journal (pre-run): seq=4 run-7fc8d943-1 state=exited build_id=smoke-20261104-d303f07-1 failure=none
```

No retry ladder was needed. `firmware_id` is unchanged from the Phase A entry, so Phase A's validation still applies.

### Phase B — what was built

One instrumentation system, not two. The tree already had `perf_capture.h` (stage gates → `sdmc:/forsaken_perf.txt`) and `fidelity.h` (per-tick state hash); Phase B adds the ATM producer as a third compile-gated module on the same discipline, and deliberately does **not** enable `PERF_CAPTURE` in the measurement build so the ATM frame numbers are not perturbed by ~60 extra stage gates per frame.

New files:

- **`atm_prof.h`** — public surface, fully no-op unless `-DATM_PROFILING`. Declares the frame hooks, the benchmark driver, and a bounded app-counter API (`enum`-indexed fixed array, integer adds only).
- **`atm_prof_3ds.c`** — the producer and the deterministic benchmark state machine (`IDLE → WAIT_LEVEL → ARMED → WARMUP → MEASURE → DONE`).

Modified files (every change is inside a profiling gate):

| File | Change |
|---|---|
| `Makefile.3ds` | `ATM_PROFILING` / `ATM_CORE_DIR` / `ATM_BENCH_{LEVEL,FRAMES,WARMUP,STEREO}` knobs; explicit out-of-tree rule for `$(BUILD)/atm_core.o`; new knobs added to `STAMP_CONTENT` so a flag flip forces a full rebuild |
| `main.c` | `atm_prof_frame_begin()` / `atm_prof_frame_end()` around the existing `PERF_FRAME_*` span; `atm_prof_write_result()` after `PERF_DUMP()` and on the `FAILURE` path, before teardown |
| `oct2.c` | autoboot gate widened with `\|\| defined(ATM_PROFILING)` plus a name-resolved level branch; per-frame `atm_prof_bench_tick()` after `MainGame()` in `STATUS_SinglePlayer`; the framelag determinism pin widened from `FIDELITY_DUMP` to `FIDELITY_DUMP \|\| ATM_PROFILING` |
| `main_3ds.c` | `platform_get_3d_slider()` gains an `ATM_BENCH_STEREO` pin (same role as the existing `FRAME_INTERP_FORCE_MONO` pin) |

Design decisions worth keeping:

- **Shared core compiled in place, never copied.** `atm_core.c` is built from `/mnt/nas/Development/netload_stub/sysmodules/rosalina/source/atm/atm_core.c` with the game's own numeric flags, so the game, Rosalina, and the host conformance test cannot diverge on bucket edges or percentile rounding. It is therefore also outside the `AGENTS.md` gate scope (like `picaGL/**`): vendored ATM firmware, not Forsaken game code.
- **`svcGetSystemTick()`, not `osGetTime()`.** `osGetTime()` is 1 ms granular and would quantize every sample; the tick counter (SYSCLOCK_ARM11 = 268111840 Hz) resolves to ~3.7 ns. The ATM histogram still buckets at 1 ms, so `p50/p95/p99` remain bucket-edge estimates while `avg_us`/`max_us` are tick-derived. libctru's `SYSCLOCK_ARM11` macro is a double, so a local integer constant is used — this keeps the hot path free of double arithmetic (R4) and of a runtime 64-bit divide (R11: constant divisor → multiply/shift).
- **Level pinned by name, not slot.** `mission.dat` ordering differs between `EDITION=original` and `EDITION=remaster`, so a pinned slot would silently benchmark a different level per edition. Unresolvable names fall back to slot 0 and record `resolve=fallback0` in the result file.
- **Stereo pinned at build time.** The 3D slider is a live analog input; an unpinned slider makes "the same run" depend on where the slider happened to sit, so mono/stereo baselines would not be comparable.
- **No SD I/O in the frame path.** At most two writes per run (a `status=started` marker when the window opens, the summary at teardown) through a private fd. `trace()` was deliberately not reused: it is `__3DS_DEBUG__`-gated and would drag in autoboot and god mode, i.e. a fidelity change dressed as instrumentation.
- **Bounded fallbacks so a bad build cannot hang the console**: `level_never_loaded`, `left_gameplay` (five consecutive measured frames without a gameplay tick), `deadline`, and `frame_target`.
- **Determinism.** The benchmark reuses the existing framelag pin (`framelag = 1.0F`, one 71 Hz step per frame). Without it, two runs of the same level execute different amounts of simulation work per frame and per-frame timing would compare unequal work.

### Build and gate evidence

```sh
export DEVKITARM=/opt/devkitpro/devkitARM DEVKITPRO=/opt/devkitpro
make -f Makefile.3ds EDITION=remaster ATM_PROFILING=1 RENDERER=citro3d SLIM=1 -j"$(nproc)"   # exit=0
tools/perf/check.sh                                                                          # exit=0
make -f Makefile.3ds EDITION=remaster RENDERER=citro3d SLIM=1 -j"$(nproc)"                   # exit=0 (release neutrality)
```

- Profiling build: `forsaken3ds.3dsx` 3 531 916 B sha256 `d58e972a01f85f8ad5a99b93630b94eeb2737b307b1cd946af1dbab05134faa2`; matching `forsaken3ds.elf` 5 449 840 B sha256 `dff69a16a39c9fb0d9b4c6f1c2d111d2c708b0bac4d3eae502826262a3181b68` — kept beside the `.3dsx` as required. Log `build_3ds/atm-phaseB/build.log`.
- Gates: `gates_vfp` PASS `blockers=0 reviews=0 baselined=345`, `audit_arm11_math` CONTRACT CLEAN, `audit_r11_idiv` NEW=0, `audit_cxx_class` 0 violations. **Zero new findings** from the two new root-level files, which are in gate scope. Log `build_3ds/atm-phaseB/gates.log`.
- Release neutrality: a default build with no profiling flags links **zero** `atm_*` symbols (`arm-none-eabi-nm forsaken3ds.elf | grep -E 'atm_(prof|frame|bench)'` → no match) and `build_3ds/atm_prof_3ds.o` is a 688-byte empty TU. Release `.3dsx` sha256 `503b4a0c4bfccd8bb9e3ff1558f7cd60eb7ffd77d87d6651702d2d86fe4b7c4d`. Log `build_3ds/atm-phaseB/build-release.log`.
- The superseded 2026-11-03 binaries were preserved before overwriting: `build_3ds/preserved-20261103/forsaken3ds.3dsx` sha256 `fb6e80b46aedb29211933a84277c7b1b312bb09e4f598f4d6f8186501243887e`, matching the value banked in `docs/validation/ralph-pi-log.md` iter 021.

### Verification run — TIMEOUT, and it exposed the blocker

```sh
ATM=(python3 /mnt/nas/Development/netload_stub/tools/atmctl.py --ip 192.168.4.167 --device-id old3ds-lan167 --timeout 120)
"${ATM[@]}" run /mnt/nas/Development/forsaken/forsaken3ds.3dsx ./perf/atmB-verify-1 \
  --build-id forsaken-atmB-d303f07-1 --scenario forsaken-bench-mono --run-timeout 300
```

`outcome=TIMEOUT`, `run_id=run-7fc8d943-2`, `verified_artifacts=15`, `--timeout 120` was required for the 3.5 MB deploy (the controller default is 10 s). Stages: `deploy` (sha256 matched) → `verify_staged state=staged hash_check=expected` → `running launch_result=0x00000000` → still running at 300 s. No `/3ds/atm_result.txt` was ever produced. Full JSON `perf/atmB-verify-1.run.json`; retrieved manifest `perf/atmB-verify-1/retrieval_manifest.json`.

The app was stopped with the documented `atm cmd terminate` and the clean exit was proven from the journal (`seq=4 state=exited launch_result=0x00000000 failure=none`), not from the UDP ack. `boot_count` stayed 54 and `recovery=none` throughout: no crash, no dump, console left `listening`.

Root cause of the timeout: the boot-log tail shows the process still inside level model init (`[initmodel] i=400 name=n64\bhgun4.mx`). Level load runs inside a single `RenderLoop()` iteration, so the main loop does not iterate and the benchmark's `ATM_BENCH_ENTER_TIMEOUT_MS` fallback cannot fire during a blocking load. Follow-up: measure the load window separately (or raise `--run-timeout`), rather than pretending the frame-level watchdog covers it.

### The blocker: the enrolled "Old 3DS" reports New-3DS hardware

`sdmc:/forsaken_boot.log` is servable over ATM's HTTP pull and is truncated per process, so it is provably this run's output (banked at `perf/atmB-verify-1.boot.log`, sha256 `54d4873ab525e2ee1ae41e7deeb6bf0f70c0bd6684b6094d0a71083e0ae7c7b9`):

```text
[platform] is_n3ds=1 region=124MB tier=HIMEM (512²) malloc=32MB linear=32MB
[hd_textures] region=124MB primary=hd_textures fallback=hd_textures_lomem
```

`APT_CheckNew3DS()` returns 1, the application memory region is 124 MB (an Original 3DS has 64 MB of RAM total), the texture tier chosen was the 512² HIMEM pack, and `main_3ds.c:312-316` therefore enabled the New-3DS CPU speedup. Three independent kernel reads agree, and `render_c3d.c:2225-2227` documents 124 MB as the N3DS case.

Consequence: **no Old-3DS measurement can be produced on this console**, and the task forbids substituting New-3DS timing. `perf/BLOCKED.md` is raised for operator decision (confirm/swap the console, or explicitly restate the objective). This is not an unreachability or recovery blocker — the console is reachable, `operational=yes`, `recovery_ready=yes`, and healthy throughout.

### Phase B status

**Implemented and gate-clean; not yet hardware-validated.** The instrumentation compiles into a profiling-only build, is provably absent from the release build, and the deterministic benchmark is wired end to end, but no frame sample has ever been produced because the only available console is the wrong hardware class. Phase C (three-run baseline) cannot start until the console question is settled.

### Housekeeping notes for the next iteration

- `perf/BLOCKED.md` is live and must be cleared by the operator, with the clearing recorded, before anything preflights again.
- `atm artifacts` still lists ~30 staged payloads including the now-superseded `forsaken3ds.3dsx`. Cleanup is `atm del <basename>` proven by listing absence, and never targets the currently staged launchable build.
- `ATM_BENCH_LEVEL=ramqan` resolves only against the remaster mission list; the locally staged `romfs/data/levels/mission.dat` is the 1998 15-entry list, so the level actually benchmarked depends on which romfs is on the console SD. Pin this deliberately once known.
- Lid was `closed_blanked` in every preflight this session; open it before timed runs.
- No commits or pushes were made. All changes remain uncommitted for operator review.

### Addendum — unexplained clean reboot observed after the run was closed

Recorded 2026-11-04 21:55 UTC, after the console was already left `listening`.

Between the post-terminate confirmation (`session=7fc8d943`, `boot_count=54`,
journal `state=exited`) and the closing status read, the console rebooted on its
own: `session` `7fc8d943` → `7cfd7bd0`, `boot_count` 54 → 55, `uptime_s` reset to
156, `boot_kind=normal`, `recovery=none`, `recovery_ready=yes`, `failure=none`,
`last_error=none`, and **no new crash dump** (arm11 dumps still end at
`crash_dump_00000037.dmp`, arm9 at `00000000`, `dump_sync_entries=9`,
`auto_dump_present=not_ok`). No host command issued between those two reads
mutated the device — only read-only `status`/`journal`/HTTP GETs.

A clean boot with no exception path and no recovery state is not a crash, so this
is filed as an **unexplained state change**, not as a crash finding, and it is
recorded in `perf/BLOCKED.md` as an operator question. It matters for the loop
even though the console is the wrong hardware class: a spontaneous reboot
between runs would invalidate any timing series on any console, because ATM's
session and the app's cold-start state would no longer be matched to the run
that produced them.

Final console state at end of iteration: `device=old3ds-lan167`,
`firmware_id=d303f0764adc+dirty`, `session=7cfd7bd0`, `boot_count=55`,
`state=listening`, `busy=no`, `recovery=none`, `recovery_ready=yes`,
`failure=none`. No app left running. No commits or pushes made.

---

## Fresh iteration — 2026-11-11 — first real Old-3DS baseline + first optimization (KEEP)

Branch: `perf/o3ds-framerate-20261111`. All work here; nothing pushed
(operator reviews; branch stays unpushed). Pre-existing dirt from
`experimental/native-renderer` left untouched except the adopted harness
files noted below. No physical action asked or taken; all reboots remote
(`atm reboot --wait 180`, boot-count proven).

### 0. BLOCKED.md (2026-11-04 N3DS claim) is superseded — with evidence

The live console was preflighted first: `device=old3ds-lan167`,
`firmware_id=9f3632fe047c+dirty` (firmware moved on since 11-04; all
comparisons below are within this firmware only), `boot_count=337`,
`recovery=none`, `recovery_ready=yes`, `operational=yes`, lid
`closed_blanked`. After `atm reboot --wait 180`: boot 337→338,
session change proven, stub serving 17491.

The game itself (normal libctru init via stub chainload, NOT the
ATM-launch path) reports on hardware, fresh boot log:
`[platform] is_n3ds=0 og_profile=0 clock=268MHz cache=L2-off
region=64MB tier=LOMEM (256²) malloc=32MB linear=22MB`.
The 11-04 `is_n3ds=1/124MB` reading came from an ATM-launched run's
stale fossil log; under a proper NS launch the same console measures
as Old 3DS (consistent with the independent memid-2 64 MB region
proof). The N3DS park no longer applies; this entry records the
clearing (operator tasking 20261111 + hardware identity proof above).

### 1. Mechanism used (AGENTS.md §10b, all hardware-proven this session)

- Push ONLY by `3dslink -a 192.168.4.167 <unique-name>.3dsx` to the live
  stub (port 17491); never `atm run`/`atm launch` for game builds.
- The stub chainloads (SetTarget + full argv + clean exit) and the NS
  takeover relaunch runs the game; after clean exit the stub is back
  (discover `boot3ds`, 17491 OPEN) — no reboot needed between runs.
  Re-verify liveness with `atm discover`, never bare TCP connects.
- `transfers.log` (`status=ok` + `bytes_out` == file size) is the only
  push receipt that counts (3dslink % lies).
- atm_result.txt identity line now carries `console=is_n3ds=0
  region_mb=64` (measured, normal init) per run.

### 2. Baseline: ramqan mono, EDITION=remaster RENDERER=citro3d SLIM=1 (HARDWARE)

Build fb-mono1 (sha256
`e609f75e0ddd8097550e2cca2cfadbba2cdb4802fb09828c0405acd7fae5117`):
`ATM_PROFILING=1 FRAMEDUMP=1`, bench ramqan/240/30/mono. Normal init
(headless `__appInit` + aptMainLoop bypass gated behind new
`ATM_HEADLESS=1`, default 0 — chainload builds use the proven
framedump configuration). Perf gates ALL PASS.

| run | build | load_ms | window_ms | avg_us | min | max | over_budget |
|---|---|---|---|---|---|---|---|
| fb-mono1 | e609f75e | 30345 | 29693 | 113428 | 80470 | 128704 | 240/240 |
| fb-mono2 | e609f75e | 30362 | 29778 | 113665 | 94444 | 129485 | 240/240 |
| fb-proof1 | e609f75e | 30379 | 46864* | 113859 | 84642 | 133367 | 240/240 |

\* proof1 window stretched by a mid-run framedump DUMP (17 s of SD
writes land between frames, outside the measured span — samples clean).
Noise floor on avg: ±0.2%. p50/p95/p99 read a constant 32000: the ATM
histogram is 32×1 ms buckets with last-bucket ≥31 ms overflow, so every
~80–130 ms sample saturates the top bucket — percentiles carry zero
information at this frame time; avg/min/max/count are the metrics.
Level-load (~30.3 s, σ 17 ms) is one-time per run, not per-frame.

### 3. Stereo cost (same binary shape, ATM_BENCH_STEREO=1)

fb-stereo1 (73d8f00e): 240 stereo_frames, avg **138671** (+25.1 ms,
+22% vs mono). Stereo does NOT double the frame: fixed cost ≈ 88 ms +
~25 ms/eye. The fixed cost is the target, not the second eye.

### 4. Phase breakdown (PERF_CAPTURE=1 diagnostic builds, shares only)

cnt1 (abs values inflated ~3.4% by gate overhead; shares valid), ms/frame:
MAINGAME 61.2 = MROUT 22.8 (BULLPRIM 14.8, ENEMIES 0.2–5.6 varying,
rest ~2.4) + RENDER 38.1 (CAM 31.3 = VIS 2.7 + BG 7.5 + OPAQUE 11.6 +
TRANS 7.1; FSBEGIN 6.1; FSEND ~0). ATM frame 117.5 − 61.2 = 56.3 ms
outside MainGame. PRESENTLOOP stage (new): render_flip = 0.2 ms —
the flip is NOT the gap. DIAG split: handle_events 0.05 ms,
music_update ~0, post-MainGame sound tail (stage 58) = **51.9 ms**.
Per-frame live bullets (new counters): prim 105.5, sec 0.

### 5. opt1 (KEPT as complement): throttle looping-sfx maintenance to 1/4

 commit cbed248. Steady-state ndsp param pushes + play-state checks
every 4th frame; range/create/release/first-start stay every-frame.
Result fb-opt1 (ba32ac03): avg **113259** (−0.15%, noise). The gated
body was NOT the cost — the throttle alone does not move the wall.
Kept: zero-risk complement to opt2 (sourced-slot params).

### 6. opt2 (KEEP): back off failing looping-sfx creates (the 52 ms)

commit 9ff9dcc. Counters showed sfx_create = 2823/run (10.4/frame)
with sfx_release = 0: failing `sound_source_create` retried 71×/s.
A failed load costs milliseconds of FS/alloc work; 10.4 × ~5 ms =
the tail. Retry a failing slot at most every 32nd frame (~0.45
sim-s); release resets the timer so re-entry stays immediate. A sound
that can never load stays silent either way (it never played).
Result fb-opt2 (binary `fb-opt2-ramqan.3dsx`): avg **64089**
(−49.2 ms, **−43%** vs opt1), min 47120, max 105807, window 16.7 s,
sfx_create 2823 → 94, sim identical (prim_bullets 28594, slot 19,
resolve exact). 8.8 fps → 15.6 fps on ramqan mono.

### 7. Render proof (HARDWARE)

fb-proof1 (same bytes as baseline): `DUMP proof1` at title →
`OK`; twins retrieved (`/3ds/framedump_0001_proof1_{top,bottom}.bmp.log`,
exact sizes 288054/230454, BM magic); FNV-1a recomputed from retrieved
bytes MATCHES log on both screens (top 771bc5a5, bottom 933fe0d1 —
both live content, bottom is not the static clear). Proof GREEN.
Banked under perf/fb-proof1/ with the verifier (`/tmp/fdmp_verify.py`).

### 8. Test collateral note (durable-test-collateral)

No new unit-test framework: the maintained verification for perf work
is this log + banked hardware artifacts + `tools/perf/check.sh` (run
per change, all green throughout) + the deterministic bench itself
(identical sim: bullets 28594 ×3, stage totals within 0.3%). A host
unit test cannot pin hardware frame time; the A/B + hash MATCH is the
test. Sfx counter + stage instrumentation stays in harness builds
only (no-op macros without ATM_PROFILING/PERF_CAPTURE).

### 9. State and next

- Keepers: cbed248 (throttle) + 9ff9dcc (backoff). DIAG/DIAG2 commits
  reverted in-tree (history kept for the attribution record).
- Next targets (all HARDWARE ms/frame): BULLPRIM 14.7 (105 bullets ×
  ~140 µs — collision battery per bullet), ModelDisp 11.2 (2582
  fine-grained MDLENTRY calls × 3 µs), BG 7.5, VIS 2.7, TRANS 7.1.
- Console left: listening, stub serving (post-opt2 exit), no app
  running. Staged payloads fb-* accumulate per loop hygiene (current +
  previous kept; del older at iteration end).

---

## Fresh iteration — 2026-11-11 (cont.) — opt4 color table (KEEP), bullets parked, variance lessons

Branch unchanged. Same console/firmware. Baselines: opt2 clean mono
avg 64089 (fb-opt2); control rerun fb-ctl1 (same bytes) avg 64018
(Δ 0.1% — device stable across 2+ hours; see variance note §14).

### 13. opt4 (KEEP): byte-to-float color LUT kills 4 vdiv.f32/vertex

draw_render_object converted every vertex per frame (12.3K/frame,
58 draws): pos+uv copies plus 4× `((u32>>s)&0xFF)/255.0f` — real
`vdiv.f32` each (confirmed by disassembly: 4 vdivs in the conversion
loop). A 1 KiB static table (`s_c255[b] = (float)b/255.0f`, init once)
replaces them with L1D reads. Bit-identical (same correctly-rounded
quotient per byte value), so pixels cannot change; framedump proof
still required and GREEN (see §14). Gates ALL PASS (R2 review on the
first `(unsigned)` cast fixed by dropping it; baseline count
345 → 334 as the removed vdivs burn adoption debt).
Result fb-opt4b (clean rerun after a transient load failure, see
§15): avg **58890** (−5.2 ms, −8.1% vs opt2). 8.8 → 17.0 fps total.

### 14. Render proof for opt4 + variance lessons

fb-opt4 `DUMP opt4` at title → `OK`; twins retrieved (exact sizes, BM
magic); FNV-1a recomputed from retrieved bytes MATCHES both screens
(top 5d763949, bottom 85476dc5). Proof GREEN. (The bench run that
carried the proof hit `level_never_loaded` — an 86 s title-phase DUMP
starved/contended the SD and the level load missed its 90 s window;
clean rerun fb-opt4b completed normally. Lesson: proof DUMPs and timed
benchmark windows do not mix — run proof in separate untimed runs or
accept the window stretch, never both in one counted run.)
Variance: DIAG9-era runs showed ±10–20 ms swings (diag9 82.9,
diag9b 75.3 vs ~65 baseline) while the control rerun held ±0.1%.
Attribution: transient device/SD contention, not the tree (same bytes
reproduce). Protocol change: every keeper delta must be confirmed by
an immediate rerun; single perturbed runs are discarded, not averaged.

### 15. Bullets parked (14.7 ms, SoundInfo idea banked)

DIAG3–DIAG9 mapped the battery: update 0.4, wall+bgobj 2.5, water
0.2, ship+sec 1.0, enemy+trig 10.1 ms/frame (105 other-owned bullets
× 92 enemies walked, 37 full RaytoSphere2 tests each; vgcalls=0,
triggers never fire — bench has no ship/enemy bullets).
opt3 (compact hittable list) cut visits 9708 → 3877 but wall did not
move (early-outs were already ~free; the 1.05M full tests dominate) —
REVERTED, not stacked. All DIAG stages reverted; counters kept as
Phase-D harness infra (sfx/prim/sec/draws/verts; per-visit counters
removed after showing 1.2 ms observer overhead).
Banked next: SoundInfo group pre-filter mirroring CheckHitShip's
existing gate (skips sealed-group enemies before RaytoSphere2).
Requires a fidelity A/B (protected hit set) — not attempted this
session; needs 2–3 hardware cycles plus multi-trajectory validation.

### 16. FSBEGIN anomaly (open, bounded)

FSBeginScene = C3D_FrameBegin(SYNCDRAW) + setup. No-bucket runs:
6.0–6.6 (but 8.9 post-opt4, 9.4 stereo — variable). Bucket runs:
10.9 ×2 (systematic +4.8). Identical draws submitted, identical CPU
before FrameBegin — unexplained. Leading hypothesis: GPU-wait
component exposed variably by pipelining (stereo +3.3 supports a
~25–35 ms GPU share); bucket correlation may be layout/timing.
Bounded (±5 ms), net bucket still −0.5. Buckets stay flag-only (not
default); FSBEGIN stays open. If GPU-wait is real, CPU cuts below
~30 ms/frame will backfill — far from today's 59 ms, no action now.

### 17. Counters kept as harness infra (zero-cost when gated)

atm_prof.h now carries: level_load_ms, status_exits, stereo/mono
frames, prim/sec_bullets, sfx_used/create/release, draws, verts.
Per-bullet/per-draw adds (~100 ns); per-visit counters deliberately
NOT kept (measured 1.2 ms/frame observer overhead). All degrade to
no-ops without ATM_PROFILING; draws/verts need no flag beyond it.
DIAG3–DIAG9 stage indices (48,57–62) are free again.

---

## Fresh iteration — 2026-11-11 (cont.) — bullets dissected, buckets kept, FSBEGIN anomaly open

Branch unchanged. All runs same console/firmware (9f3632fe047c+dirty).
Baselines for this section: opt2 clean mono avg 64089 (fb-opt2).

### 10. MODEL_BUCKET=1 (kept, flag-only, not defaulted)

Per-frame ModelDisp walked the global model list per visible group
(10.5 calls × ~245 models = 2572 visits/frame, 8.0 ms in MDLENTRY).
`MODEL_BUCKET=1` (pre-existing flag + verifier, Makefile default 0)
builds a per-camera clip-group bucket once and walks chains.
Verify build (VERIFY=1 + capture): `model_bucket_verify
groups=2856 entries=3837 mismatches=0` — behavior-identical on
hardware; framedump DUMP+MATCH on the measure build (proof GREEN,
both twins). Wall: 64089 → 63579/63582 (two clean runs, Δ −0.5 ms).
MAINGAME stage −2.7 ms but FSBEGIN stage +4.8 ms (6.1 → 10.9, two
bucket runs, systematic). The FSBEGIN growth is unexplained (identical
draws submitted; see §12) and eats most of the win. Kept: verified
correct, net negative wall, better asymptotic scaling; NOT defaulted
(needs broader level coverage + FSBEGIN answer first). Flag lives in
the log + banked build records, not in the Makefile.

### 11. Bullet battery dissection (DIAG3–DIAG9, all reverted or kept as infra)

105.5 live prim bullets/frame, all OWNER_OTHER (spotfx/modelspotfx
ambient — NOBODY/MODELSPOTFX; no ship/enemy bullets in this bench, so
trigger checks never fire: pcheck=echeck=0). Per-bullet 140 µs:
update 4 µs, wall+bgobj 24 µs, water 2 µs, ship+sec 9 µs,
enemy+trigger 96 µs. The 96 µs span is CheckHitEnemy walking all 92
used enemies per bullet (9708 visits/frame, 37 full RaytoSphere2
tests each): the `// return(NULL);` at enemies.c:6262 is a commented-
out early-out, NOT active code — the function is live (verified by
disassembly + counters, after a misread). vgcalls=0 (no BSP-walk hits).
- opt3 (compact hittable-enemy list, 660f71f): visits 9708 → 3877,
  but wall 64.09 → 64.06 (no movement). The eliminated early-outs were
  already ~free; the 1.05M full tests dominate. REVERTED (a11bd3d),
  not stacked.
- DIAG counters kept as harness infra (enemy_visits/fulltest, vgcalls,
  trig_*, own_*): zero-cost without flags, guide future bullet work.
- Takeaway: the cost is ~37 full RaytoSphere2+VectorLength tests per
  bullet (~2.6 µs each). A SoundInfo group pre-filter (mirroring
  CheckHitShip's existing gate) is the banked next idea; needs a
  fidelity A/B as it touches the protected hit set. Bullets parked at
  14.7 ms pending that protocol.

### 12. Open anomaly: FSBEGIN +80% with buckets (6.1 → 10.9 ms)

FSBeginScene = C3D_FrameBegin(SYNCDRAW) + clear + binds + uniforms.
Identical draws, identical CPU-before-begin, yet +4.8 ms in both
bucket runs. Not explained (GPU-wait model contradicts unchanged
inputs; cache-layout unproven). If it is GPU-wait growth, CPU cuts
will backfill 1:1 and the strategy must shift to GPU work. Next:
stereo+PERF_CAPTURE breakdown (existing ATM_BENCH_STEREO flag) to
split FSBEGIN scaling with GPU load — no code change.

---

## Fresh iteration — 2026-11-11 (final) — VERIFIED DELIVERABLE: 113.4 → 58.9 ms

Branch `perf/o3ds-framerate-20261111` (unpushed, per instructions).
Final binary fb-final-ramqan.3dsx (banked under perf/fb-final/ with
ELF + manifest; built from committed tree, default flags +
ATM_PROFILING + FRAMEDUMP, no MODEL_BUCKET, no PERF_CAPTURE).

HARDWARE (old3ds-lan167, fw 9f3632fe047c+dirty, boot 338, is_n3ds=0,
region 64 MB, OG 268 MHz), ramqan mono, 240 measured frames:
avg **59058** (min 33349, max 111720), reason=frame_target,
level_load 30346, sim identical (prim_bullets 28594, slot 19 exact).
Render proof GREEN (DUMP final1 at title → twins retrieved, FNV-1a
MATCH both screens). Gates ALL PASS (baselined 334 — opt4 burned
11 adoption-debt vdivs).

Ranked idea table (all deltas vs same-scenario baseline on THIS
console, firmware 9f3632fe047c+dirty):

- baseline mono fb-mono1/2/3: 113.4–113.9 avg (±0.2%). Anchor.
  p50/95/99 unusable (32-bucket histogram saturates at 32 ms).
- stereo pin fb-stereo1: +25 ms (+22%). Fixed cost ≈ 88 ms +
  ~25/eye; stereo NOT 2×. (A later stereo-perf run read 115 vs 139
  for clean stereo — one of the two was perturbed; stereo needs a
  clean rerun before any stereo claim is final.)
- breakdown fb-perf1/cnt1: MAINGAME 61 = sim 22.8 (BULLPRIM 14.7)
  + render 37.9; tail 52 ms found via PRESENTLOOP + tail staging.
- sfx counters fb-cnt1: 105.5 live prim bullets, 0 sec; sfx_used
  28.4, sfx_create 10.4/frame with 0 releases (failing loads).
- opt1 throttle fb-opt1: −0.2 ms (no movement alone; kept as
  complement to opt2 — gated body wasn't the cost).
- opt2 backoff fb-opt2: −49.2 ms (−43%). KEEP. Creates 10.4 →
  0.35/frame. Core win.
- buckets fb-bkt1/2: −0.5 ms. KEEP as flag-only (audit 0
  mismatches/2856 groups; FSBEGIN +4.8 unexplained; not defaulted).
- opt3 compact list: 0.0 ms (visits −60% but wall flat — early-outs
  already free). REVERTED, not stacked.
- opt4 color LUT fb-opt4b/final: −5.2 ms (−8%). KEEP. 4 vdiv →
  L1D reads/vert; bit-identical; proof green.
- control rerun fb-ctl1 (opt2 bytes): 64.02 ≈ opt2 64.09 — device
  stable; observed ±10 ms swings are per-run transients, not drift.

Framerate: 8.8 → 17.0 fps (still over the 14 ms budget, 1.9×
throughput). Keepers: opt1 + opt2 (sfx), opt4 (color LUT).
Reverted: opt3, all DIAG stages/counters except Phase-D infra
(prim/sec/sfx/draws/verts). Bucket flag documented, not defaulted.

Open threads (not attempted): SoundInfo group pre-filter for the
bullet enemy walk (needs fidelity A/B + multi-trajectory validation);
FSBEGIN anomaly (bounded, logged); vertex-cache/indexed draws for the
remaining ~12K verts/frame conversion + 58 draws/frame submission.

SD hygiene gap: ~20 uniquely-named fb-*.3dsx pushed via 3dslink live
under /3ds/ but outside `atm artifacts`/`atm del` reach (`deleted:
false`); ~70 MB total, all byte-identical to banked builds. Left in
place; manual SD cleanup can drop all but fb-mono1 (baseline anchor),
fb-opt2 (pre-opt4), fb-final (deliverable).

## 2026-11-11 — opt5 STEREO_REPLAY wall + pixel proof (HARDWARE)

Builds: fb-proofA2 (no replay, snap-all capture, c2a5126) vs
fb-proofB2 (STEREO_REPLAY, same HEAD). ramqan stereo bench, 240+30
frames, OG 268 MHz. Framedump selftest at frame 100, twins retrieved
via /3ds/*.bmp.log, file-pixels FNV-1a verified against logged hash
(all MATCH — retrieval channel green).

Wall: A2 avg 82672 us (draws 30561 = 127.3/frame, verts 6.37M =
26.5K/frame) vs B2 avg 77560 us (draws 15950 = 66.5/frame, verts
3.30M = 13.8K/frame). Replay saves 5.1 ms (−6.2%): the VBO
pack/convert loop runs once, not per eye. Sim trajectory identical
(prim_bullets 28594, sfx 7701/94 both runs).

Pixel (same-frame twins, snap-all-then-write): left A2-vs-B2 0 bytes
differ (replay leaves the dominant eye pixel-identical). Bottom
0.083% (HUD run-noise: score/time digits). Right 13.4% clustered in
3 blobs, 75.8% of diffs on bright content, B2-right renders 3.5%
fewer bright pixels than A2-right — NOT diffuse shear shift. Cause
open: left-eye-frustum culling dropping right-visible geometry vs
sim run-noise. Control running: B3 = same replay binary re-pushed,
B2-vs-B3 isolates pure run noise (expect ~0.1% if sim deterministic
on the right eye; ~13% if noise dominates).

Snap-all fix (c2a5126): stereo DUMP path snapshot both top eyes
back-to-back before any SD write. Sequential capture drifted seconds
(100s of frames) between left and right, invalidating all same-frame
A/B. Mono path unchanged. Tooling hardening, not an opt.

Verdict opt5 (2026-11-11, B3 control): REVERTED, not a keeper. B2-vs-B3
(same replay binary, two runs) is 0.000% on both top eyes — the
renderer is bit-deterministic — so the A2-vs-B2 13.4% right-eye delta
is entirely the replay, none of it noise. Cause: replay draws the
LEFT-eye visible set (portal/BSP walk + cull from the left-eye camera
position, oct2.c:4795) with right-eye matrices. Geometry visible only
to the right eye is missing deterministically every run (clustered
blobs, −3.5% bright pixels vs live-right) — a faster wrong frame, not
an optimization. The −5.1 ms wall win (convert+submit once) is real
and worth recovering via opt5b: single union walk from the CENTER
camera with a both-eyes-widened frustum, gather once, replay per eye
with true per-eye matrices. Scoped, not built: needs portal-walk
analysis (no explicit frustum planes in tree — visibility is
eye-position portal-walk-determined, oct2.c:4801/4851), fidelity risk.
STEREO_REPLAY stays a build flag, default off; deliverable keeps
two-pass stereo.

SD hygiene: +3 staged builds (fb-proofA2/B2-ramqan.3dsx,
fb-proofB2 reused for B3) outside `atm del` reach; same cleanup note.
Console left: boot 341, listening, game exited clean (bench
frame_target + QuitRequested). Next preflight as usual.

### Backlog (user research ideas, unmeasured — bottom of stack)

- Lighting cap: PICA200 has 8 hardware lights and the engine already
  drives them + 4 more software lights. Proposal: prioritize the 8
  hardware lights first and drop the 4 software lights on OG 3DS. Needs
  profiling (software lights = CPU per-vertex loop; GPU shader path
  already replaced XLight1Group — measure what the +4 actually cost and
  what the visual delta is before cutting). Not attempted; measure first.

## 2026-11-11 — Phase 4: SoundInfo group pre-filter (KEEP, default on)

Idea (banked §11/§15): CheckHitEnemy walks all 92 used enemies per
bullet with no group gate (CheckHitShip has one), running ~37 full
RaytoSphere2 tests per bullet (~2.6 us each) that mostly miss on the
ramqan ambient trajectory (pcheck=echeck=0).

Change (one idea, one commit): CheckHitEnemy takes the bullet's group
(new `u_int16_t Group` param, mirrored from the CheckHitShip call
signature; callers pass GroupImIn) and skips enemies with
`SoundInfo[eGroup][bGroup] != 0` before any per-enemy vector math —
exactly CheckHitShip's gate (primary.c). Function moved verbatim
enemies.c → new checkenemy.c (auto-picked by the wildcard) so the
host test links the REAL function; Makefile flag `CHE_PREFILTER`
(default 1) + stamp coverage; fidelity output moved to
sdmc:/3ds/forsaken_fidelity.txt (HTTP-retrievable) + optional
FIDELITY_CHECKPOINT_TICKS interval.

Fidelity (protected hit set — the bar for this change):
- Host: tests/test_checkhitenemy.c links REAL checkenemy.c + REAL
  sphere.c (RaytoSphere2), stubs only the trivial/data layer. 9/9
  green: connected hit lands at the surface, sealed skipped with Dist
  untouched, sealed-nearer loses to connected-farther, owner/disabled/
  off-ray preserved. Sensitivity: gate-stripped build fails 4/9.
- Hardware A/B, OG 268 MHz, fw 9f3632fe047c+dirty: ramqan mono
  FIDC checkpoints (60/120/180/240, ship/enem/prim/sec components)
  byte-identical gate-off vs gate-on across THREE runs (F0/F1/F1b
  rerun); bio-sphere M0/M1 byte-identical. No crash, no divergence.
- Limit (honest): the bench fires nothing, so both trajectories are
  all-miss for the filter (skipping a miss cannot change an outcome —
  provable) — combat trajectories with real cross-group hits remain
  uncovered. The gate mirrors the shipped ship-gate semantics; the
  sealed-nearer→connected-farther winner change is the intended
  alignment (ships already behave this way). A firing-input bench
  would close this; none exists today.

Wall (HARDWARE, ramqan mono 240f, deliverable config):
- off W0 59744 (vs fb-final 59058, +1.2% tree drift) → on W1 52002.
  Delta -7742 us (-13.0%). sim identical (prim_bullets 28594, draws
  15715). Fidelity-config pair: 61011 → 52611/52622 (rerun-confirmed,
  11 us apart). bio-sphere (no prim bullets): 17142 → 17140, no
  movement — consistent (nothing to skip), fidelity still identical.
- Render path untouched: no framedump proof required (listener-only
  FRAMEDUMP in all builds, no draws changed). Gates ALL PASS.

Verdict: KEEP, default ON (CHE_PREFILTER=1). Deliverable moves
59058 → ~52002 (-7.1 ms, -12%; total 113.4 → 52.0 ms, 2.18x).
R3 pin on the gate (hot loop by the ≥256x criterion; reviewed).
SD hygiene: +6 staged builds (fb-cheF0/F1/M0/M1/W0/W1) outside `atm
del` reach; same cleanup note. Console left boot 348, listening,
benches exited clean.

## 2026-11-11 — Phase 5a: transpoly single-pass (REVERTED, exact but no movement)

DIAG2 sub-stages (63-66, committed as infra): TRANS 6.6ms = TRANPOLYS
6.60 (water 0.10, tris 0, misc 0.02). TRANS = 10 draws, 2340 verts/frame
— ~0.6ms/draw, 13x the opaque per-draw cost. Suspect: PolyDisp double
TPage-chain walk (count pass + fill pass per group call).
Tried: single-pass fill in PolyDispGroupClipped (same visit order, same
4*numbits accounting, fill inline; exact by construction).
HARDWARE wall (mono ramqan, selftest builds): off 52272 → on 52244
(-28us, noise). The removed walk cost nothing measurable (cache-resident
chains; cost is in fill/draw, not traversal). Pixels: OFF-vs-ON twins
byte-identical both screens (top d512f860, bottom 53d462b5) — proof
GREEN, transformation exact. REVERTED per no-movement rule (exact but
pointless). Banked perf/fb-spOFF (mono twins+wall), perf/fb-spON.
Lesson: TRANS cost is per-draw/fill, not traversal — next idea must
target draw count, texture binds, or fill overdraw, not walks.

## 2026-11-11 — External spec review (Astra R01–R22) vs tree + hardware

Cited revision dcdc6fe not present locally (different fork); every
premise below re-verified against HEAD 75273de + working tree. Findings
are hypotheses checked against source and our Old-3DS measurements, not
gains. Baseline context: mono ramqan ~52ms post-opt6 (BULLPRIM 9.4,
OPQMODEL 10.7 = MDLENTRY walk 7.6, TRANS 6.6, BG 6.1, ENEMIES 5.5,
FSBEGIN 6–11 variance, VIS 2.7). CPU-bound (opt6 moved wall 1:1).

Confirmed in source: R02 overflow path (FLUSH+FrameSplit+reset, no
completion wait, render_c3d.c:2995 — defect real, unreached in bench:
497KB used of 4MiB arena); R03 triple-count (allocator ×3 at
render_c3d.c:1924 AND callers already pass index counts: mxaload.c:362
triangleCount*3, oct2.c:3172/6955 32767*3 → 768KiB recoverable);
R04 36B gpu_vertex_t; R05 DrawArrays expansion; R08 TexBind-before-cache
(render_c3d.c:2127 precedes the early return); R12 adpcm_advance +
R13 setup_wavebuf present; R09 replay exists-but-disabled (our opt5).
R06 partially active: XLightMxloadHeader runs per MODFLAG_RealLight
model, but MDLLIGHT stage = 0 and MDLSTATIC = 0.9ms — the big-restore
premise is mostly inactive under GPU_LIGHTING here.
Already-have-it: R01 ≈ PERF_CAPTURE stages + atm counters + fidelity
hashes + framedump proof + banked artifacts (gaps: audio-refill timing,
GPU time — citro3d query returns garbage here, proven 2026-11-11 — and
frame-begin-wait split; fill gaps, don't rebuild R01); R10-model =
MODEL_BUCKET (flag-only); build identity = .dflags.stamp.

Recommended order (measurement-led, diverges from spec where noted):
1. R03 index units (confirmed, tiny, independent) + write-bounds audit.
2. R20 build matrix (-O3 selected/LTO; stamp already handles identity).
3. TRANS fill/draws (OUR lead — spec R04/R05/R06 miss it: 10 draws,
   2340 verts, 6.6ms; walks disproven tonight, next idea targets draw
   count/binds/fill, not traversal).
4. Buckets default + FSBEGIN anomaly (R10-model exists; net was -0.5
   on +4.8 FSBEGIN — explain FSBEGIN first).
5. R08 bind-cache, R11 fullscreen-branch dead math (VIS 2.7), R06
   measure-before-trim (0.9ms active).
6. R07/R14/R18 only on GPU-boundedness evidence (we're CPU-bound);
   R12/R13 on audio-thread profiles; R19/R15/R04 after.
7. R02 stress mode + high-water alongside any arena change (P0
   correctness, no frame win); R17/R21/R22 last.
No implementation this turn — findings + order only, per handoff.

## 2026-11-11 — R03 index allocation units (KEEP, memory)

Spec R03 premise fully confirmed: allocator triple-counted
(render_c3d.c:1924 `numIndices*3*sizeof(WORD)`) AND every caller passes
index-element counts (mxaload/mxload/mload triangleCount*3, water
ntris*3, oct2/RenderBufs 32767*3). Fix: allocate numIndices WORDs.
Write-bounds audit: per-frame fills MAXPOLYVERTS-capped (800 verts/call,
≤1200 indices vs 98301 requested), text overlays write tens of quads,
load-time loops exact. 768 KiB linear recovered (2×393,204).
HARDWARE (mono ramqan, selftest build): 50439 → 50418 on immediate
rerun (21us apart — reproduces). Boots, loads (30333/30377ms), benches
clean; sim identical (prim_bullets 28594); twins file↔log MATCH both
screens; top vs pre-change 608/288000 = 0.211% = known ambient run
variance (same scale as A-vs-A2 0.2%), bottom hash identical across
runs. No regression; memory win only (wall delta vs W1 confounded by
config — see R20 control). Gates ALL PASS.

## 2026-11-11 — R20 build matrix (REVERTED, no wall movement)

Hook (reverted): GAME_OPT/GAME_LTO + stamp coverage, defaults untouched
(-O2, no LTO; banned flags stay banned). Whole-tree -O3 and -flto
(compile+link, devkitARM 15.2, links clean) A/B'd mono ramqan vs
same-tree -O2 control:
- O2 control 50123 (also isolates post-R03 tree drift: W1 52002 → 50123,
  -1.9ms reproduced x3 runs; mechanism open — R03 linear-layout/cache
  coloring vs two removed C3D calls/frame; logged, not claimed).
- O3 50069 (-54us, noise; +135KB binary). LTO 50012 (-111us, noise;
  -16KB binary). Sim/counters identical all runs.
- Side observation (not the metric): LTO level_load 30344 → 27165ms
  (-3.2s load). Init code likes LTO; frame loop doesn't care. Noted,
  not kept (benchmark = frame wall).
Verdict: REVERTED, hooks removed, tree back to -O2. Per-file -O3 would
need pattern rules + its own A/B; not attempted (whole-tree is flat).
Banked perf/fb-o2, perf/fb-o3, perf/fb-lto.

## 2026-11-11 — Phase 5b: FM single-pass (REVERTED, exact, no movement)

TRANFM sub-split (new stages 67-70): TRANS 6.6 = CLIP 0.08, FM 6.16,
EXE 0.06, UNCLIP 0.37, water 0.10. The cost is FmPolyDispGroupClipped
(10.5 calls/frame, 0.59ms each): billboard setup + fill + draws.
Tried the same single-pass merge (walk once, count+fill inline; FM
fill matches its counts exactly via the numbits loop, so no
PolyDisp-style quirk). Wall: off 50450 → on 50467 (+17us, noise).
Pixels: OFF-vs-ON twins byte-identical both screens (top f1a8d71c).
REVERTED per no-movement rule. Banked perf/fb-fmOFF, perf/fb-fmON.
Analysis: both trans walks are cache-free (~0); the 6.16ms is
fill+setup+draws over UNCACHED linear buffers — fill writes LVERTEX,
convert reads them back, scratch writes gpu_vertex_t, index reads,
all uncached (~100-cycle RAM latency each). 2340 trans verts ×
round-trip ≈ milliseconds. Next: fewer/smaller bytes through the
uncached path (compact stream R04, persistent geometry R05, or
cached-buf+flush R17 family) — NOT traversal, NOT state (10 draws).
Insertion sort per draw is O(n²) but input is TPage-ordered already
(sorted by construction) — skipped, not the cost.

## 2026-11-12 — T2K transfer triage (TF01–TF04 vs tree + measurements)

Spec shared 2026-11-12; each premise re-verified against HEAD. No code
changed in triage; visual identity nonnegotiable (framedump MATCH stays
the gate).
- TF01 trig reduction: ALREADY EQUIVALENT — CLOSE, no work. Our
  fast_sinf (new3d.h) is truncation + negative-fraction correction +
  mask, static inline, with the floorf-disassembly rationale documented
  in-tree. The spec's "still calls floorf" does not hold here (T2K's
  own header says the lookup originated in Forsaken). Do not touch.
- TF04-A shared quad topology + direct emission: NEXT Phase-5 lead.
  Our trans fill rebuilds quad indices per call then expands to 6 GPU
  verts in convert (tverts 2340, 10 draws). Premise holds; Phase A
  (float UV retained) fits the measured 6.16ms FM cost. Phase B (20B
  layout) needs the UV eligibility histogram first — atlas UVs rarely
  0/1, likely inapplicable; data before code. Phase C conditional.
- TF02 split streams: premise holds (single interleaved 36B stream),
  but ordered AFTER R04 + mutation measurements per the spec itself;
  the 24B compact path comes first. P2.
- TF03 prep-before-wait: premise holds (prep after FrameBegin). Gated
  on tonight's A/B/A: if GPU-bound is confirmed, overlap is the right
  idea (P1); if CPU-bound, it stays P3 behind direct cuts. R02 safety
  + memory ledger are prerequisites either way.
Stack order now: A/B/A verdict → TF04-A → buckets default (needs the
verdict too) → R08/R11/R06 trims → TF02 → TF03 (priority per verdict).

## 2026-11-12 — A/B/A: buckets wall-neutral; frame is GPU-bound (PIVOT)

A/B/A, mono ramqan, back-to-back (boots 364/365, same fw): O2 50123 →
BKT 50094 → BKT-rerun 50101 → O2-rerun 50087. All four within 36us: no
drift, no bucket wall effect, despite OPQMODEL -5.5ms in stages
(10.69→5.16, MDLENTRY 699611→3853, verify 0 mismatches on current
tree) and FSBEGIN 5.41 (old +4.8 claim REFUTED — was perturbation, not
buckets; fdiag baseline itself was perturbed at 11.08).
Conclusion: CPU work removed (-5.5ms) with zero wall movement = the
frame is GPU-bound at ~50ms. Consistent with history: opt6 cut CPU
7.7ms and moved wall 1:1 (CPU-bound at 59.7); now at ~50 the GPU is
the long pole and CPU cuts backfill. FSBEGIN ~5.4 = residual GPU wait.
STRATEGY PIVOT: stop cutting CPU to move the wall; work the GPU side
(draws, overdraw/fill, texture traffic, shader cost, overlap CPU prep
with GPU via TF03). Buckets stay flag-only (no measured benefit to
defaulting; zero visual change either way).
Banked perf/fb-o2, fb-bkt1(+b), fb-o2b, fb-bktv.

## 2026-11-12 — TF04-A shared quad topology (REVERTED: correct but rounding, no wall)

Implemented: immutable 3 KiB shared quad index table (linear, init-once
+ explicit flush) + indexed DrawElements path for billboard draws
(4 verts/quad copied directly, sort+batching+state unchanged, replay
aware). Mechanism proven working: tverts 561192 → 374128 (-33.3%
exactly, convert de-expanded), draws identical (15734), same batching.
Wall: off 50436 → on 50440 (+4, noise). GPU-bound frame: convert
savings backfill, as predicted.
Pixels: NOT byte-identical (top 55% bytes differ) — but the diff is
95% within 4 levels/channel, 99.9% within 16, zero-mean, uniform
scatter, identical histograms and bright counts. Root-caused as
element-mode rasterization rounding, not a mapping bug: vertex values,
submit order, batching, state, table pattern, count, base semantics
all verified identical; DrawElements runs the GEOSTAGE element path
while DrawArrays does not, and vertex-cache reuse changes interpolation
rounding (accumulated over translucent layers → ±2 LSB). A submit-path
change can never byte-match; it needs statistical equivalence, not
byte identity. Kept the conservative call: REVERT (no wall movement +
cannot meet the byte-prooff standard; tree stays clean for R05).
Lesson banked for R05 persistent geometry: submit-path work needs a
statistical-equivalence protocol (distribution + histogram + no-cluster
rule used here), and GPU-bound means convert-side savings need a GPU
fetch/fill lever to move the wall. Banked perf/fb-qOFF, perf/fb-qON.

## 2026-11-12 — R07 sizing: active lights counter (infra committed)

Mono ramqan: CNT lights=2208 over 240 frames = 9.2/frame, but the pack
caps at 8/upload — so uploads run >1/frame (matches VIS/BG ~2
passes/frame). Mean per upload needs the call count (not yet
instrumented): ~480 calls → 4.6 mean (variants win); ~276 calls →
always 8 (variants dead). Next: call-count + distribution, then
0/1/2/4/8-capacity shader variants ONLY if the distribution spreads
AND the bottleneck is vertex-side (fill-bound scenes gain nothing).
Counter kept as infra (tdraws pattern). Framedump proof required for
any shader change (byte MATCH expected: same per-light math, zeros
already contribute nothing).

## Build lesson: flag defs must precede the CFLAGS := expansion

SHADER_LIT2's first wiring silently built identical binaries (ON and
OFF both 3526924, no lit2 symbols): SHDLIT2_CFLAGS was defined at
Makefile line ~514 but CFLAGS is a := assignment expanded at line ~181,
so the per-variant -D never reached the compile. Same incident class
as the three stale-object/dflags events. Rule restated: every
*_CFLAGS used inside CFLAGS must be DEFINED above the CFLAGS :=
line (like PERF/BUCKET/CHE at lines 30-66); the stamp entry alone does
not carry the flag into the command. Verified after fix by nm (ON has
the variant symbols, OFF does not) + differing binary sizes.

## 2026-11-12 — R07 2-light shader variant (REVERTED: correct, no wall movement)

45% of uploads pack <=2 lights (le2=232/510; bimodal: <=2 or 5-8, none
3-4), so a 2-cap program skips 6 unrolled vertex blocks on nearly half
the passes. Built render_c3d_lit2.v.pica (identical math on lights[0..1],
same uniform layout), per-pass selection with all 7 bind sites honoring
it, guarded fallback when the program fails to load.
HARDWARE: wall flat (off 50089/50466 vs on 50438/50089 — GPU-bound
frame absorbs vertex-ALU savings; vertex shading is not the long pole
here). lightsw=463 confirms the 2-cap path executed (switch on every
pass change). Pixels: 78% differ <=4/channel, zero-mean, identical
bright counts — the TF04-A rounding signature exactly (different
compiled code => ULP scheduling differences, accumulated through
blending). Correct but unprovable-bytes + no wall movement.
REVERTED (machinery removed; distribution counters kept as infra).
Process notes: (1) a same-day Makefile edit placed SHDLIT2_CFLAGS after
the := CFLAGS expansion, silently building identical binaries — caught
by nm/size comparison before measuring (see lesson entry); always
verify new flags reach the compile. (2) Cache-bust every console HTTP
read (?t=) and require the checksum line + framedump growth before
trusting any number; plain reads served stale content for hours and
nearly invalidated this verdict. (3) An early edit left a self-recursive
bind helper (infinite recursion on every non-2-cap bind); caught by
re-reading the diff, would have presented as a hang. Re-read every
helper after restructuring it.
Banked perf/fb-r07Xoff, perf/fb-r07Xon(+bad, +b), perf/fb-r07c, perf/fb-lc*.

## 2026-11-12 — Phase 6 close-out (partial) + final stack

R1 FPSCR: already satisfied in-tree — vfp_set_fz_dn() (main_3ds.c:137,
with read-back + full rationale) + vfp_thread_fz_dn() at music thread
entry (music_3ds.c:310), libctru initThreadVars verified 2026-10-25
(mov r1,#0x03000000; vmsr fpscr,r1). No action; pin stays.
Buckets default: DECLINED (A/B/A proved wall-neutral on a GPU-bound
frame; verify clean at 0 mismatches; FSBEGIN scare refuted as
transient). Stays flag-only. Revisit only if CPU headroom matters again.
First capture (top-15 profile): OPEN. No capture tooling beyond
PERF_CAPTURE stages + atm counters; corpus stays provisional. A real
top-15 needs either a sampling profiler on-device or manual stage
accounting (current stages cover it coarsely: BULLPRIM 9.4, OPQMODEL
5.2 w/buckets, TRANS 6.6, BG 6.1, FSBEGIN ~5.4, ENEMIES 5.6).
GPU-bound decision tree (evidence-banked): vertex ALU ruled out (R07
flat despite 45% eligible passes), CPU ruled out (buckets -5.5ms CPU,
wall flat). Remaining levers in order: (1) R04 compact verts (36B->24B,
-33% fetch bandwidth — untested, top candidate); (2) fill/overdraw
coverage analysis (needs a covered-pixels measure; visual risk);
(3) TF03 overlap (prep-before-wait has ~0 headroom — CPU never idles;
the sim/render split is the real variant, large project); (4) R22
quality budgets only after a preserved-fidelity win.
Overnight tally: keepers opt1/opt2 (sfx), opt4 (color LUT), opt6
(SoundInfo), R03 (index units); infra: trans counters, tdraws/tverts,
light distribution, fidelity path; reverted: stereo replay (pixels),
trans single-pass x2 (no movement), indexed quad submit (rounding),
-O3/LTO (flat), 2-cap shader (correct, no wall). Deliverable ~50ms mono
OG (was 113.4). Branch unpushed for review.

## 2026-11-12 — R04 compact vertices (REVERTED: correct, rounding, no wall)

36B->24B (float3 + ubyte4 + float2), convert writes raw bytes (LUT
deleted), PICA UNSIGNED_BYTE loaders x5, shader scales by 1/255 via
compile-time const (T2K idiom; uniform layout untouched).
HARDWARE wall: off 50491 -> on 50356 (-135us, noise). tverts identical
(same verts, fewer bytes — counter counts verts not bytes).
Pixels: same uniform +-few-LSB signature as TF04-A/R07 (59/20/15/3/2/0.1):
expected, since (float)b/255.0f (CPU division) vs b*0.0039215686f (GPU
multiply) can differ by 1 ULP, accumulated through translucency
blending. Mechanism validated, visually identical, but unprovable by
bytes and wall-flat. REVERTED. The fetch-vs-fill question stays open:
this result is consistent with fill-bound (fetch -33% changed nothing)
but does not prove it, as convert-side CPU savings also backfill.
Banked perf/fb-r04off, perf/fb-r04on.

## 2026-11-12 — Stack assessment: what remains (GPU-bound frame)

TF03 prep-before-wait: DEFERRED with rationale, not skipped. The
mechanism only helps if CPU idles while the GPU works; ours does not —
MAINGAME runs back-to-back into the next FrameBegin and the 5.4ms
FSBEGIN wait is GPU overhang, not CPU idle. Moving prep earlier needs
the full sim/render split (snapshot tick N+1 while rendering N), a
multi-day project with its own fidelity A/B per module. Revisit only
with a measured idle window or as the vehicle for a proven-big win.
R18 textures: open, unmeasured. If the working set lives in linear
(not VRAM), every textured fragment crosses the shared FCRAM bus
against CPU+vertex+command traffic. Needs a residency/texture-traffic
measure before any move (VRAM is 6 MiB; the set may not fit).
Fill/overdraw: open, unmeasured. TRANS is 10 draws/2340 verts for
6.6ms CPU-side; the GPU fill cost is unknown (no timer). Discriminator
for a future run: per-draw coverage estimate (scissor rects are
already portal-tight; look for fullscreen translucent layers first).
R22 quality budgets: parked until a preserved-fidelity GPU win exists.
Phase 6: R1 present, buckets flag-only (final), corpus provisional
(no capture path), log current, gates green, branch unpushed.
Net of the night: keepers opt1/opt2/opt4/opt6/R03 (-61ms wall, -768KiB
linear); reverted stereo-replay, trans single-pass x2, indexed quads,
-O3/LTO, 2-cap shaders, compact verts (all exact-or-equivalent, none
moved the GPU-bound wall). Next first light: R04-style fetch work only
if a fetch-bound scene is found; otherwise fill/overdraw or TF03-split.

## 2026-11-12 — r22fx push: FX-density-only toggle build for visual test

Tip `d4e4e2c` (opt/r22-toggles; light-cap toggle removed, `g_fx_full_density`
only, default on). Fresh out-of-tree build in /tmp/r22-wt:
`BUILD=/tmp/r22-fx EDITION=remaster RENDERER=citro3d SLIM=1 FRAMEDUMP=1`
(no ATM_PROFILING — interactive visual-test shape). Gates ALL PASS.
Output `r22fx.3dsx`: 3537076 bytes,
sha256 `0f10cb7c033f47aed3bd68d53b90215edcc80894ac839e65c4457b00608f18e1`.
HARDWARE (old3ds-lan167, firmware `9f3632fe047c+dirty` — unchanged, comparisons
valid; lid closed_blanked): reboot 394→395 proven (session change +
recovery_ready=yes) to restore stub, `3dslink -a 192.168.4.167 r22fx.3dsx`,
receipt `transfers.log: file=r22fx.3dsx status=ok bytes_out=3537076`
(matches size; 3dslink % display ignored). Post-push discover timeout =
stub exited after chainload, game running for operator FX test.
SD hygiene: `atm del` on prior-iteration r22base/r22toggle-interactive
returned deleted=false with zero staged artifacts listed — no ATM-visible
staged payloads to rotate; the running r22fx build is protected by rule.
Iteration leaves the console running the game intentionally (operator visual
test in progress); next push reboots first per the chainload cycle.

## 2026-11-12 — r22fx "full bright" report: diagnosed, not a bug

Operator reported everything full-bright on r22fx vs the previous
interactive build. Delta old→new is exactly d4e4e2c (light-cap toggle
removal); render path provably neutral when the cap was off (variable
equaled GPU_MAX_LIGHTS=8; same pack, same zerofill, same count uniform;
shared shader header identical in both binaries). Operator confirmed they had switched "Light cap (2 max)" ON in the old
build — then corrected the diagnosis: what reads as fullbright is the
BAKED vertex lighting going flat white. Mechanism (verified, not a
guess): final color = saturate(baked + ambient + sum of packed
dynamic lights). Old build packed 2 lights; baked variation survived
below the clip ceiling. New build packs 8; baked + ambient + 8 lights
clips to 1.0 across most surfaces, flattening baked variation to
white. The baked term is still computed — it is buried and clipped.
Binary proof: render_c3d.o old-vs-new disassembly is 85/86 functions
identical, only c3d_upload_xlights differs; title.o strings differ
only by the removed g_light_cap/LightCap/"Light cap (2 max)" refs.
So the ONLY behavioral delta is 2 vs 8 packed dynamic lights.
Caveat on the standing direction: "lets not reduce the lighting" was
decided believing the cap was off; the operator's lived baseline had
it ON. Pending operator call: restore the cap toggle (their stranded
LightCap=true would take effect immediately) or keep full 8-light.

### r22fx fullbright, continued: 8-light path proven good on ramqan

Viewed banked `perf/stills-step1-mono/framedump_0001_bench_top.bmp`
(08:14 stack, 8-light pack): baked shading fully intact — dark corners,
red lava falloff, no washout. So 8 packed lights do NOT inherently
fullbright; the 2-vs-8 saturate story cannot be the whole explanation,
consistent with the operator's "more than 2 lights before". Standing
facts: the two binaries differ ONLY in the cap removal (85/86 render
funcs identical; title diff is toggle/config-key removal; config is
key-based Lua, order-safe). Unresolved fork: (a) the old run was never
actually capped (toggle flipped while exploring, "probably" uncertain —
then both builds ran 8 lights and the delta is scene/state, not code),
or (b) their scene washes out at 8 where ramqan doesn't. Next: confirm
same level/spot, DUMP their live framebuffers (needs operator ok for
the ~17 s SD stall), or A/B by re-pushing the old binary. No console
action taken; game left running for the operator.

### r22fx fullbright, live capture + A/B staged (operator-authorized autonomous)

DUMP fullbright1 on the running r22fx (PING→PONG, DUMP→OK
framedump_0001_fullbright1): twins retrieved, sizes exact
(288054/230454, BM magic), FNV-1a recomputed MATCH (top ee4e4843,
bottom 68eefb3c). Top = frozen nose-against-wall frame (textured, some
variation); bottom = death/stats screen (HULL 0, FPS 0, PRESS ANY KEY
TO CONTINUE) — the operator had died; the "fullbright" verdict was
rendered against this frozen wall frame, so a single capture cannot
separate a lighting regression from a bright death-cam wall.
A/B staged: reboot 395→396 proven (fw unchanged), pushed pre-removal
binary as r22prev.3dsx (3537228 B, transfers.log status=ok byte-exact),
PONG confirms it running. r22prev honors the stranded LightCap=true
(cap row present); r22fx ignored it. Operator replays the same spot on
both and judges. No code changed.

## 2026-11-12 — R18 texture residency: MEASURED, ABANDONED (not texture-bound)

Residency first (code read): `Tex3DS_TextureImportStdio(..., vram=false)` —
all HD textures live in linear FCRAM, fetched across the shared bus.
Footprint now measured, not guessed: ramqan holds 44 textures, resident =
peak = 4,014,080 B (~3.8 MiB), reported as R18 lines in atm_result.txt
(branch-local accounting in render_c3d.c, atm_prof_dump_r18).
Wall A/B, ramqan mono 240+30, OG `old3ds-lan167` fw 9f3632fe047c+dirty:
textured 49247/49042 (mean 49144) vs vertex-only 47832/47700 (mean 47766).
Delta −1378 us (−2.8%) — real (tight spreads, still-verified lever) but a
sliver of a ~49 ms GPU-bound frame. The VRAM move is NOT justified;
R18 abandoned, branch kept as record, nothing merged.
Method trap, banked: the first NOTEXTURE lever targeted
replay_display_list — DEAD CODE in default builds (s_dlReplay only arms
with STEREO_REPLAY=1; bench is forced_mono). Four bench runs rendered
pixel-identical stills (same FNV-1a) before the stills caught it. The live
batch dispatch in draw_render_object is the real one; a red-ambient marker
proved which binary executes. Rule: a no-delta A/B is only valid with
still-level proof the lever changed rendering.

### 2026-11-12 — Fill/overdraw: MEASURED, ABANDONED (not fill-bound)

Submit-gate A/B on ramqan mono 240+30 (`FILL_SKIP`: 0 = normal, 1 = skip
TRANS submits, 2 = skip all 3D submits; conversion/batching/counters/state
all run identically, so deltas are pure GPU draw cost; still-verified each
mode — mode 2 still is pure clear color):
  full:     48760 / 48957 (mean 48858)
  no TRANS: 47893 / 48012 (mean 47952, -906us, -1.9%)
  no draws: 46085 / 46322 (mean 46203, -2655us, -5.4%)
Opaque draws = ~1.7ms; translucency = ~0.9ms; floor (CPU + fixed GPU) =
~46.2ms. Total GPU draw cost 2.7ms of a 48.9ms frame: the frame is not
fill-bound on ramqan (consistent with the earlier fill-cuts ruling).
Branch kept as record, nothing merged.
Method note: the submit gate lives in the live batch dispatch in
draw_render_object; the replay path is disarmed by default and ungated
(requires STEREO_REPLAY off, i.e. bench shape). Q3 ring confirms zero
ortho draws inside the TRANS window, so mode 1 skips exactly translucent
3D draws.

### 2026-11-12 — Fill/overdraw submit-gate A/B (ABANDONED — not fill-bound)

`FILL_SKIP` (branch-local, `opt/fill-overdraw`): 0 = normal, 1 = skip
TRANS-phase C3D_DrawArrays, 2 = skip all 3D submits. Conversion,
batching, counters, TexEnv programming all run identically, so deltas
are pure GPU draw cost. Stills verify each mode (mode 2 = pure clear
color; mode 1 stills differ from baseline by the removed translucency;
F1b reproduces F1a's hash exactly).
  full:     48760 / 48957 (mean 48858)
  no TRANS: 47893 / 48012 (mean 47952, -906us, -1.9%)
  no draws: 46085 / 46322 (mean 46203, -2655us, -5.4%)
Opaque ~= 1.7ms, translucency ~= 0.9ms; floor (CPU + fixed GPU) ~= 46.2ms.
Total GPU draw cost 2.7ms of 48.9ms: not fill-bound on ramqan (agrees
with the earlier fill-cuts ruling). Branch kept as record, nothing merged.
Consistency picture: wall = max(CPU ~= 46.2, GPU ~= 48.9); buckets -5.5ms
CPU left the wall flat, draw removal moves it 1:1. R05 ceiling follows:
CPU-side convert savings cannot move a wall pinned by unchanged GPU work.

## 2026-11-13 — frame-interp M0: native baseline banked (HARDWARE)

Branch `experimental/frame-interp` cut from `perf/low-risk-profiling` @
`f777e6d`; pre-existing dirty work carried over untouched (M .gitignore,
AGENTS.md, atm_prof.h/.c counter names, docs; untracked perf/BLOCKED-*).
Source-map verified with corrections: MainGameRender=oct2.c:5099 (spec
says 5017), MainGame=oct2.c:5404 (spec says 5276); all other spec
anchors (interp hooks, quat helpers, TransExe, water, texture anim,
title Visuals) confirm. rt64-3ds @ c449a78 present (matches spec).
Toolchain devkitARM 15.2.0. Gates ALL PASS.
Prototype audit: interp_3ds.c overwrites live CurrentCamera/Models[].Mat
and restores after — the exact pattern the new spec §4 forbids; no
generations; interp_reset() exists but is NEVER called (no cut/load
invalidation). M1 must replace overwrite-restore with sidecar snapshots.
Build: BUILD=/tmp/fi-base EDITION=remaster RENDERER=citro3d SLIM=1
ATM_PROFILING=1 FRAMEDUMP=1, binary fi-base-m0.3dsx (3533048 B,
sha256 a587cf7f…cb965). Console old3ds-lan167, fw 9f3632fe047c+dirty
(unchanged), reboot 399→400 proven, lid closed_blanked.
Result (perf/fb-fi-m0/atm_result.txt, sha256 fc7544a0…7d7c):
ramqan slot 19 exact, forced_mono, 240+30 frames, status=complete,
reason=frame_target, avg 50102us, min 33251, max 89203, over_budget
240/240 (budget 14084us). level_load 30361ms. draws 15734, verts
3.33M, tdraws 2411, tverts 561K, lights 2208 (9.2/frame), prim_bullets
28594, sfx_create 11. fillpix 635M (2.65M/frame), tfillpix 64.8M.
Feasibility (§12.2): 30·S+60·(R+I) with R≈45ms CPU/presentation is
~3× over budget — OG mono cannot reach 60 presentations; M5 gate is
expected to fail on OG unless paired with quality reduction or scoped
to New 3DS. Recorded as the prediction to beat, not a stop.
Method lessons: (1) `atm retrieve` does NOT sync /3ds/atm_result.txt —
fetch it via HTTP /3ds/atm_result.txt directly (four runs "lost" before
this was found; 61 synced artifacts were all stale dumps/shots).
(2) Rosalina GDB does not deliver process-exit events over
extended-remote — a `continue` past teardown hangs forever; always
detach before the process exits. (3) GDB attach pid rotates per
chainload; re-list processes each run. Infra: GDB backtrace works with
the exact .elf (frame_target + full counters read live from g_reason /
g_stats in run #4). Console left listening, boot 400, no app running.
## 2026-11-13 — M1 validated + R08 landed (HARDWARE)

M1: FRAME_INTERP_PROBE + FIDELITY_DUMP + ATM bench, ramqan mono 240+30:
`CNT: interp_dirty=0` over 240 double-rendered frames — repeated
rendering does not move authoritative gameplay hash (ships/enemies/
primaries/secondaries). Purity proof GREEN. `scratch_peak_verts=26076`
(22% of the 4 MiB arena; 3× headroom for extra presents).
Floor cost of an identical second traversal: 79.3ms vs 50.1ms native
(+58%). Result: perf/fb-fi-m1-probe/atm_result.txt (sha
7da31dbe…), stills banked (top FNV f1be9c5b).
R08 (bind-before-cache): control avg 50435us vs fixed 50447us (+0.02%,
noise — no wall claim, expected on a GPU-bound frame); top-frame FNV
f1be9c5b == f1be9c5b (byte-identical); fidelity hashes identical
(0be4389e… tick 60 across all builds). Landed as hygiene: removes one
redundant GPU register write per cache-hit bind. Results:
perf/fb-r08ctl/ (ctl 2a7e6471…), perf/fb-r08fix/.
Method incident: boot 401 never started the userland stub (17491
closed while ATM healthy) — one normal-cycle reboot (→402) restored
it. Intermittent autoboot, not a wedge; no evidence lost.

## 2026-11-13 — R11 fullscreen-branch dead math: MEASURED, LANDED as hygiene (HARDWARE)

Per-group magnified projection (~15 flops + 4 fdivs) computed then
discarded by the fullscreen override for every mono main-camera group
(visi.c). Hoisted the fullscreen check up front; the R3-pinned decision
below stays verbatim (condition duplicated, not moved) and the new check
carries the same `@vfp-exempt R2, R3` pin (gate text-scan flags the
duplicated `(long)` casts/comparison as new; same reviewed decision).
Gates ALL PASS, baselined count unchanged at 334.
A/B, ramqan mono 240+30, same tree otherwise: control avg 50445us vs
fixed 50447us (+2us, noise — no wall claim); top-frame FNV f1be9c5b ==
f1be9c5b (byte-identical); fidelity tick-60 0be4389e identical across
all builds to date. Landed: dead math removed, zero behavioral delta.
Results: perf/fb-r11ctl/ (ctl a3a03400…), perf/fb-r11fix/ (fixed 5baedf6d…).

## 2026-11-13 — R06 RealLight restore skip: MEASURED, LANDED as hygiene (HARDWARE)

XLightMxloadHeader (static MX) spent the MDLSTATIC 0.9ms partly on a
per-frame originalVerts→live color restore whose computed-light half
is compiled out under GPU_LIGHTING. Audit: no remaining per-frame
writer of computed colors into static-MX live verts (ambient/stealth
set render_color_blend globals and return; InterpFrames morphs and
ENVMxa UV writes are MXA-only; mxload writes both copies at load;
else-if dispatch excludes a second writer per frame). Cut: restore
loop guarded by `#ifndef GPU_LIGHTING`; lock/unlock retained.
A/B, ramqan mono 240+30: control 50448us vs fixed 50457us (+9us,
noise — the 0.9ms stage is not wall-visible); top-frame FNV
f1be9c5b == f1be9c5b (byte-identical); fidelity tick-60 0be4389e
identical. Landed: redundant per-frame memcpy removed, zero pixel
delta. Results: perf/fb-r06ctl/, perf/fb-r06fix/ (fixed 68a32257…).
MXA variant (XLightMxaloadHeader) deliberately untouched — MDLLIGHT
stage reads 0, nothing to gain there.


## 2026-11-13 — Combat bench v1: BUILT, DETERMINISTIC, BASELINED (HARDWARE)

ATM_BENCH_FIRE=1 holds primary fire for the whole bench (input_3ds.c,
same discipline as the AUTOTEST hold but without the sweep apparatus;
Makefile refuses FIRE without PROFILING). Ramqan mono 240+30.
Firing run: complete, frame_target, avg 50714us (+~250us, +0.5% vs
no-fire 50457 — the combat load is small on this scene), max 106979
(impact spikes), prim_bullets 29544 (+950 — live mean +4/frame; bullets
die fast against the spawn room).
Determinism: THREE identical-binary runs → byte-identical fidelity
files (tick 60/120/180/240 all match). Gameplay proof is solid.
Caveat (open): warmup stills are NOT boot-stable under fire —
3c61e9fb / e1bcdc3c / a3465850 for identical sim state. Pixel diff is
a subtle bottom-half shimmer (20% of bytes, mean delta 3.7 LSB;
474 pixels >32). Gameplay hash excludes particles, so the variance
source is visual-only (particle phase or pointer-derived seed are
prime suspects — NOT yet root-caused).
Methodology consequence: combat-bench A/B verdicts rest on
fidelity+wall+counters; render pixel-proof stays on the no-fire bench
(byte-stable f1be9c5b x6 runs). Division of labor: no-fire bench for
render changes, combat bench for gameplay-side (bullet/AI/collision)
changes. Results: perf/fb-cmb1/ (firefid 42c45a66…, stills vary).
Next: use it — bullet/AI opts are measurable for the first time.

## 2026-11-13 — FSBEGIN wait split: MEASURED (HARDWARE)

New PERF_S_FSBWAIT (C3D_FrameBegin only) + PERF_S_FSBSETUP (clear +
state setup) stages, appended to the perf enum (old ids stable).
Ramqan mono 240+30, PERF_CAPTURE bench build: FSBEGIN total 11.34ms
avg, of which FrameBegin wait 11.28ms and setup 0.03ms; FSEND 0.00ms.
So 99.7% of frame-begin is GPU/queue wait, CPU setup is free — there
is nothing to optimize inside FSBeginScene, and the wait is overhang,
not work. Quantified TF03 prize: at most ~11ms/frame of overlap for a
second core on OG; the NO-GO stands (preemptible syscore, 1-frame
latency is a product call, no thread-safe CPU write target exists).
R01 frame-begin-wait gap now filled. (PERF build wall 62.9ms avg —
diag overhead, not a regression; same binary shape otherwise.)
Results: perf/fb-fsbeg/ pending (atm_result only; perf file is
append-mode and shared).

## 2026-11-13 — Stereo breakdown + opt5b ceiling: MEASURED (HARDWARE)

Stereo bench (ATM_BENCH_STEREO=1 + PERF_CAPTURE), ramqan mono scene:
stereo wall 90.7ms avg vs 50.4 mono (+40ms second eye). Per frame:
CAM 56.5 (both eye traversals), VIS 1.47 x3.8 calls = 5.5ms,
OPAQUE 5.88 x3.8 = 22ms, TRANS 3.63 x3.8 = 13.6ms, MDLSTATIC 1.7ms.
Visibility walk is ~1.5ms/eye; conversion+submit is the bulk.
opt5b-as-union-walk (single center VIS walk, draw union per eye)
ceiling is therefore ~1.5ms on a 90ms frame — below the land bar for
touching portal/visibility code that has open black-void bugs on
Remaster levels. The ~5ms variant is replay-fixed (share converted
geometry, fix inset/HUD omission), i.e. re-implementing reverted opt5
with the missile-PIP/viewport scoping fix — a multi-day stereo-only
project. Parked pending stereo-priority call: the loop benches and
ships forced_mono; if stereo isn't the played mode, neither variant
has any user-visible effect. Results: stereo atm_result + perf dump
banked with the run (90.7ms wall).

## 2026-11-13 — opt5b forensics hang: forensics-family builds wedge pre-bench (HARDWARE)

Pure-verify 120f liveness probe (current tree, MEMO_VERIFY=1, no
FORENSICS) completes fresh: target_frames=120, status=complete,
window_ms=31428, memo_mismatch=33707. Tree runs; hang is specific to
forensics-family binaries (st-verify-forensic boots 405/406,
st-forensic-num boots 407/409 — all hang; result file stays stale,
boot log frozen at 1679B init prefix, both LCDs black).
GDB (exact .elf, attach pid 43 = 3dsx_app, detached after): render
thread in C3D_FrameBegin -> gxCmdQueueWait -> gspWaitForAnyEvent from
DisplayTitle -> FSBeginScene; gsp event thread parked; music thread in
normal sleep. No crash dump, no errdisp, process alive. Forensics code
runs only on bench memo mismatch, so it cannot execute pre-hang —
suspects: link-layout shift exposing latent UB, stage-name limitation,
or boot flakiness. Test A (identical binary re-pushed boot 409) still
hangs. Next: Test B (same bytes, new stage name) + staged-payload
cleanup (never did the per-iteration `atm del`; ~15 stale .3dsx on SD).
Gates: ALL PASS (R2 pin + cast removal for the 5 one-shot counter adds).
Boot 408/409, firmware 9f3632fe047c+dirty throughout. Evidence:
perf/fb-forensic-diag/ (unacked diag shots/dumps — most are historical).

## 2026-11-13 — opt5b root cause: RenderBufs key aliasing (HARDWARE-PROVEN)

First-mismatch decode (adds-only forensics, boot 413): sub=0,
54 words (6 verts), fi=0, exp=1170796175 (~6434.0), got=1155050150
(~1739.0) — wholly different first words, not a near-miss.
Code cause: DisplayGroupClippedPolys(&RenderBufs[2], group)
(oct2.c:6352, per-visible-group TRANS loop) refills the SAME
RENDEROBJECT per group with texgroups restarting at 0
(numTextureGroups=0 per PolyDispGroupClipped fill). The memo key
(ro,sub) is identical across groups, so eye-L's later groups
overwrite earlier groups' records and eye-R's lookups hit the wrong
group's bytes: 78052/106018 systematic mismatches, and the submit
path drew the wrong group's verts (the stills difference). texgroup
ranges are never rewritten and InterpFrames is pure, so window and
mutation theories are out; arena peak 26k << 116k max, so no
wrap/clobber either.
Fix (this tree): source-content hash in the memo (FNV-1a over the
LVERTEX window at record; hit only on hash match, else safe
live-convert miss) + verify-mismatch submits the LIVE range.
Host test extended with the alias-regression case; all green.
A/B next: fixed-memo vs pure-verify (wall, framedump FNV, fidelity).

## 2026-11-13 — opt5b epoch+invalidation: aliasing fixed, UV-step isolated (HARDWARE)

Epoch key (pass,group,resume) killed the shared-slot aliasing: mismatch
78052 -> 12301, hits 27375 -> 40267, wall -6.1% vs nomemo. Forensics
decode of the residue: first mismatch tv word (uv step +1/8 = one atlas
row) on a stable ro — texture animation advancing mid-eye-L (guarded
once per frame, but AFTER eye-L already drew earlier batches).
Fix: smemo_invalidate() on every real animation step (2 sites in
mload.c/mxload.c + water integrate) so pre-step records miss safely
and post-step records hit. With invalidation: mismatch 12301 -> 10344,
remaining first-mismatch is a color[0] divergence on a stable batch
(mechanism open; candidates: per-eye color baking or unguarded RGB
morph — verify builds submit live on mismatch so pixels stay correct).
Ship builds (no verify) submit recorded bytes: correct except animated
batches show a one-step UV lag in the right eye (~9% of frame-15
pixels, lava-flow-type surfaces), gameplay identical (fidelity match).
Landing: default OFF pending R22 human judgment; volatile-flag follow-up
scoped (bypass animated fills) if R22 rejects the lag.

## 2026-11-13 — opt5b VERDICT: LAND (default OFF) — epoch memo + coalesced submit (HARDWARE)

A/B (ramqan stereo bench, Old 3DS, 240+30 frames, FRAMEDUMP frame-15 + fidelity):
- Wall: memo 86159us avg vs nomemo 90688us (-4529us, -5.0%). window 40381 vs 41357.
- Gameplay: fidelity tick files byte-identical across nomemo/verify/fix/meship builds.
- Pixels: bottom HUD identical; top/top_right 9.2% differ, confined to animated lava/water-type surfaces (one UV step lag in the right eye).
- Correctness: aliasing eliminated by construction (pass,group,resume epoch + per-frame invalidation on texture-anim steps/water integrate); verify builds confirm mismatch ~0 for keyed batches; 12k->10k residual mismatches are same-fill UV-advancing batches handled via live submit in verify builds.
- Gates ALL PASS; host stereomemo test 11/11 (incl. alias regression + invalidation cases).

What shipped: stereomemo epoch keys, 7 fill-site stamps, mid-frame invalidation, coalesced resubmit, mismatch-submits-live, forensics counters (debug), host tests. STEREO_REPLAY stays default OFF (no behavior change); enable for +5% stereo wall with the documented one-step UV lag on animated surfaces in eye-R. Follow-ups if R22 rejects the lag: volatile-flag bypass for animated fills (pixel-perfect, keeps static savings), or pre-eye animation sweep (needs trigger-timing audit). Unexplained, worked around: big-forensics builds (snprintf/fb blocks) wedge first FrameBegin 6/6; adds-only shape runs 4/4 — layout-sensitive, mechanism open.

## 2026-11-13 — CORRECTION: file-fidelity comparisons were vacuous (all runs)

The sdmc:/3ds/forsaken_fidelity.txt compared as "identical" across every
bench (nomemo/verify/fix/meship*) is STALE, not proof: FIDELITY_CHECKPOINTS
was never enabled in bench builds (FIDCK= empty in every .dflags.stamp),
so fid_out() never ran and no bench ever truncated/rewrote the file
(the 60/120/180/240 FIDC lines predate this session's benches). Every
"fidelity identical" claim to date is therefore VOID and is retracted.
What REMAINS valid: interp_dirty=0 (in-memory per-tick hash, live every
run — repeated renders never move gameplay state), wall numbers, stills
FNV/bytes, counters. Repair: bench builds from here on pass
FIDELITY_CHECKPOINTS=1 FIDELITY_CHECKPOINT_TICKS=60 so every run writes
fresh checkpoints; nomemo-vs-meship epoch A/B will be re-proven on
fresh files. Lesson: a proof file with identical content across runs
is a stale-file suspect until a run is seen to CHANGE it — require the
checksum line + framedump growth (per 11/12 lesson) AND file mtime/size
movement before trusting any SD artifact.

## 2026-11-13 — opt5b LANDED (default OFF): epoch memo + coalesced submit (HARDWARE)

Repaired A/B (checkpointed builds, fresh fidelity both sides):
- Wall: meship5 86284 vs nomemo-fid 88866 (-2.6%); meship4 86159, meship3 84406 vs nomemo 90535/90688 (-5 to -7%). Band: -3 to -7% stereo wall on ramqan/Old 3DS.
- Fidelity: FIDC tick files byte-identical nomemo vs memo (fresh files both sides — the pre-checkpoint "identical" claims were retracted as stale-file vacuous; this one is real).
- Stills: HUD identical; static 3D byte-identical (verify oracle: 0 aliasing mismatches); animated lava/water-type surfaces differ one UV step in eye-R (~9% frame-15 pixels, coherent lava-channel bands).
- Correctness machinery: epoch key (pass,group,resume) kills RenderBufs aliasing (78k->12k mismatches); per-eye texture-advance invalidation; coalesced resubmit (draws 121719->30659); mismatch-submits-live backstop; 11 host cases; gates clean.
Land: STEREO_REPLAY stays default 0 (no behavior change). Opt-in gives +~5% stereo wall with the documented one-step UV lag on animated surfaces (binocular shimmer candidate). R22 human A/B judges; volatile-flag bypass (live-convert animated fills, pixel-perfect) is the scoped follow-up if rejected.
FNV method fix: device hashes capture-order pixels; BMP twins store bottom-up rows — verify with row-flipped FNV, never raw bytes. (Earlier .bmp.log pixel diffs remain valid: both sides same orientation.)
Open: big-forensics builds wedge first FrameBegin (adds-only shape runs) — mechanism unknown, worked around, not blocking ship shape.

## 2026-11-13 — R02 stress: overflow defect REPRODUCED (HARDWARE, debug-only)

STRESS_SCRATCH_VERTS=2048 build (fresh dir): peak 2046 (splits fire
constantly), 240 frames COMPLETE, no hang, no crash, fidelity
byte-identical (gameplay untouched), draws counted normally — but
frame-15 still is BLACK with one reddish blob vs the full nomemo
scene (verified fresh twins via row-flipped FNV match to device log).
The mid-frame FrameSplit + cursor-reset path does NOT correctly resume:
either GPU state is lost across the split (targets? lights? TexEnv?)
or the GPU reads overwritten verts (no completion wait). Undetermined
which; both fit a black frame. Impact: ZERO in practice — real peaks
are 4.8k-26k vs 116k cap (4-20x headroom), never reached in any bench.
R02 verdict: harness banked (STRESS_SCRATCH_VERTS knob + stamp tracking),
high-water monitor already live (s_scratchPeak), defect characterized
with exact repro. FIX DEFERRED by design: double-buffer (8 MiB linear,
too tight) vs early-frame-end (pacing change) both need their own
review; only revisit alongside an actual arena change. Never ship the
stress knob (kills batching + memo; verified 86.6ms wall purely from
split overhead, not a perf vehicle).

## 2026-11-13 — R05 scope decision: DESIGN BANKED, implementation deferred past interp (HARDWARE-grounded)

Fermi from measured evidence: memo skips 38% of eye-R conversion for
-6ms wall => eye-R conversion ~= 16ms, both eyes ~= 32ms/frame on
ramqan stereo. R05 (convert once, resubmit persistent buffers) attacks
all of it for static content, vs the memo's eye-R-only share.
Scene hunt says the pole scales with verts+draws (not fill/texture/ALU),
so a vertex-path lever is directionally right.
Why deferred, not built: (1) full persistence needs per-batch
static-proofs (deform/UV-anim/light-bake/envmap freeze checks),
lifetime management across level load/unload (hunk interplay), and a
submit path for persistent buffers — multi-day build with fidelity
risk, while interp M2-M6 (the headline feature) is still pending;
(2) the memo already banks the eye-R half of the prize with zero
lifetime risk; (3) the remaining eye-L-static share needs a static-fraction
measurement first (per-system vert counters — one bench build).
Minimal viable R05 when resumed: persistent buffers for MLOADHEADER
static walls only (provably non-deforming; UV anims already guarded +
invalidated; light bake idempotent under GPU_LIGHTING), submit via
BufInfo on persistent offsets, free on level unload; A/B wall+stills+
fidelity vs nomemo. Volatile-flag follow-up (animated fills bypass)
from opt5b composes with it: static hits persistent, dynamic converts.
R14: spec text missing from tree (no definition found in repo, gates,
or log beyond the R07/R14/R18 grouping). NOT implementable blind —
surfaced as blocked pending spec text; not attempted.

## 2026-11-13 — interp M2-M6 verdict: NOT STARTED (blocked, design banked)

M0 feasibility stands and hardened with current numbers: stereo R~90ms,
mono R~50ms per presented frame; interp-60 mono needs R<=~12ms (4x
speedup). Memo (-6%) + full-R05 estimate (-10-20%) still leave ~2x gap
on OG. M5 budget gate fails on OG by construction; New 3DS has no
capture here to prove headroom. Building M2-M6 now would be weeks of
invasive scheduler/snapshot surgery toward a known-failing gate.
Prototype preserved: FRAME_INTERP=1 links clean against today's tree
(verified 2026-11-13, no bitrot); all interp flags intact, default off.
M2 design (when approved): fixed-30 endpoint-only mode — (1) input
latch (bounded queue, consume-once) in input path, (2) fixed-step
accumulator on svcGetSystemTick (never framelag), catch-up cap 4,
timestamp-safe Native handoff, (3) framelag pinned at 1/30s + consumer
audit, (4) pause/HOME/level clock rebase + history reseed, (5) overload
handoff without dup/loss. Validation: fidelity-identical trajectories
at equal simulated durations + input-edge + pause/resume/load/overload
matrix. Cost 3-5 days + 4-6 hardware cycles; flag-gated, default off.
M3-M6 queued behind (a) M2, (b) R05-level R reductions, (c) New-3DS
capture. R22 human A/B covers LANDED work (memo on/off stereo feel,
overall) + interp prototype as flag-only curiosity (no target to hit).
R14: still no spec text in tree/gates/log — remains blocked; not attempted.

## 2026-11-13 — Audit remediation (post adversarial review)

Independent audit findings F1-F12 addressed as follows (forward commits
only — a97fa18 is already pushed; rewriting it would trade a review
problem for a history-integrity problem, so the split discipline
applies from here, not retroactively):
- F2/F3/F5 CLOSED (3bf7682): audit_memo_stamps gate in check.sh fails
  on any unstamped shared fill or new fill-shaped function (pass on
  tree, fail on stamp-deleted copy, both verified); probe-chain
  invariant stated in stereomemo.c; stereomemo.h no longer claims
  unconditional pixel-identity (animated UV lag documented).
- F6 LOUD (9ba7dd5): scratch_splits counter. Stress run: 3196 splits
  (black-frame path exercised, now visible in every result file).
  Control memo run: scratch_splits=0, hits 34368, wall 85724us,
  fidelity byte-identical to no-memo — overflow path untouched in
  real scenes, and any future bench that touches it is self-flagging.
- F9 SUPERSESSION (this entry): on this branch, the keyed memo
  supersedes master's display-list replay (replay draws the left-eye
  visible set in the right eye — proven wrong, log 2026-11-11 opt5).
  Master's replay arming must be retired, not merged, wherever the
  branches join. R04 24B vertices (master) are NOT merged here: solo
  wall was noise; evaluate only composed with R05 persistent buffers
  under the audit validation protocol (n>=5, A/B/A, tail-capable
  histogram, 3 scenes). MODEL_BUCKET: default-on on master, flag-only
  (benched 0) here — wall-neutral on GPU-bound frames; do not flip
  defaults without the same protocol. Neither branch merges into the
  other without that protocol run first.
- F12: key text evidence (atm_result + fidelity for the A/B pairs and
  stress/defend2 runs) committed under perf/fb-*; BMP twins remain
  local (gitignored) and are reproducible from the banked build
  commands + row-flipped FNV hashes in framedump.log.
- NOT done, deliberately: BatchId-by-construction refactor (gate
  closes the failure mode mechanically at lower risk); FLUSH_BATCH
  macro dedup and R05 build (queued behind R22/human review and the
  validation protocol); interp M2 (unchanged verdict).

## 2026-11-13 — R22 verdict: ACCEPTED — stereo memo default-on (HUMAN + HARDWARE)

Owner played the memo-ON playable build (r22-memo-on.3dsx, pushed via
3dslink, chainload-proven) and judged the one-step UV lag on animated
surfaces "fine". Per the pre-agreed mapping, STEREO_REPLAY ?= 1 is now
the Makefile default (opt-out preserved), flag-matrix and stereomemo.h
updated to match. Default-path hardware verification run follows in
this entry's commit (bench built WITHOUT an explicit STEREO_REPLAY
flag must show memo hits >0 and scratch_splits=0).
Default-path verification (bench built with NO explicit STEREO_REPLAY
flag; stamp STREP=1, -DSTEREO_REPLAY in compile line): memo_hit=34368,
memo_miss=71654, scratch_splits=0, avg 85587us, fidelity checkpoints
byte-identical to the no-memo reference (594bab3c @240). Default-on is
live and proven; opt-out STEREO_REPLAY=0 unchanged.

## 2026-11-14 — ALERT: defend2 memo build wedges in FrameBegin (HARDWARE, deterministic)

Validation protocol caught what ramqan-only testing missed: defend2 +
STEREO_REPLAY=1 hangs twice at the same point (status=started forever;
GDB exact-ELF: render thread in C3D_FrameBegin -> gxCmdQueueWait ->
gspWaitForAnyEvent from MainGameRender; music/ndsp threads normal; no
crash dump). Defend2 no-memo completes (34166us). Ramqan memo has 10+
clean runs. Scene x flag interaction, mechanism unknown: recorded-range
submit bounds, per-eye batch-split divergence, and the layout-wedge
family are all live candidates. DEFAULT-ON (46d7e33) IS SUSPECT until
this is bisected — ramqan-only validation was insufficient, exactly
the audit F4 failure mode. Bisect order: MEMO_VERIFY defend2, then
lookup-submit bounds instrumentation.

## 2026-11-14 — CRITICAL FIX: uninitialized tg_memo arrays wedged GPU on defend2 (HARDWARE)

Validation protocol (interleaved, checkpointed, 3 scenes) caught a
ship-blocking defect ramqan-only testing missed: defend2 + memo hung
deterministically in C3D_FrameBegin (GPU queue wait; ship, verify, and
force-miss builds all hung; mono and no-memo completed). Bisect:
MEMO_NO_COALESCE completed -> coalesced submit implicated. Root cause:
tg_memo_off/cnt were UNINITIALIZED stack arrays; ortho groups never
set them, yet the LOOKUP flush read them — garbage counts submitted
to DrawArrays wedged the GPU queue. Ramqan's stack happened benign.
Fix: zero-initialize both arrays + !orthographic guard on the lookup
flush. Debug knobs MEMO_FORCE_MISS / MEMO_NO_COALESCE retained
(default off) with stamp tracking.
Post-fix validation (Old 3DS, 240+30, stereo):
  ramqan  no-memo median ~89.0ms (n=4, one 1.44s outlier run kept)
          memo 86006/86841 (n=5 band 85587-86841) => -3.4%, max ~equal
  defend2 34166 vs 34096 (-0.2%, hits 99.6% — scene is sim-bound)
  capship 29220 vs 28316 (-3.1%, hits 99.9%)
  scratch_splits=0 everywhere; fidelity byte-identical (ramqan).
Honest headline: memo = -3.4% on the heavy scene, ~flat where the
scene is not conversion-bound. The earlier -5..-7% band was
cross-config noise. Default-on stands (R22) WITH this fix; without
it, default-on would have shipped a defend2 hang — the protocol
earned its cost. Results banked under perf/val/.

## 2026-11-14 — R05 design gate: do not build the cache yet (analysis)

Static audit of the level-mesh conversion inputs (MLOADHEADER walls):
- XLight1Group (visi.c:1483, per visible group per frame) under
  GPU_LIGHTING restores baked baseline colors (idempotent byte-wise)
  and SKIPS the dynamic per-vertex loop (lights.c:556) — so wall
  colors are frame-stable EXCEPT: TanimUV writes (animated textures,
  per execbuf PolyAnim) and the water-effect tint section.
- Therefore persistent conversion is byte-valid for execbufs with no
  animating PolyAnim and no water tint, PROVIDED invalidation fires
  on texture steps (hooks exist: smemo_invalidate sites) and on
  water-state changes.
- Identification is the hard part: draw_render_object receives an
  opaque RENDEROBJECT*; there is no owner tag distinguishing level
  walls from model/billboard buffers, and no per-execbuf "animates"
  flag plumbed to the backend today. A cache keyed without that tag
  repeats the RenderBufs aliasing class of bug.
Required build order (do not shortcut):
  1. Owner/static tagging at load (mload/mxload set a flag on the
     RENDEROBJECT: STATIC_PERSIST_OK cleared by PolyAnim presence,
     ENV envmap, MXA deform, water tint).
  2. Static-fraction bench (counters: verts converted by owner class)
     to size the prize before allocating linear memory.
  3. Persistent linear buffers per execbuf texgroup, converted on
     first draw, submitted via BufInfo; freed on level unload (hunk
     epoch); invalidation via the existing smemo generation.
  4. Audit validation protocol (A/B/A, 3 scenes, fidelity+stills+
     splits=0) before default-on.
Estimated multi-day; the defend2 uninitialized-array wedge is the
standing lesson against rushing renderer caches. R05 remains the
next block; this gate is its entry record.

## 2026-11-14 — R05 sizing MEASURED (HARDWARE): level walls = 56% of conversion

Owner tagging landed (vertOwner at the 5 FSCreate choke points:
mload/mxload/mxaload/water/InitRenderBufs; additive field, all
backends ignore it) + vown_* counters (live conversions only).
Ramqan stereo, memo on, 240 frames:
  vown_level=2,392,302 (56.5%)  vown_shared=1,201,764 (28.4%)
  vown_model=640,701 (15.1%)    vown_mxa/other/water=0
  (submitted 6,373,692; memo hits make up the difference)
Level-wall persistence is the right target. MEMORY GATE before
building: persistent copies cost 36B/unique-vert from the linear
heap (post-texture free ~7.9MB on OG) — unique level vert count
must be measured at load before allocating anything. Next step is
that counter, not the cache.

## 2026-11-14 — R05 persist: CORRECT, mono -8.6%, stereo flat (HARDWARE)

PERSIST_VERTS=1 retains level-wall conversions in linear buffers
keyed by smemo_persist_gen (invalidate on tex-anim steps/water;
free on level-load reset). Validation:
  ramqan stereo: vown_level 2,392,302 -> 508,836 (-79% conversions),
    wall 85,724 (memo band 85.6-86.8k — flat, GPU-wait bound),
    fidelity byte-identical, splits 0, stills statistically
    equivalent (97.6% bytes within +-4 LSB; vertex-cache rounding
    class, not byte-identical)
  ramqan mono: 63,821 -> 58,316 (-8.6%) — CPU-bound there, pays
  defend2 stereo: 34,094 (clean, no wedge), capship: 28,245 (clean)
Memory: ramqan unique level verts 25,304 (~0.87MB source; retained
expanded buffers tracked in s_persistBytes, freed on level change).
Verdict: ship default-OFF. Stereo gain is nil today; mono gain is
real but the stills are LSB-equivalent rather than byte-identical —
the same class R04/TF04-A were reverted for. Flipping the default
is a visual-equivalence judgment for the human session, with this
evidence attached. If accepted, flip is one line (PERSIST_VERTS ?= 1).

## 2026-11-14 — Autonomous queue exhausted; human-session list

Done autonomously since R22: validation protocol (caught + fixed the
defend2 GPU wedge), audit F2/F3/F5/F6/F9/F12, R05 sizing + persist
cache (default off). Capship stills fetched + hash-verified
(32f5538f/6a4b257b/6c48ae2b MATCH) under perf/fb-capship/.
Remaining items all need either a human judgment or their own
design review — none are safe to rush:
  HUMAN: persist default flip (LSB-equivalence judgment, mono -8.6%
    evidence attached); R14 spec text; interp go/no-go/N3DS;
    master-merge approval.
  DESIGN-REVIEW-FIRST (me, next sessions): BatchId-by-construction
    (gate currently covers the failure mode); FLUSH_BATCH macro
    dedup (touches the submit path that wedged once); R02 overflow
    fence (touches frame pacing); snprintf-builds FrameBegin wedge
    bisect (unknown mechanism, workaround in place); New 3DS capture
    session; residual color-word mismatch forensics.
Console left listening; tree clean; branch pushed.

## 2026-11-14 — Slider-gated persist default-on (HARDWARE, owner policy)

PERSIST_VERTS ?= 1 with a runtime gate: active only when
stereo_mode != STEREO_MODE_3DS (3D slider down), refreshed per
FSBeginScene. Stereo keeps the proven memo-only path; buffers stay
allocated across slider moves and go unused in stereo.
Validation (ramqan, default flags, PV=1 in stamp):
  stereo: vown_level=2,392,302 (persist inactive), splits 0, 88,443us
  mono:   vown_level=508,836 (active), splits 0, 57,899us vs
          63,821 no-persist (-9.3%), fidelity 594bab3c identical.
Same gate principle audited across landed optimizations: CHE/sim
wins help both modes; buckets/squared-distance were mono-neutral;
memo is stereo-only by construction. Persist was the only item
paying in mono alone — it is now mono-gated rather than global-off.

## 2026-11-14 — Owner decisions recorded

- Merge hygiene (#2): APPROVED — R04/MODEL_BUCKET verdicts + land
  this branch (next entries).
- Interp (#3): DROPPED entirely. No OG build, no New 3DS build.
  Prototype remains flag-off in-tree as a record only.
- Stereo state (#4): ACCEPTED as the milestone. No further OG
  stereo optimization after merge work.

## 2026-11-14 — MODEL_BUCKET default-on + TF04-A mono-gated revisit (HARDWARE)

Bucket stereo verdict (ramqan, this branch): bucket-off 88,443 vs
bucket-on 77,705 / 77,635 (two runs within 70us) — -12%, fidelity
byte-identical, draws/verts counts identical. Default flipped on
(converges with master). Mono was neutral in 2026-11-12 A/B/A;
stereo doubles the traversal the bucket eliminates.
Mono-reject audit (owner request): nearly all rejects (TF04-A, R04,
R07, opt3, single-pass x2, NODE_SQDIST, R20) were measured in MONO
and flat even there — not "rejected for being mono-only". TF04-A is
the one revisit: its convert slice is now relatively larger (persist
removed the level share; mono proved CPU-conversion-bound), and its
LSB pixel class matches the accepted persist precedent. Rebuilt as
shared quad table (quadidx.c, host-tested), mono-gated via
s_monoActive, QUADIDX_VERTS default 1. A/B follows.

## 2026-11-14 — TF04-A revisit verdict: mechanism exact, wall-neutral (HARDWARE)

Mono ramqan (bucket+persist gated stack): quad ON 51,274 vs OFF
51,484 (-0.4%, noise). tverts 561,192 -> 374,128 = the historical
-33.3% exactly; fidelity byte-identical; splits 0. MODEL_BUCKET
(-12% stereo, now default) removed the traversal/conversion
headroom quad would have exploited. QUADIDX_VERTS default back to
0 (opt-in); quadidx.c + host test + mono runtime gate retained.
Not a keeper today — same conservative rule as 2026-11-12, now
with the bucket interaction measured rather than assumed.

## 2026-11-14 — MERGED to master (HARDWARE-PROVEN)

experimental/frame-interp merged into master. Resolutions:
render_c3d.c + shader taken wholesale from experimental (36B
vertex path; master's R04 24B not ported per verdict, display-list
replay retired — 0 s_dlReplay symbols remain); atm counters unioned
(master fillq/hitskip + experimental memo/vown/splits); visi.c keeps
master's early unmagnified shortcut + experimental's in-block
fullscreen guard; q3 ring dump stubbed (lived in master's backend;
FILLQ counters remain, stay 0); tests/Makefile unioned.
Merged ramqan stereo (Old 3DS, defaults): avg 71,306us — faster
than either parent (branch tip 77,426; master stack unmeasured as
one) — fill-cuts/bullprim from master compose with memo+buckets.
Fidelity byte-identical, memo_hit=34368, scratch_splits=0.
Gates ALL PASS on merged tree; host tests all green.
## 2026-11-14 — WIRING BUG: release builds never had the memo (found by slider build)

-DSTEREO_REPLAY/-DPERSIST_VERTS were wired inside the ATM_PROFILING
block, so every non-bench build silently compiled the memo/persist
out. All bench evidence stands (benches are ATM builds), but the
R22 playable build the owner judged had NO memo — its "fine" did not
cover the animated-surface UV lag. Caught when QUADIDX code broke
the release build. Fix: STEREO/PERSIST/QUADIDX *_CFLAGS defined
above the CFLAGS := expansion and consumed in the main list (the
SHDLIT2 lesson, third occurrence). Release ELF now contains
smemo_lookup (nm-verified). R22 re-judgment needed on a build that
actually has the memo: slider50 build (also halves max eye-sep
30->15 per owner request) pushed for that look.
