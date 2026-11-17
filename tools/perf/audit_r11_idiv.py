#!/usr/bin/env python3
"""
audit_r11_idiv.py -- the R11 hand-check, mechanised (Forsaken port).

    python3 tools/perf/audit_r11_idiv.py [--all] [--sig] [--bless] [--json]

Contract R11 forbids INTEGER division or modulo by a RUNTIME value in a hot
loop: ARMv6 has no integer divide instruction, so `/` or `%` on integers is a
`bl __aeabi_idiv` / `__aeabi_idivmod` -- tens of cycles PLUS a call barrier that
spills the loop's live registers. A compile-time-constant divisor is fine; GCC
turns it into a magic-number multiply.

This is a SCREEN, NOT A PROOF. "is this integer division" is inferred from the
shape of the operands (a float divide is hardware `vdiv`, R7's concern, not
R11's), and "inside a hot loop" is approximated as "in a file that has a loop and
is not corpus-cold". A hit is a CANDIDATE for a human to confirm; a clean run
means "nothing obvious", never "proven absent".

Scope is the in-scope sources of the 3DS build's trees (imported from gates_vfp) and the cold
list is the corpus cold_always set, so the three screens agree on "the tree".

SIGNATURES ARE LINE-INDEPENDENT (--sig). The baseline keys on
`file + divisor + normalized statement text`, not file:line, so an edit above a
site does not report the SAME site as new (the lesson T2K's baseline learned).
The adoption-day candidates live in contracts/r11_candidates_baseline.txt; a NEW
signature fails the build. Regenerate with --bless.

Known expected hit: new3d.c Random_Range `( Seed1 & 32767 ) % Max`.
"""

import argparse
import json
import os
import re
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from gates_vfp import in_scope_files, load_corpus, source_trees, DEFAULT_CORPUS  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
BASELINE = Path(__file__).resolve().parent / "contracts" / "r11_candidates_baseline.txt"

# A divisor that is one of these is compile-time constant -> magic-number
# multiply, not a call. Collected from the tree rather than hardcoded.
# Integer-type alternation covers the BSD-style typedefs this tree uses
# (`u_int16_t`, `int16_t`, `uint16_t`), not just `uint16_t`. Without `u_int` the
# divisor `Max` in `Random_Range( u_int16_t Max )` is not seen as integer, and
# because the same file also declares `float Max` (Random_Range_Float) the
# file-level name test would call `( Seed1 & 32767 ) % Max` a float divide -- the
# contract's named expected R11 hit would vanish. `u?_?int\d+_t` matches all three.
CONST_DECL = re.compile(
    r"\b(?:constexpr|const)\s+(?:static\s+)?(?:unsigned\s+|signed\s+)?"
    r"(?:int|long|short|size_t|u?_?int\d+_t|char)\s+([A-Za-z_]\w*)\s*=")
# The strict variant: `constexpr` only. A plain `const` on a local is a local,
# not a named constant. In pure C this finds nothing, so the UPPER_CASE convention
# and enum members carry the constant-divisor filter.
CONST_DECL_STRICT = re.compile(
    r"\bconstexpr\s+(?:static\s+)?(?:unsigned\s+|signed\s+)?"
    r"(?:int|long|short|size_t|u?_?int\d+_t|char)\s+([A-Za-z_]\w*)\s*=")
ENUM_MEMBER = re.compile(r"^\s*([A-Z][A-Z0-9_]{2,})\s*(?:=|,)")

# `a / b` or `a % b` where the divisor is a bare identifier or a member read --
# a RUNTIME value. A numeric-literal divisor is excluded by the pattern itself.
DIV = re.compile(r"[/%]\s*(?!/|\*|=)\s*([A-Za-z_]\w*(?:(?:\.|::)\w+)*)\s*(?![\w.(\[]|::)")
QUALIFIER = re.compile(r"::|\.")
STATIC_ASSERT = re.compile(r"\bstatic_assert\b")
BLOCK_COMMENT = re.compile(r"/\*.*?\*/", re.S)
STRING_LIT = re.compile(r'"(?:[^"\\\n]|\\.)*"')
INCLUDE_LINE = re.compile(r"^[ \t]*#[ \t]*include[ \t]*<[^>]*>", re.M)
LOOP = re.compile(r"\b(for|while)\s*\(")

FLOAT_STRONG = re.compile(r"\d\.\d|\dF\b|\df\b|\bfloat\b|\bdouble\b", re.IGNORECASE)
FLOAT_NAME = re.compile(
    r"\b((?:inv|scale|alpha|ratio|frac|norm|len|dist|[xyzuvw])[A-Z_]?\w*)\s*[/%]",
    re.IGNORECASE)
INT_DECL = re.compile(
    r"\b(?:unsigned\s+|signed\s+)?(?:int|long|short|size_t|u?_?int\d+_t|char|bool)"
    r"\s+([A-Za-z_]\w*)")
FLOAT_DECL = re.compile(r"\b(?:float|double)\s*\*?\s*([A-Za-z_]\w*)")

EXEMPT_PIN = re.compile(r"/\*\s*@vfp-exempt\s+(?P<rules>[^\u2014]*?)(?:\u2014|\*/)", re.S)
RULE_ID = re.compile(r"\bR\d+\b")
PIN_REACH = 4


def collect_constants(files):
    """Names believed to be compile-time-constant divisors (constexpr + enum)."""
    legacy = os.environ.get("VFP_R11_CONSTSET", "").strip().lower() in (
        "legacy", "loose", "const")
    decl = CONST_DECL if legacy else CONST_DECL_STRICT
    names = set()
    for path in files:
        try:
            txt = path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        names.update(decl.findall(txt))
        for line in txt.splitlines():
            m = ENUM_MEMBER.match(line)
            if m:
                names.add(m.group(1))
    return names


def load_cold():
    try:
        return load_corpus(DEFAULT_CORPUS).get("cold_always", [])
    except OSError:
        return []


def is_cold(rel, cold):
    return any(rel == c or rel.startswith(c.rstrip("/")) for c in cold)


def int_operand_names(text):
    return set(INT_DECL.findall(text)) - set(FLOAT_DECL.findall(text))


def float_operand_names(text):
    return set(FLOAT_DECL.findall(text)) - set(INT_DECL.findall(text))


OPERAND_BEFORE = re.compile(r"([A-Za-z_]\w*)\s*[/%]")
OPERAND_AFTER = re.compile(r"[/%]\s*([A-Za-z_]\w*)")
MEMBER_NAME = re.compile(r"(?:->|\.)\s*([A-Za-z_]\w*)")


def header_float_members():
    """Names declared `float`/`double` in root-tree headers. Used ONLY to judge
    `->member`/`.member` operands, never bare locals, so a header `float x;`
    cannot silence an integer `x % n` in a .c file. Name test, not proof."""
    names = set()
    # Every header in all three source trees: the same set the flat root held
    # (PC-tree headers included, as they were before the reorg).
    headers = [h for t in source_trees(include_pc=True) for h in t.rglob("*.h")]
    for h in sorted(headers, key=lambda h: h.name):
        try:
            txt = h.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        txt = BLOCK_COMMENT.sub(lambda m: re.sub(r"[^\n]", " ", m.group(0)), txt)
        names.update(FLOAT_DECL.findall(txt))
    return names


def looks_float(code, int_names, float_names=frozenset(), member_floats=frozenset()):
    """Is the `/` or `%` on this line a float operation? (name test, not proof)"""
    if FLOAT_STRONG.search(code):
        return True
    for m in FLOAT_NAME.finditer(code):
        if m.group(1) not in int_names:
            return True
    if float_names:
        for rx in (OPERAND_BEFORE, OPERAND_AFTER):
            for m in rx.finditer(code):
                if m.group(1) in float_names:
                    return True
    if member_floats:
        for m in MEMBER_NAME.finditer(code):
            if m.group(1) in member_floats:
                return True
    return False


def collect_pins(raw):
    """line -> set of rule ids pinned at that line, read from the RAW text."""
    pins = {}
    for m in EXEMPT_PIN.finditer(raw):
        rules = set(RULE_ID.findall(m.group("rules")))
        if rules:
            line = raw.count("\n", 0, m.start()) + 1
            pins.setdefault(line, set()).update(rules)
    return pins


def pinned(pins, line, rule):
    for pin_line, rules in pins.items():
        if pin_line <= line <= pin_line + PIN_REACH and rule in rules:
            return True
    return False


def norm_stmt(code):
    return " ".join(code.split())


def scan(show_all=False):
    cold = load_cold()
    files = in_scope_files()
    consts = collect_constants(files)
    hfloats = header_float_members()
    hits, filtered = [], {"cold_file": 0, "not_in_loop": 0,
                          "constant_divisor": 0, "float_expr": 0,
                          "static_assert": 0, "pinned": 0}

    for path in files:
        rel = path.name
        try:
            raw = path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        pins = collect_pins(raw)
        raw = BLOCK_COMMENT.sub(lambda m: re.sub(r"[^\n]", " ", m.group(0)), raw)
        raw = STRING_LIT.sub(lambda m: '"' + " " * (len(m.group(0)) - 2) + '"', raw)
        raw = INCLUDE_LINE.sub(lambda m: re.sub(r"[^\n]", " ", m.group(0)), raw)
        lines = raw.splitlines()
        cold_file = is_cold(rel, cold)
        int_names = int_operand_names(raw)
        float_names = float_operand_names(raw)
        # The loop test is FILE-LEVEL, not brace-scoped, and that is deliberate
        # (T2K's lesson): the hottest R11 sites are straight-line code in helpers
        # whose CALLERS loop, so brace-scoping would filter them out.
        file_has_loop = any(LOOP.search(ln.split("//", 1)[0]) for ln in lines)

        for i, line in enumerate(lines, 1):
            code = line.split("//", 1)[0]
            if STATIC_ASSERT.search(code) and DIV.search(code):
                filtered["static_assert"] += 1
                if not show_all:
                    continue
            for m in DIV.finditer(code):
                divisor = m.group(1).rstrip(".")
                tail = QUALIFIER.split(divisor)[-1]
                in_loop = file_has_loop
                if pinned(pins, i, "R11"):
                    filtered["pinned"] += 1
                    if not show_all:
                        continue
                if cold_file:
                    filtered["cold_file"] += 1
                    if not show_all:
                        continue
                elif not in_loop:
                    filtered["not_in_loop"] += 1
                    if not show_all:
                        continue
                elif (tail in consts or tail.isupper()
                      or divisor in consts or divisor.isupper()):
                    filtered["constant_divisor"] += 1
                    if not show_all:
                        continue
                elif looks_float(code, int_names, float_names, hfloats):
                    filtered["float_expr"] += 1
                    if not show_all:
                        continue
                key = (rel, i, divisor)
                prev = next((h for h in hits if h["key"] == key), None)
                if prev:
                    prev["count"] += 1          # `((x%n)+n)%n` is TWO calls
                    continue
                hits.append({"key": key, "count": 1,
                             "file": rel, "line": i, "divisor": divisor,
                             "cold": cold_file, "in_loop": in_loop,
                             "stmt": norm_stmt(code),
                             "text": code.strip()[:100]})
    for h in hits:
        h["sig"] = "%s  divisor=%s  x%d  %s" % (
            h["file"], h["divisor"], h["count"], h["stmt"])
    return hits, filtered, len(files)


def load_baseline_sigs(path):
    counts = Counter()
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return counts
    for line in text.splitlines():
        if not line.strip() or line.startswith("#"):
            continue
        counts[line.strip()] += 1
    return counts


BASELINE_HEADER = """\
# R11 candidate baseline -- contracts/vfp_hotpath.md R11 (screen, not proof).
# One LINE-INDEPENDENT signature per candidate integer div/mod site reported by
# tools/perf/audit_r11_idiv.py --sig:
#     <file>  divisor=EXPR  xN  <normalized statement>
# A NEW signature fails the build; a removed signature (fixed site) passes with a
# note. Line numbers are absent on purpose: a file:line baseline reports the SAME
# site as new after any edit above it. Regenerate with:
#     python3 tools/perf/audit_r11_idiv.py --bless
"""


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--all", action="store_true",
                    help="report every division site, including the filtered ones")
    ap.add_argument("--json", action="store_true")
    ap.add_argument("--sig", action="store_true",
                    help="print only line-independent signatures (for baselining)")
    ap.add_argument("--bless", action="store_true",
                    help="write current signatures into the baseline and exit")
    args = ap.parse_args()

    hits, filtered, nfiles = scan(args.all)

    if args.sig:
        for h in hits:
            print(h["sig"])
        return 0

    if args.bless:
        sigs = Counter(h["sig"] for h in hits)
        lines = []
        for s in sorted(sigs):
            lines.extend([s] * sigs[s])
        BASELINE.parent.mkdir(parents=True, exist_ok=True)
        BASELINE.write_text(BASELINE_HEADER + "\n".join(lines) + ("\n" if lines else ""),
                            encoding="utf-8")
        print(f"blessed {sum(sigs.values())} R11 candidate signature(s) into {BASELINE.name}")
        return 0

    allowed = load_baseline_sigs(BASELINE)
    new, baselined = [], 0
    for h in hits:
        if allowed.get(h["sig"], 0) > 0:
            allowed[h["sig"]] -= 1
            baselined += 1
        else:
            new.append(h)

    if args.json:
        json.dump({"hits": hits, "new": new, "baselined": baselined,
                   "filtered": filtered, "files": nfiles}, sys.stdout, indent=2)
        return 1 if new and not args.all else 0

    print("R11 audit -- integer / or % by a RUNTIME value inside a loop (Forsaken)")
    print("  scanned %d root in-scope files" % nfiles)
    print("  filtered: %d in cold_always files, %d not in a loop, "
          "%d constant divisor, %d float expression, %d static_assert"
          % (filtered["cold_file"], filtered["not_in_loop"],
             filtered["constant_divisor"], filtered["float_expr"],
             filtered["static_assert"]))
    print("  pinned: %d suppressed by an @vfp-exempt R11 pin" % filtered["pinned"])
    print("  baselined: %d (adoption debt — burn down, never grow); NEW: %d"
          % (baselined, len(new)))
    if not new:
        print("\n  no NEW candidates.")
        print("  NB this is a SCREEN, not a proof -- 'is it integer' and 'is it"
              " hot' are both\n     inferred, so a clean run means nothing obvious,"
              " never nothing present.")
        return 0
    print("\n  %d NEW candidate(s) -- confirm each against a real profile:" % len(new))
    for h in new:
        mult = "  x%d calls on this line" % h["count"] if h["count"] > 1 else ""
        print("    %s:%d  divisor=%s%s" % (h["file"], h["line"], h["divisor"], mult))
        print("        %s" % h["text"])
    return 1


if __name__ == "__main__":
    sys.exit(main())
