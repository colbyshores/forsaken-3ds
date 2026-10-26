#!/bin/bash
# wave1b_pipeline.sh — wait for wave-1b sweep completion, verify the artifact
# is fully written, then analyze automatically. Exits non-zero on timeout so
# the caller can distinguish failure from success.
set -u
IP=192.168.4.52
OUT=/tmp/wave1b_perf.txt
ANAL=/tmp/wave1b_analysis.txt
T0=$(date +%s)
TIMEOUT=3600   # sweep ~34 min + slack

say() { echo "[$(date -u +%H:%M:%S)] $*"; }

# 1) wait for the game to take the console (:8080 down). If the sweep already
#    ended before this script started, skip straight to verification.
if curl -s --max-time 2 -o /dev/null "http://$IP:8080/" 2>/dev/null; then
	say "stub up: waiting for game start (:8080 down)"
	while curl -s --max-time 2 -o /dev/null "http://$IP:8080/" 2>/dev/null; do
		[ $(( $(date +%s) - T0 )) -gt $TIMEOUT ] && { say "TIMEOUT waiting for game start"; exit 1; }
		sleep 5
	done
fi
say "game running: waiting for sweep end (:8080 back)"

# 2) wait for the stub to return
until curl -s --max-time 2 -o /dev/null "http://$IP:8080/forsaken_perf.txt" 2>/dev/null; do
	[ $(( $(date +%s) - T0 )) -gt $TIMEOUT ] && { say "TIMEOUT waiting for stub return"; exit 1; }
	sleep 15
done
say "stub back: fetching + verifying artifact"

# 3) verify a COMPLETE, FRESH artifact:
#    - two consecutive fetches byte-identical (md5 stable)
#    - last line is "PERF: dump end" (teardown dump finished)
#    - contains stage=10 (TRANS index exists only in wave-1b rows)
prev=""
for i in $(seq 1 20); do
	curl -s --max-time 30 "http://$IP:8080/forsaken_perf.txt" -o "$OUT" || { sleep 5; continue; }
	md=$(md5sum "$OUT" | cut -d' ' -f1)
	tail_ok=$(tail -1 "$OUT" | grep -c '^PERF: dump end$')
	fresh=$(grep -c 'stage=10 ' "$OUT")
	if [ -n "$prev" ] && [ "$md" = "$prev" ] && [ "$tail_ok" = 1 ] && [ "$fresh" -gt 0 ]; then
		say "ARTIFACT VERIFIED md5=$md lines=$(wc -l < "$OUT") wave1b_rows=$fresh"
		break
	fi
	prev=$md
	sleep 5
	[ $(( $(date +%s) - T0 )) -gt $TIMEOUT ] && { say "TIMEOUT verifying artifact"; exit 1; }
done
[ -f "$OUT" ] || { say "no artifact"; exit 1; }

# 4) analyze automatically: wave-1b rows only (rows containing stage=10)
python3 - "$OUT" "$ANAL" <<'EOF'
import re, sys
from collections import defaultdict
STAGES = ["frame","maingame","render","fsbegin","mROUT","cam","fsend","vis","bg","opaque","trans"]
TICK = 268111840.0
rows, cur_level, cur = [], None, {}
for line in open(sys.argv[1]):
    line = line.strip()
    m = re.match(r"PERF: === level=(.+?) ===$", line)
    if m:
        if cur and cur_level and "bg" in cur: rows.append((cur_level, cur))
        cur_level, cur = m.group(1), {}; continue
    if line == "PERF: dump begin":
        if cur and cur_level and "bg" in cur: rows.append((cur_level, cur))
        cur_level, cur = None, {}; continue
    m = re.match(r"PERF: stage=(\d+) count=(\d+) total=(\d+) max=(\d+)", line)
    if m:
        i,c,t,mx = map(int, m.groups())
        if i < len(STAGES): cur[STAGES[i]] = (c,t,mx)
if cur and cur_level and "bg" in cur: rows.append((cur_level, cur))
agg = defaultdict(lambda: defaultdict(list))
for lvl, d in rows:
    for s,(c,t,mx) in d.items(): agg[lvl][s].append((c,t,mx))
def mean_excl(d, s):
    cs = sum(c for c,_,_ in d[s]); ts = sum(t for _,t,_ in d[s]); mx = max(mx for _,_,mx in d[s])
    return (ts-mx)/(cs-1)/TICK*1000
out = []
for lvl, d in agg.items():
    if not all(s in d for s in STAGES): continue
    fr = mean_excl(d,"frame")
    out.append((mean_excl(d,"cam"), lvl, 1000/fr, mean_excl(d,"vis"), mean_excl(d,"bg"),
                mean_excl(d,"opaque"), mean_excl(d,"trans"), mean_excl(d,"cam")))
lines = [f"{'level':<14}{'fps':>5}{'cam/call':>9}{'vis':>6}{'bg':>6}{'opaque':>7}{'trans':>6}"]
for cam,lvl,fps,vis,bg,op,tr,cc in sorted(out, reverse=True):
    lines.append(f"{lvl:<14}{fps:>5.1f}{cc:>9.1f}{vis:>6.1f}{bg:>6.1f}{op:>7.1f}{tr:>6.1f}")
txt = "\n".join(lines)
open(sys.argv[2],"w").write(txt+"\n")
print(txt)
EOF
say "WAVE1B PIPELINE COMPLETE"
