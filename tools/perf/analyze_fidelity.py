#!/usr/bin/env python3
"""analyze_fidelity.py -- compare two fidelity-sweep artifacts.

    python3 tools/perf/analyze_fidelity.py BASELINE.txt CANDIDATE.txt

Compares per-level final hashes (FID: level=... ticks=... hash=...) and, when
present, per-checkpoint hashes (FIDC: tick=N hash=... [ship=... enem=...
prim=... sec=...]) to localize the first diverging tick and, with the
component digests, the diverging subsystem.

Levels where the hashes disagree are reported with the first checkpoint tick
at which the two runs differ, plus every component that disagrees at that
tick (component digests exist only in FIDELITY_CHECKPOINTS=1 builds and are
additive to the global hash, so old-format files still compare on hash=).

Exit code 0 if the files are identical, 1 otherwise (usable as a merge gate).
"""

import re
import sys


FID_RE = re.compile(r"^FID: level=(\S+) ticks=(\d+) hash=([0-9a-f]{8})")
FIDC_RE = re.compile(r"^FIDC: tick=(\d+) hash=([0-9a-f]{8})"
                     r"(?:\s+ship=([0-9a-f]{8}))?(?:\s+enem=([0-9a-f]{8}))?"
                     r"(?:\s+prim=([0-9a-f]{8}))?(?:\s+sec=([0-9a-f]{8}))?")


def load(path):
    """Parse a fidelity artifact.

    File layout: a level's FIDC checkpoints are written DURING the level and
    its 'FID:' line at the level's END, so checkpoints precede the FID line
    they belong to. Parse with a pending bucket: FIDC lines accumulate, and
    the next FID line claims the bucket.
    """
    levels = {}
    order = []
    checks = []
    pending = []
    with open(path, "r", encoding="utf-8", errors="replace") as fh:
        for line in fh:
            m = FID_RE.match(line)
            if m:
                name, ticks, h = m.group(1), int(m.group(2)), m.group(3)
                levels[name] = (ticks, h)
                order.append(name)
                checks.append((name, list(pending)))
                pending = []
                continue
            m = FIDC_RE.match(line)
            if m:
                tick = int(m.group(1))
                comps = {}
                for key, idx in (("hash", 2), ("ship", 3), ("enem", 4),
                                 ("prim", 5), ("sec", 6)):
                    if m.group(idx):
                        comps[key] = m.group(idx)
                pending.append((tick, comps))
    return levels, order, checks


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    la, order_a, ca = load(sys.argv[1])
    lb, order_b, cb = load(sys.argv[2])
    identical = True

    print("fidelity A/B -- %s vs %s" % (sys.argv[1], sys.argv[2]))
    if order_a != order_b:
        print("  LEVEL LIST DIFFERS: %s vs %s -- files from different sweeps?"
              % (len(order_a), len(order_b)))
        identical = False
        common = [l for l in order_a if l in lb]
    else:
        common = order_a

    for name in common:
        ta, ha = la[name]
        tb, hb = lb[name]
        if ha == hb:
            continue
        identical = False
        print("\n  %s: hash differs (A=%s B=%s, ticks %d/%d)"
              % (name, ha, hb, ta, tb))
        # first divergent checkpoint + components
        seq_a = dict(p for x in ca if x[0] == name for p in x[1])
        seq_b = dict(p for x in cb if x[0] == name for p in x[1])
        first = None
        for tick in sorted(set(seq_a) | set(seq_b)):
            if seq_a.get(tick) != seq_b.get(tick):
                first = tick
                break
        if first is None:
            if not seq_a or not seq_b:
                print("    no checkpoint lines in one or both files"
                      " (FIDELITY_CHECKPOINTS=1 builds record them)")
            else:
                print("    checkpoints agree up to the shorter run's end"
                      " (divergence is at level end or mid-tick)")
            continue
        ca_h, cb_h = seq_a.get(first, {}), seq_b.get(first, {})
        print("    first divergent checkpoint: tick %d" % first)
        print("      A hash=%s  B hash=%s" % (ca_h.get("hash"), cb_h.get("hash")))
        # components only when BOTH sides record them (old-format runs have
        # hash only; a missing component on one side is not a divergence)
        comps = [k for k in ("ship", "enem", "prim", "sec")
                 if k in ca_h and k in cb_h and ca_h[k] != cb_h[k]]
        if comps:
            print("      diverging components at that tick: %s" % ", ".join(comps))
            for k in comps:
                print("        %s: A=%s B=%s" % (k, ca_h[k], cb_h[k]))
        elif "ship" in ca_h and "ship" in cb_h:
            print("      component digests agree at that tick-- divergence is"
                  " inside the global hash's fold order (tick counter?)")
        else:
            print("      (component digests not recorded on both sides)")

    if identical:
        print("\n  VERDICT: IDENTICAL -- byte-for-byte per-level state.")
        return 0
    n_diff = sum(1 for n in common if la[n][1] != lb[n][1])
    print("\n  VERDICT: DIFFERS -- %d level(s) diverge." % n_diff)
    return 1


if __name__ == "__main__":
    sys.exit(main())
