#!/usr/bin/env python3
"""analyze_interp.py CAPTURE [BASELINE] — temporal-interpolation prototype report.

Prints, per level: observed fps, camera-pass cost (stage 5), in-between pass
cost (stage 57, PERF_S_INTERPPROBE), and the pass ratio. With BASELINE it also
prints the fps delta against the pre-interpolation capture. The key number is
the in-between pass cost: the prototype re-runs the render traversal, so that
cost is added to every displayed frame. Interpolation cannot raise the
simulation rate — it only changes how many frames are presented and at what
cost per frame.
"""
import re, sys, os

TICK = 268111840.0

def load(path):
    t = open(path).read()
    ms = list(re.finditer(r'PERF: === level=(.*?) ===', t))
    idx = [i for i, m in enumerate(ms) if m.group(1) == 'vol2']
    if not idx:
        return {}
    i = idx[-1]
    out = {}
    for j in range(i, len(ms)):
        end = ms[j+1].start() if j+1 < len(ms) else len(t)
        sec = t[ms[j].end():end]
        st = {int(m.group(1)): tuple(map(int, m.groups()[1:]))
              for m in re.finditer(r'PERF: stage=(\d+) count=(\d+) total=(\d+) max=(\d+)', sec)}
        f = st.get(0, (0, 0, 0))[0]
        if not f:
            continue
        mean_ms = st[0][1] / f / TICK * 1000.0
        cam = st.get(5, (0, 0, 0))
        probe = st.get(57, (0, 0, 0))
        out[ms[j].group(1)] = {
            'frames': f,
            'fps': 1000.0 / mean_ms if mean_ms else 0.0,
            'cam_ms': cam[1] / f / TICK * 1000.0,
            'inb_ms': probe[1] / f / TICK * 1000.0,
            'inb_per_frame': probe[0] / f,
            'all_ms': st.get(58, (0, 0, 0))[1] / f / TICK * 1000.0,
            'interp_ms': st.get(59, (0, 0, 0))[1] / f / TICK * 1000.0,
            'reported_fps': min(60.0, (1000.0 / mean_ms) * 2.0) if st.get(59, (0, 0, 0))[0] else (1000.0 / mean_ms),
        }
    return out

def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    cur = load(sys.argv[1])
    base = load(sys.argv[2]) if len(sys.argv) > 2 else {}
    if not cur:
        print('no complete sweep found in', sys.argv[1])
        return 1
    hdr = f"{'level':<12}{'fps':>6}{'present':>8}{'base':>6}{'cam_ms':>8}{'inb_ms':>8}{'inb/frame':>10}{'total_ms':>9}{'interp_ms':>10}{'allpass_ms':>12}"
    print(hdr)
    for lvl, d in sorted(cur.items(), key=lambda kv: -kv[1]['inb_ms']):
        b = base.get(lvl, {}).get('fps')
        total = d['cam_ms'] + d['inb_ms']
        print(f"{lvl:<12}{d['fps']:>6.1f}{(f'{d['reported_fps']:.1f}' if d['reported_fps'] else '-'):>8}{(f'{b:.1f}' if b else '-'):>6}"
              f"{d['cam_ms']:>8.2f}{d['inb_ms']:>8.2f}{d['inb_per_frame']:>10.2f}{total:>9.2f}{d['interp_ms']:>10.2f}{d['all_ms']:>12.2f}")
    return 0

if __name__ == '__main__':
    sys.exit(main())
