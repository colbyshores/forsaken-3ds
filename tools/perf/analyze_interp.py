#!/usr/bin/env python3
"""analyze_interp.py CAPTURE [BASELINE] — temporal-interpolation prototype report.

Stage map (current perf_capture.h enum): 5=cam, 57=interpprobe, 58=interptotal,
59=interponly (extra pass), 60=interpsim (true-frame render window),
61=presentloop (RenderLoop window), 62=present (pglSwapBuffers submissions).

The key interpolation numbers are interponly (cost of the in-between pass),
present count per frame, and presents/s: with FRAME_INTERP_EXTRA_PRESENT a
simulation tick should submit TWO display frames (true + interpolated), so
presents/frame ~2 and presents/s ~2x simulation fps. Interpolation cannot
raise the simulation rate — it only changes how many frames are presented.

Rows are taken from the LAST occurrence of each level marker (the SD perf
file is append-only across runs) and only from dumps that carry the full
current stage set, so historical partial dumps never masquerade as the
latest run. A focused single-level run (AUTOTEST_MAX_LEVELS_TO_SWEEP=1)
therefore analyzes on its own.
"""
import re, sys

TICK = 268111840.0
MIN_STAGES = 62   # current enum is 63 wide; older partial dumps are skipped

def load(path):
    t = open(path).read()
    # level sections end at the next level marker OR at a bare "dump begin"
    # (teardown dumps after the last level are title-screen data, not the
    # last level's stats).
    marks = [(m.start(), 'level', m.group(1), m.end())
             for m in re.finditer(r'PERF: === level=(.*?) ===', t)]
    marks += [(m.start(), 'dump', None, m.end())
              for m in re.finditer(r'PERF: dump begin', t)]
    marks.sort()
    out = {}
    for i, (pos, kind, name, end) in enumerate(marks):
        if kind != 'level':
            continue
        nxt = marks[i+1][0] if i+1 < len(marks) else len(t)
        sec = t[end:nxt]
        st = {int(g[0]): tuple(map(int, g[1:]))
              for g in re.findall(r'PERF: stage=(\d+) count=(\d+) total=(\d+) max=(\d+)', sec)}
        if len(st) < MIN_STAGES:
            continue
        f = st.get(0, (0, 0, 0))[0]
        if not f:
            continue
        wall_s = st[0][1] / TICK
        pres = st.get(62, (0, 0, 0))
        out[name] = {
            'frames': f,
            'fps': f / wall_s if wall_s else 0.0,
            'cam_ms': st.get(5, (0, 0, 0))[1] / f / TICK * 1000.0,
            'probe_ms': st.get(57, (0, 0, 0))[1] / f / TICK * 1000.0,
            'all_ms': st.get(58, (0, 0, 0))[1] / f / TICK * 1000.0,
            'interp_ms': st.get(59, (0, 0, 0))[1] / f / TICK * 1000.0,
            'interp_per_frame': st.get(59, (0, 0, 0))[0] / f,
            'sim_ms': st.get(60, (0, 0, 0))[1] / f / TICK * 1000.0,
            'presentloop_ms': st.get(61, (0, 0, 0))[1] / f / TICK * 1000.0,
            'present_count': pres[0],
            'pres_per_frame': pres[0] / f,
            'pres_per_s': pres[0] / wall_s if wall_s else 0.0,
            'present_ms': pres[1] / pres[0] / TICK * 1000.0 if pres[0] else 0.0,
        }
    return out

def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    cur = load(sys.argv[1])
    base = load(sys.argv[2]) if len(sys.argv) > 2 else {}
    if not cur:
        print('no current-format level rows found in', sys.argv[1])
        return 1
    hdr = (f"{'level':<12}{'fps':>6}{'pres/s':>8}{'p/f':>6}{'base':>6}{'d%':>7}"
           f"{'cam_ms':>8}{'sim_ms':>8}{'interp_ms':>10}{'all_ms':>8}{'pl_ms':>7}{'pres_ms':>9}")
    print(hdr)
    for lvl, d in sorted(cur.items(), key=lambda kv: -kv[1]['interp_ms']):
        b = base.get(lvl, {}).get('fps')
        delta = (d['fps'] / b - 1.0) * 100.0 if b else None
        ps = f"{d['pres_per_s']:.1f}" if d['present_count'] else '-'
        pf = f"{d['pres_per_frame']:.2f}" if d['present_count'] else '-'
        bs = f"{b:.1f}" if b else '-'
        ds = f"{delta:+.1f}" if delta is not None else '-'
        print(f"{lvl:<12}{d['fps']:>6.1f}{ps:>8}{pf:>6}{bs:>6}{ds:>7}"
              f"{d['cam_ms']:>8.2f}{d['sim_ms']:>8.2f}{d['interp_ms']:>10.2f}"
              f"{d['all_ms']:>8.2f}{d['presentloop_ms']:>7.2f}{d['present_ms']:>9.3f}")
    return 0

if __name__ == '__main__':
    sys.exit(main())
