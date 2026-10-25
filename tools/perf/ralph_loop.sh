#!/bin/bash
# ralph_loop.sh — autonomous capture loop against netload_stub.
# Cycle: wait for game to take the port (stub exited) -> wait for stub back
# (sweep done, chain-loaded) -> re-push. Each cycle = one full autotest
# sweep appending per-level PERF rows to sdmc:/forsaken_perf.txt.
cd /mnt/nas/Development/forsaken || exit 1
HOST=192.168.4.52
PORT=17491
open() { (exec 3<>"/dev/tcp/$HOST/$PORT") 2>/dev/null; }
CYCLES=${1:-3}
for c in $(seq 1 "$CYCLES"); do
	echo "cycle $c: waiting for game to start (port close) $(date -u +%H:%M:%S)"
	while open; do sleep 5; done
	echo "cycle $c: game running, waiting for sweep end (port open) $(date -u +%H:%M:%S)"
	while ! open; do sleep 10; done
	sleep 3
	echo "cycle $c: stub back, pushing $(date -u +%H:%M:%S)"
	if timeout 180 3dslink -a "$HOST" forsaken3ds.3dsx; then
		echo "cycle $c: pushed OK"
	else
		echo "cycle $c: PUSH FAILED rc=$?"
	fi
done
echo "RALPH LOOP DONE $(date -u +%H:%M:%S)"
