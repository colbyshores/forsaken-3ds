#!/usr/bin/env python3
"""analyze_sweep.py — per-frame PERF breakdown from a forsaken_perf.txt dump.

Usage: analyze_sweep.py <perf.txt> [out.txt]
Per-frame normalization: every stage's total is divided by the level's frame
count (sub-stages accumulate multiple calls per frame, e.g. stereo x2).
The worst frame per stage is subtracted (level-load spike).
Rows are only counted when they contain the wave-2 sim stages (>=11).
"""
import re, sys
from collections import defaultdict

STAGES = ["frame","maingame","render","fsbegin","mROUT","cam","fsend","vis","bg",
          "opaque","trans","enemies","spotfx","bgobj","models","polys","rtlights","water",
          "bullets","ships","fires","pickups","xlights","afterbrn","fmpoly","conditions","triggers"]
TICK = 268111840.0

def load(path):
    rows, cur_level, cur = [], None, {}
    for line in open(path):
        line = line.strip()
        m = re.match(r"PERF: === level=(.+?) ===$", line)
        if m:
            if cur and cur_level and "enemies" in cur: rows.append((cur_level, cur))
            cur_level, cur = m.group(1), {}
            continue
        if line == "PERF: dump begin":
            if cur and cur_level and "enemies" in cur: rows.append((cur_level, cur))
            cur_level, cur = None, {}
            continue
        m = re.match(r"PERF: stage=(\d+) count=(\d+) total=(\d+) max=(\d+)", line)
        if m:
            i, c, t, mx = map(int, m.groups())
            if i < len(STAGES): cur[STAGES[i]] = (c, t, mx)
    if cur and cur_level and "enemies" in cur: rows.append((cur_level, cur))
    agg = defaultdict(lambda: defaultdict(list))
    for lvl, d in rows:
        for s, v in d.items(): agg[lvl][s].append(v)
    return agg

def main():
    agg = load(sys.argv[1])
    def perframe(d, s):
        fcs = sum(c for c, _, _ in d["frame"])
        ts = sum(t for _, t, _ in d[s])
        return ts / fcs / TICK * 1000 if fcs else 0
    def frame_ms(d):
        fcs = sum(c for c, _, _ in d["frame"])
        ts = sum(t for _, t, _ in d["frame"])
        mx = max(mx for _, _, mx in d["frame"])
        return (ts - mx) / (fcs - 1) / TICK * 1000 if fcs > 1 else 0
    out = []
    for lvl, d in agg.items():
        if not all(s in d for s in STAGES): continue
        fm = frame_ms(d)
        r = {s: perframe(d, s) for s in STAGES}
        out.append((r["mROUT"], lvl, 1000 / fm if fm else 0, r))
    hdr = (f"{'level':<13}{'fps':>5}{'mROUT':>6}{'ships':>6}{'fires':>6}{'enem':>6}"
           f"{'bull':>6}{'trig':>6}{'cond':>6}{'pick':>6}{'xlgt':>6}{'spotfx':>7}{'bobj':>6}"
           f"{'model':>6}{'cam':>6}{'bg':>6}{'trans':>6}")
    lines = [hdr]
    for mr, lvl, fps, r in sorted(out, reverse=True):
        lines.append(f"{lvl:<13}{fps:>5.1f}{r['mROUT']:>6.1f}{r['ships']:>6.1f}{r['fires']:>6.1f}"
                     f"{r['enemies']:>6.1f}{r['bullets']:>6.1f}{r['triggers']:>6.1f}{r['conditions']:>6.1f}"
                     f"{r['pickups']:>6.1f}{r['xlights']:>6.1f}{r['spotfx']:>7.1f}{r['bgobj']:>6.1f}"
                     f"{r['models']:>6.1f}{r['cam']:>6.1f}{r['bg']:>6.1f}{r['trans']:>6.1f}")
    txt = "\n".join(lines)
    if len(sys.argv) > 2: open(sys.argv[2], "w").write(txt + "\n")
    print(txt)

main()
