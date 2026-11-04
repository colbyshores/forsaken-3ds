# ATM Optimization Log — Forsaken (Old 3DS)

## Phase A — VALIDATE the hardware loop

Console identity:
- IP: 192.168.4.167
- Device: old3ds-lan167
- Firmware: 22c4bb74bc64+dirty
- Luma3DS+ATM (repo: /mnt/nas/Development/Luma3DS)
- Boot count: 51 (ok)
- Lid: closed_blanked
- Devmode: armed=yes, active=yes (svc_atm=active:devmode, svc_gdb=active:devmode)
- GDB: running on 4000-4003
- Auth: deployment=yes, remote_control=yes
- Recovery ready: yes
- Operational: yes

Smoke test: Will run atm_workload_ok.3dsx

Phase A complete: smoke test run-788e0bbd-1 completed with 18 verified artifacts.
- atm_result.txt: workload=atm_workload_ok frames=240 duration_ms=3790 avg_us=15791 p50_us=18000 p95_us=19000 p99_us=22000 max_us=22000 over_budget=125
- osGetTime granularity 1ms noted (percentiles bucket estimates)
