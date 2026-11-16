#!/usr/bin/env python3
"""ARM11 math audit for Forsaken: trig-LUT coverage + double residue.

Ported from T2K's ``tools/runner/audit_arm11_math.py`` and re-derived for this
tree. Helpers are imported from ``gates_vfp`` (same directory) so scope, corpus,
and the adoption-debt baseline ratchet stay a single source of truth.

Two screens, both feeding the SAME baseline ratchet as gates_vfp
(``contracts/hot_findings_baseline.txt``) under rules TRIG and R4-D:

  TRIG   Raw libm trig in a compiled root source must sit in a policy exception
         (new3d.cpp LUT construction, title.c cold title) or carry an
         ``@vfp-exempt R6`` pin; otherwise it must move to fast_sinf/fast_cosf.

  R4-D   ``double`` residue inside a corpus-hot function without an
         ``@vfp-exempt R4`` pin (contract §2 R4). MECHANICS_GATED is empty for
         now: Forsaken's ported gameplay doubles are burn-down items reviewed
         individually, not a blanket gate.

Forsaken has no PC tree, so T2K's audit_trig_pc is dropped.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from gates_vfp import (  # noqa: E402
    DEFAULT_CORPUS,
    HOT_FINDINGS_BASELINE,
    Finding,
    Function,
    filter_baseline,
    in_scope_files,
    line_of,
    load_corpus,
    parse_functions,
    seed_map,
    strip_comments_keep_lines,
    write_baseline,
)

# Raw libm trig: bare C names (double) and f-suffixed. Forsaken is pure C, so no
# std:: qualification.
TRIG_RE = re.compile(
    r"\b(sin|cos|tan|asin|acos|atan|atan2|sinf|cosf|tanf"
    r"|asinf|acosf|atanf|atan2f)\s*\("
)
DOUBLE_RE = re.compile(r"\bdouble\b")

# Policy exceptions, keyed by root file name.
ALLOWED_FILES: dict[str, str] = {
    "new3d.cpp": "ALLOWED-LUT-CONSTRUCTION",  # was new3d.c pre-conversion
    "title.c": "ALLOWED-COLD-TITLE",
}

# Files whose gameplay math is ported in DOUBLE on purpose. Empty for now: each
# Forsaken double residue is reviewed individually (contract §2 R4), not gated as
# a class.
MECHANICS_GATED: tuple[str, ...] = ()


def raw_line(clean: str, idx: int) -> int:
    """strip_comments_keep_lines preserves line structure, so the cleaned-text
    line number is the raw-text line number."""
    return line_of(clean, idx)


def has_pin_above(raw: str, line: int, rule: str, max_back: int = 25) -> bool:
    """Honour an existing @vfp-exempt <rule> pin within max_back lines above."""
    lines = raw.split("\n")
    lo = max(0, line - 1 - max_back)
    window = "\n".join(lines[lo:line])
    return bool(re.search(r"@vfp-exempt\b[^*]*\b" + rule + r"\b", window))


def function_of(funcs: list[Function], index: int) -> Function | None:
    for f in funcs:
        if f.start <= index < f.end:
            return f
    return None


def load_hot() -> dict[str, object]:
    corpus = load_corpus(DEFAULT_CORPUS)
    return seed_map(corpus, [])


def is_hot(seed: dict[str, object], rel: str, fname: str) -> bool:
    entry = seed.get(rel)
    if entry is None:
        return False
    if fname in (getattr(entry, "cold_symbols", ()) or ()):
        return False
    syms = getattr(entry, "symbols", None) or []
    return "*" in syms or fname in syms


def audit(seed: dict[str, object]) -> tuple[list[Finding], list[Finding], list[Finding], list[Finding]]:
    """Return (trig_violations, trig_allowed, r4_offenders, r4_allowed)."""
    trig_violations: list[Finding] = []
    trig_allowed: list[Finding] = []
    r4_offenders: list[Finding] = []
    r4_allowed: list[Finding] = []
    for path in in_scope_files():
        rel = path.name
        raw = path.read_text(encoding="utf-8", errors="replace")
        clean = strip_comments_keep_lines(raw)
        funcs = parse_functions(clean)

        for m in TRIG_RE.finditer(clean):
            idx = m.start()
            prefix = clean[max(0, idx - 6): idx + 1]
            if "fast" in prefix:  # inside the fast_* wrappers themselves
                continue
            ln = raw_line(clean, idx)
            fn = function_of(funcs, idx)
            fname = fn.name if fn else "<file scope>"
            call = m.group(0).rstrip("(").strip()
            hot = is_hot(seed, rel, fname)
            finding = Finding(
                rule="TRIG", severity="MUST", file=rel, line=ln, symbol=fname,
                hotpath=hot, hotpath_reason=("seed" if hot else "cold"),
                exemption=None, detail=f"raw libm trig {call} in {fname}",
            )
            policy = ALLOWED_FILES.get(rel)
            if policy is None and has_pin_above(raw, ln, "R6"):
                policy = "ALLOWED-R6-PIN"
            if policy:
                finding.exemption = policy
                trig_allowed.append(finding)
            else:
                trig_violations.append(finding)

        for m in DOUBLE_RE.finditer(clean):
            idx = m.start()
            ln = raw_line(clean, idx)
            fn = function_of(funcs, idx)
            fname = fn.name if fn else "<file scope>"
            if rel in MECHANICS_GATED:
                r4_allowed.append(Finding("R4-D", "MUST", rel, ln, fname, False,
                                          "mechanics-gated", "MECHANICS-GATED",
                                          f"double in {fname}"))
                continue
            if fn and is_hot(seed, rel, fname):
                if has_pin_above(raw, ln, "R4", max_back=40):
                    r4_allowed.append(Finding("R4-D", "MUST", rel, ln, fname, True,
                                              "seed", "HOT-R4-PINNED",
                                              f"double in hot function {fname}"))
                else:
                    r4_offenders.append(Finding("R4-D", "MUST", rel, ln, fname, True,
                                                "seed", None,
                                                f"double in hot function {fname}"))
            else:
                r4_allowed.append(Finding("R4-D", "MUST", rel, ln, fname, False,
                                          "cold", "cold", f"double in {fname}"))
    return trig_violations, trig_allowed, r4_offenders, r4_allowed


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--json", action="store_true", help="machine-readable output")
    ap.add_argument("--bless", action="store_true",
                    help="write current TRIG/R4-D findings into the shared baseline and exit")
    args = ap.parse_args()

    seed = load_hot()
    trig_v, trig_ok, r4_off, r4_ok = audit(seed)
    debt = trig_v + r4_off  # the findings that feed the ratchet

    if args.bless:
        n = write_baseline(HOT_FINDINGS_BASELINE, debt, owned_rules={"TRIG", "R4-D"})
        print(f"blessed {n} TRIG/R4-D finding(s) into {HOT_FINDINGS_BASELINE.name}")
        return 0

    new, baselined = filter_baseline(debt, HOT_FINDINGS_BASELINE)
    new_trig = [f for f in new if f.rule == "TRIG"]
    new_r4 = [f for f in new if f.rule == "R4-D"]
    base_trig = [f for f in baselined if f.rule == "TRIG"]
    base_r4 = [f for f in baselined if f.rule == "R4-D"]

    if args.json:
        print(json.dumps({
            "trig_violations": [f.__dict__ for f in trig_v],
            "trig_allowed": [f.__dict__ for f in trig_ok],
            "r4_offenders": [f.__dict__ for f in r4_off],
            "r4_allowed_count": len(r4_ok),
            "new": [f.__dict__ for f in new],
            "baselined": len(baselined),
        }, indent=2))
        return 1 if new else 0

    print("=" * 74)
    print("ARM11 MATH AUDIT — trig LUT coverage + double residue (Forsaken)")
    print("=" * 74)

    print(f"\nTRIG — raw libm trig in compiled sources: {len(trig_v) + len(trig_ok)} sites")
    by_pol: dict[str, int] = Counter(f.exemption for f in trig_ok)
    print(f"  policy exceptions: {len(trig_ok)}")
    for pol, n in sorted(by_pol.items()):
        print(f"    {pol}: {n}")
    print(f"  violations (not allowed/pinned): {len(trig_v)}  "
          f"[baselined {len(base_trig)}, NEW {len(new_trig)}]")
    for f in new_trig[:20]:
        print(f"    {f.file}:{f.line}  {f.detail}  [{f.hotpath_reason}]")

    print(f"\nR4-D — double residue in hot functions: {len(r4_off) + len(r4_ok)} sites")
    print(f"  offenders (hot, unpinned): {len(r4_off)}  "
          f"[baselined {len(base_r4)}, NEW {len(new_r4)}]")
    for f in new_r4[:20]:
        print(f"    {f.file}:{f.line}  {f.detail}")

    if new:
        print(f"\nVERDICT: FAIL — {len(new)} new TRIG/R4-D finding(s) not in baseline")
        return 1
    print("\nVERDICT: CONTRACT CLEAN (clean-or-baselined)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
