#!/usr/bin/env bash
# ab_vis_cache.sh — A/B runner for MODEL_VIS_CACHE.
# Run 1 (already pushed by hand: cache variant) is awaited, captured, then
# run 2 (true baseline, $1) is pushed and captured. Both land in the
# append-only forsaken_perf.txt; snapshots are saved separately.
set -u
IP=192.168.4.52
BASE_3DSX=$1
T0=$(date +%s)
say(){ echo "[$(date -u +%H:%M:%S)] $*"; }

wait_stub_back() {
  while ! curl -s --max-time 2 -o /dev/null "http://$IP:8080/" 2>/dev/null; do
    [ $(( $(date +%s) - T0 )) -gt 5400 ] && { say "TIMEOUT waiting for stub"; exit 1; }
    sleep 10
  done
  sleep 8
}
fetch_perf() {
  local out=$1
  for i in 1 2 3 4 5; do
    curl -s --max-time 30 "http://$IP:8080/forsaken_perf.txt" -o "$out" && \
      [ "$(tail -1 "$out")" = "PERF: dump end" ] && return 0
    sleep 5
  done
  return 1
}

say "run1 (cache variant): waiting for sweep end"
wait_stub_back
fetch_perf /tmp/ab_run1_cache.txt && say "run1 captured $(wc -c < /tmp/ab_run1_cache.txt) bytes" || { say "run1 fetch failed"; exit 1; }

say "pushing baseline $BASE_3DSX"
timeout 180 3dslink -a $IP "$BASE_3DSX" 2>&1 | tail -1
sleep 30
say "run2 (baseline): waiting for sweep end"
wait_stub_back
fetch_perf /tmp/ab_run2_base.txt && say "run2 captured $(wc -c < /tmp/ab_run2_base.txt) bytes" || { say "run2 fetch failed"; exit 1; }
say "AB VIS CACHE COMPLETE"
