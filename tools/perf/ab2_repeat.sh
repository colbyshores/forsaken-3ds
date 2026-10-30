#!/usr/bin/env bash
# ab2_repeat.sh BASE_BUCKET — repeat A/B: baseline sweep ($1, already pushed),
# then bucket sweep ($2), capturing both. Run back-to-back in one session to
# rule out cross-session drift in the bucket verdict.
set -u
IP=192.168.4.52
BASE=$1
BUCKET=$2
T0=$(date +%s)
say(){ echo "[$(date -u +%H:%M:%S)] $*"; }
wait_stub_back(){ while ! curl -s --max-time 2 -o /dev/null "http://$IP:8080/" 2>/dev/null; do [ $(( $(date +%s) - T0 )) -gt 7200 ] && { say TIMEOUT; exit 1; }; sleep 10; done; sleep 8; }
fetch(){ local o=$1; for i in 1 2 3 4 5; do curl -s --max-time 30 "http://$IP:8080/forsaken_perf.txt" -o "$o" && [ "$(tail -1 "$o")" = "PERF: dump end" ] && return 0; sleep 5; done; return 1; }

say "runA (baseline): waiting for sweep end"
wait_stub_back
fetch /tmp/ab2_runA_base.txt && say "runA captured" || { say "runA fetch failed"; exit 1; }

say "pushing bucket variant $BUCKET"
timeout 180 3dslink -a $IP "$BUCKET" 2>&1 | tail -1
sleep 30
say "runB (bucket): waiting for sweep end"
wait_stub_back
fetch /tmp/ab2_runB_bucket.txt && say "runB captured" || { say "runB fetch failed"; exit 1; }
say "AB2 COMPLETE"
