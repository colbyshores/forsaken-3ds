#!/usr/bin/env python3
"""Probe a Forsaken .cob file: hierarchy + NumTrans/NumZones per child."""
import struct, sys

TYPES = {0:"TRANS",1:"ROT",2:"MORPH",3:"PROP"}

def parse_zones(data, off):
    nz, = struct.unpack_from('<h', data, off); off += 2
    for _ in range(nz):
        ztype, sens = struct.unpack_from('<hh', data, off); off += 4
        damage, = struct.unpack_from('<f', data, off); off += 4
        off += 24  # box center + halfsize
        if ztype == 0:
            pass
        else:
            ns, = struct.unpack_from('<h', data, off); off += 2
            for _ in range(ns):
                off += 22
    return off, nz

def parse_trans(data, off):
    nt, = struct.unpack_from('<h', data, off); off += 2
    types = []
    for _ in range(nt):
        ttype, = struct.unpack_from('<h', data, off); off += 2
        ts, dur = struct.unpack_from('<ff', data, off); off += 8
        if ttype == 0:    off += 14
        elif ttype == 1:  off += 30
        elif ttype == 2:  off += 2
        elif ttype == 3:
            ptype, = struct.unpack_from('<h', data, off); off += 2
            if ptype == 3:
                while data[off] != 0: off += 1
                off += 1
            else:
                off += 2
        types.append(ttype)
    return off, nt, types

def parse_child(data, off, depth, results):
    cid, = struct.unpack_from('<h', data, off); off += 2
    off, nt, ttypes = parse_trans(data, off)
    off, nz = parse_zones(data, off)
    nc, = struct.unpack_from('<h', data, off); off += 2
    results.append((depth, cid, nt, nz, nc, ttypes))
    for _ in range(nc):
        off = parse_child(data, off, depth+1, results)
    return off

def main():
    path = sys.argv[1]
    data = open(path, 'rb').read()
    print(f"=== {path} ({len(data)} bytes) ===")
    off = 0
    magic, version = struct.unpack_from('<II', data, off); off += 8
    print(f"  magic={magic:#x} ver={version:#x}")
    n_models, = struct.unpack_from('<h', data, off); off += 2
    print(f"  models: {n_models}")
    for _ in range(n_models):
        end = data.index(b'\x00', off)
        print(f"    {data[off:end].decode(errors='replace')}")
        off = end + 1
    results = []
    try:
        off = parse_child(data, off, 0, results)
    except struct.error as e:
        print(f"  parse stopped: {e}")
    print(f"  consumed {off}/{len(data)} bytes")
    print()
    total_trans = sum(r[2] for r in results)
    print(f"  total components: {len(results)}, total Trans: {total_trans}")
    print(f"  {'depth':<6}{'id':<5}{'#tr':<5}{'#zon':<5}{'#kid':<5}  trans-types")
    for d, cid, nt, nz, nc, tts in results:
        ind = '  '*d
        ts_summary = ",".join(TYPES.get(t,str(t)) for t in tts[:8]) or '-'
        print(f"  {ind}{d}    {cid:<5}{nt:<5}{nz:<5}{nc:<5}  {ts_summary}")

main()
