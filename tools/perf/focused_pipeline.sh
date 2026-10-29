#!/usr/bin/env bash
# focused_pipeline.sh OUT STAGE [BASE_MD5] — wait for an autotest perf sweep
# to finish and fetch forsaken_perf.txt. TIMEOUT accommodates full remaster
# sweeps (roughly 45 minutes); optional BASE_MD5 rejects stale artifacts.
set -u
IP=192.168.4.52
OUT=$1
STAGE=$2
BASE_MD5=${3:-}
T0=$(date +%s)
TIMEOUT=2700

say() { echo "[$(date -u +%H:%M:%S)] $*"; }

# game may still be booting: wait for it to start (8080 down) then end (8080 up)
if curl -s --max-time 2 -o /dev/null "http://$IP:8080/" 2>/dev/null; then
	say "stub up: waiting for game start"
	while curl -s --max-time 2 -o /dev/null "http://$IP:8080/" 2>/dev/null; do
		[ $(( $(date +%s) - T0 )) -gt $TIMEOUT ] && { say "TIMEOUT waiting for start"; exit 1; }
		sleep 3
	done
fi
say "game running: waiting for run end (:8080 back)"
while ! curl -s --max-time 2 -o /dev/null "http://$IP:8080/" 2>/dev/null; do
	[ $(( $(date +%s) - T0 )) -gt $TIMEOUT ] && { say "TIMEOUT waiting for end"; exit 1; }
	sleep 10
done
say "stub back: fetching + verifying artifact"
prev=""
for i in $(seq 1 20); do
	curl -s --max-time 30 "http://$IP:8080/forsaken_perf.txt" -o "$OUT" || { sleep 5; continue; }
	md=$(md5sum "$OUT" | cut -d' ' -f1)
	tail_ok=$(tail -1 "$OUT" | grep -c '^PERF: dump end$')
	fresh=$(grep -c "stage=$STAGE " "$OUT")
	if [ -n "$BASE_MD5" ] && [ "$md" = "$BASE_MD5" ]; then
		say "FRESHNESS FAILURE: artifact md5 matches the preceding capture ($md)"
		exit 2
	fi
	if [ -n "$prev" ] && [ "$md" = "$prev" ] && [ "$tail_ok" = 1 ] && [ "$fresh" -gt 0 ]; then
		say "FOCUSED RUN OK md5=$md fresh_stage=$STAGE"
		exit 0
	fi
	prev=$md
	sleep 5
done
say "FOCUSED RUN VERIFY FAILED"
exit 1
