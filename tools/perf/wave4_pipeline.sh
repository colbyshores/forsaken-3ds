#!/bin/bash
# wave2_pipeline.sh — wait for the wave-2 sweep to complete, verify the
# artifact is fully written and fresh, then analyze automatically.
set -u
IP=192.168.4.52
OUT=/tmp/wave4_perf.txt
ANAL=/tmp/wave4_analysis.txt
T0=$(date +%s)
TIMEOUT=3600

say() { echo "[$(date -u +%H:%M:%S)] $*"; }

if curl -s --max-time 2 -o /dev/null "http://$IP:8080/" 2>/dev/null; then
	say "stub up: waiting for game start (:8080 down)"
	while curl -s --max-time 2 -o /dev/null "http://$IP:8080/" 2>/dev/null; do
		[ $(( $(date +%s) - T0 )) -gt $TIMEOUT ] && { say "TIMEOUT waiting for game start"; exit 1; }
		sleep 5
	done
fi
say "game running: waiting for sweep end (:8080 back)"

until curl -s --max-time 2 -o /dev/null "http://$IP:8080/forsaken_perf.txt" 2>/dev/null; do
	[ $(( $(date +%s) - T0 )) -gt $TIMEOUT ] && { say "TIMEOUT waiting for stub return"; exit 1; }
	sleep 15
done
say "stub back: fetching + verifying artifact"

prev=""
for i in $(seq 1 20); do
	curl -s --max-time 30 "http://$IP:8080/forsaken_perf.txt" -o "$OUT" || { sleep 5; continue; }
	md=$(md5sum "$OUT" | cut -d' ' -f1)
	tail_ok=$(tail -1 "$OUT" | grep -c '^PERF: dump end$')
	fresh=$(grep -c 'stage=39 ' "$OUT")
	if [ -n "$prev" ] && [ "$md" = "$prev" ] && [ "$tail_ok" = 1 ] && [ "$fresh" -gt 0 ]; then
		say "ARTIFACT VERIFIED md5=$md lines=$(wc -l < "$OUT") wave4_rows=$fresh"
		break
	fi
	prev=$md
	sleep 5
	[ $(( $(date +%s) - T0 )) -gt $TIMEOUT ] && { say "TIMEOUT verifying artifact"; exit 1; }
done
[ -f "$OUT" ] || { say "no artifact"; exit 1; }

python3 /mnt/nas/Development/forsaken/tools/perf/analyze_sweep.py "$OUT" "$ANAL"
say "WAVE4 PIPELINE COMPLETE"
