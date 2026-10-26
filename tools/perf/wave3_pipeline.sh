#!/bin/bash
# wave3_pipeline.sh — A/B watcher for the optimization-B sweep.
#
# Phase 1: the in-flight sweep (B build, aggregate-only dump) finishes ->
#          fetch its artifact (sanity/stability signal only).
# Phase 2: push the per-level marker build, wait for its full sweep,
#          fetch, verify, and diff per-level render/frame against the
#          wave-2b capture (same device, same day, contract code +
#          instrumentation, no optimization B).
set -u
IP=192.168.4.52
OUT=/tmp/wave3_perf.txt
AGG=/tmp/wave3a_perf.txt
BASE=/tmp/wave2_perf.txt
ANAL=/tmp/wave3_analysis.txt
T0=$(date +%s)
PHASE_BUDGET=3600

say() { echo "[$(date -u +%H:%M:%S)] $*"; }
up()   { curl -s --max-time 2 -o /dev/null "http://$IP:8080/" 2>/dev/null; }
budget() { [ $(( $(date +%s) - T0 )) -gt $1 ] && { say "TIMEOUT"; exit 1; } || true; }

fetch_verify() { # $1 = dest, $2 = freshness grep pattern
	local prev="" md i
	for i in $(seq 1 20); do
		curl -s --max-time 30 "http://$IP:8080/forsaken_perf.txt" -o "$1" || { sleep 5; continue; }
		md=$(md5sum "$1" | cut -d' ' -f1)
		local tail_ok=$(tail -1 "$1" | grep -c '^PERF: dump end$')
		local fresh=$(grep -c "$2" "$1")
		if [ -n "$prev" ] && [ "$md" = "$prev" ] && [ "$tail_ok" = 1 ] && [ "$fresh" -gt 0 ]; then
			say "ARTIFACT VERIFIED $1 md5=$md lines=$(wc -l < "$1") rows=$fresh"
			return 0
		fi
		prev=$md
		sleep 5
	done
	say "FAILED to verify artifact $1"
	return 1
}

# ---- Phase 1: wait for the running sweep to end (stub back) ----
say "phase1: waiting for in-flight sweep to end (:8080 back)"
until up; do budget $((T0+PHASE_BUDGET)); sleep 15; done
sleep 3
fetch_verify "$AGG" 'stage=2 ' || true

# ---- Phase 2: push marker build, wait for its sweep ----
say "phase2: pushing per-level marker build"
if ! timeout 180 3dslink -a "$IP" /mnt/nas/Development/forsaken/forsaken3ds.3dsx; then
	say "PUSH FAILED"; exit 1
fi
say "phase2: waiting for game start (:8080 down)"
until ! up; do budget $((T0+2*PHASE_BUDGET)); sleep 5; done
say "phase2: game running, waiting for sweep end (:8080 back)"
until up; do budget $((T0+2*PHASE_BUDGET)); sleep 15; done
sleep 3
fetch_verify "$OUT" '=== level=' || exit 1

python3 - "$OUT" "$BASE" "$ANAL" <<'PYEOF'
import re, sys
TICK = 268111840.0

def load(path, keep):
    """keep(block_stages) decides which level-blocks count."""
    rows, lvl, cur = [], None, {}
    for line in open(path):
        line = line.strip()
        m = re.match(r"PERF: === level=(.+?) ===$", line)
        if m:
            if cur and lvl and keep(set(cur)): rows.append((lvl, cur))
            lvl, cur = m.group(1), {}
            continue
        if line == "PERF: dump begin":
            if cur and lvl and keep(set(cur)): rows.append((lvl, cur))
            lvl, cur = None, {}
            continue
        m = re.match(r"PERF: stage=(\d+) count=(\d+) total=(\d+) max=(\d+)", line)
        if m:
            i, c, t, mx = map(int, m.groups())
            cur.setdefault(i, []).append((c, t, mx))
    if cur and lvl and keep(set(cur)): rows.append((lvl, cur))
    agg = {}
    for l, d in rows:
        a = agg.setdefault(l, {})
        for i, v in d.items(): a.setdefault(i, []).extend(v)
    return agg

def perframe(entries):
    c = sum(c for c, _, _ in entries)
    t = sum(t for _, t, _ in entries)
    return t / c / TICK * 1000 if c else 0.0

# wave-3 rows: stages 0-2 only (no stage 3+)
new = load(sys.argv[1], lambda s: 2 in s and 3 not in s)
# wave-2b baseline rows: full wave-2b blocks (stage 26 present)
base = load(sys.argv[2], lambda s: 26 in s)

out = [f"{'level':<13}{'fps':>6}{'frame':>7}{'mgame':>7}{'render':>7}{'renderA':>8}{'d_rend':>7}{'d_fps':>6}"]
rows = []
for lvl, d in sorted(new.items()):
    if 2 not in d or 0 not in d: continue
    fr = perframe(d[0]); mg = perframe(d[1]); rn = perframe(d[2])
    ra = perframe(base[lvl][2]) if lvl in base and 2 in base[lvl] else float('nan')
    fa = perframe(base[lvl][0]) if lvl in base and 0 in base[lvl] else float('nan')
    rows.append((rn - ra, lvl, fr, mg, rn, ra, fa))
for delta, lvl, fr, mg, rn, ra, fa in sorted(rows, key=lambda r: -r[2]):
    out.append(f"{lvl:<13}{1000/fr:>6.1f}{fr:>7.2f}{mg:>7.2f}{rn:>7.2f}{ra:>8.2f}{delta:>+7.2f}{1000/fr-1000/fa if fa==fa else float('nan'):>+6.1f}")
txt = "\n".join(out)
print(txt)
open(sys.argv[3], "w").write(txt + "\n")
PYEOF
[ $? -eq 0 ] || { say "ANALYZER FAILED"; exit 1; }
say "WAVE3 PIPELINE COMPLETE"
