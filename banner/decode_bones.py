#!/usr/bin/env python3
"""Decode the skeleton bones straight from a CGFX's bytes (no pycgfx), so a
build can be checked against what was actually serialised -- T2K's lesson:
read the shipped bytes before theorising.  Usage: decode_bones.py file.cgfx"""
import struct, sys

d = open(sys.argv[1], "rb").read()


def cstr(off):
    return d[off:d.index(b"\0", off)].decode("ascii", "replace")


names = {}
for i in range(0, len(d) - 4, 4):
    rel = struct.unpack_from("<i", d, i)[0]
    t = i + rel
    if 0 < rel < len(d) and t < len(d):
        names.setdefault(i, t)

# Bone layout (pycgfx sobj.Bone): name ptr, flags, joint, parent_id,
# parent/child/prev/next refs, scale3 rot3 pos3, local/world/invbase 3x4,
# billboard mode.
rows = []
for i, t in names.items():
    try:
        name = cstr(t)
    except ValueError:
        continue
    if not name or not name.replace(" ", "").replace("_", "").isalnum() or len(name) > 32:
        continue
    if i + 4 * (8 + 9 + 36 + 1) > len(d):
        continue
    flags, joint, parent = struct.unpack_from("<Iii", d, i + 4)
    if not (0 <= joint < 64 and -1 <= parent < 64):
        continue
    sc = struct.unpack_from("<3f", d, i + 32)
    rot = struct.unpack_from("<3f", d, i + 44)
    pos = struct.unpack_from("<3f", d, i + 56)
    bb = struct.unpack_from("<i", d, i + 68 + 3 * 48)[0]
    if not (0 <= bb <= 6) or not all(abs(v) < 1e4 for v in sc + rot + pos):
        continue
    if not (0.0005 <= abs(sc[0]) <= 100):
        continue
    rows.append((joint, name, parent, bb, pos, sc, rot, hex(flags)))
for r in sorted(set(rows)):
    print("joint %d %-10s parent %2d billboard %d pos (%.2f, %.2f, %.2f) scale %.3f rot (%.2f, %.2f, %.2f) flags %s"
          % (r[0], r[1], r[2], r[3], *r[4], r[5][0], *r[6], r[7]))
