#!/usr/bin/env python3
"""
audit_c_with_classes.py -- Forsaken AGENTS.md section 9, mechanised.

    python3 tools/perf/audit_c_with_classes.py [--json]

Ported from T2K's tools/runner/audit_c_with_classes.py (which gates T2K
AGENTS.md section 10). The rule set, the comment/string stripper and the
zero-tolerance framing are T2K's; the scope is Forsaken's -- the same
build-derived scope the numeric gate uses (root-level sources compiled by
Makefile.3ds, minus EXCLUDE_SRC), so the two gates can never disagree about
which files exist.

Forsaken AGENTS.md 9 keeps the tree at "C with classes": no virtual dispatch,
no RTTI, no exceptions, no project-defined templates, no type-erased
callables. Forsaken is C today, so this gate is green with zero C++ files
scanned -- but it is enabled NOW, not in the change that adds the first .cpp,
so the contract is enforced before there is anything to enforce it against.

WHAT THIS CAN SEE. Every rule here is a ZERO-TOLERANCE construct: the tree
contains exactly none of them, so the gate is green today and any hit is new
code, not baseline drift. That is what makes a hard pass/fail honest rather
than a re-blessing chore. There is no baseline file because there should never
be a need for one -- if this reports a hit, the fix is to not write that
construct, not to bless it.

WHAT IT CANNOT SEE, and why the rules still need a human:

  * It is lexical. Comments and string literals are stripped by a small state
    machine BEFORE matching (see strip_noise), because the tree documents its
    own rules in prose and a naive grep produced false positives on those
    words. Line numbers are preserved through the strip, so a report still
    points at the real line.
  * It cannot tell HOT from COLD. That is deliberate: section 9 is NOT a
    hot-path rule set. Virtual dispatch is banned because the renderer seam
    is chosen at BUILD time (render_c3d.c vs render_gl*.c), not because a
    vtable call is expensive; templates are banned as house style, not
    because they are slow on ARM11. Scoping these to the hot-path corpus
    would be a category error.
  * It does NOT check the per-frame heap-growth rule (section 9, HOT PATH).
    That one is violated by correct shipped design in this tree too (level
    and execbuf arrays grow on load), so a gate would either fail on correct
    code or need a baseline that launders the rule. It stays a review check.
    A screen that cries wolf is worse than none.
  * It cannot check the CONVERSION-EQUIVALENCE rule (a converted module must
    produce byte-identical gameplay state). That gate is the fidelity
    harness: sdmc:/forsaken_fidelity.txt, per-tick state checksums, run
    baseline-vs-converted on hardware. See AGENTS.md section 9.

Exit code 1 if any violation is found.
"""

import argparse
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from gates_vfp import REPO_ROOT, in_scope_files  # noqa: E402

# (rule, what, pattern) -- matched against comment/string-free source.
RULES = [
    ("C1", "virtual function/destructor", re.compile(r"\bvirtual\b")),
    ("C1", "override specifier",         re.compile(r"\)\s*override\b")),
    ("C2", "dynamic_cast",              re.compile(r"\bdynamic_cast\s*<")),
    ("C2", "typeid",                    re.compile(r"\btypeid\b")),
    ("C2", "#include <typeinfo>",       re.compile(r"^[ \t]*#[ \t]*include[ \t]*<typeinfo>", re.M)),
    ("C3", "throw",                     re.compile(r"\bthrow\b")),
    ("C3", "try block",                 re.compile(r"\btry\s*\{")),
    ("C3", "catch clause",              re.compile(r"\bcatch\s*\(")),
    ("C3", "#include <exception|stdexcept>",
                                        re.compile(r"^[ \t]*#[ \t]*include[ \t]*<(?:exception|stdexcept)>", re.M)),
    ("C4", "project-defined template",  re.compile(r"^[ \t]*template[ \t]*[<]", re.M)),
    ("C5", "std::function",            re.compile(r"\bstd::function\b")),
    ("C5", "std::bind",                re.compile(r"\bstd::bind\b")),
    ("C5", "#include <functional>",    re.compile(r"^[ \t]*#[ \t]*include[ \t]*<functional>", re.M)),
]

RULE_WHY = {
    "C1": "renderer seam is chosen at BUILD time; a vtable is a runtime decision where the build already made it",
    "C2": "-fno-rtti: does not compile",
    "C3": "-fno-exceptions: does not compile; use return codes / status structs",
    "C4": "house style -- concrete types and ordinary functions",
    "C5": "heap-allocated target + indirect call, no inlining; the seam is free functions over the engine state",
}


def strip_noise(src):
    """Blank out comments and string/char literals, preserving newlines so line
    numbers stay true. The tree writes its own contract into comments, so this
    is what keeps the gate from flagging prose."""
    out = []
    i, n = 0, len(src)
    state = "code"
    quote = ""
    while i < n:
        c = src[i]
        nxt = src[i + 1] if i + 1 < n else ""
        if state == "code":
            if c == "/" and nxt == "/":
                state = "line"
                out.append("  ")
                i += 2
            elif c == "/" and nxt == "*":
                state = "block"
                out.append("  ")
                i += 2
            elif c == '"' or c == "'":
                state = "str"
                quote = c
                out.append(" ")
                i += 1
            else:
                out.append(c)
                i += 1
        elif state == "line":
            if c == "\n":
                state = "code"
                out.append("\n")
            else:
                out.append(" ")
            i += 1
        elif state == "block":
            if c == "*" and nxt == "/":
                state = "code"
                out.append("  ")
                i += 2
            else:
                out.append("\n" if c == "\n" else " ")
            i += 1
        else:
            if c == "\\":
                out.append("  ")
                i += 2
            elif c == quote:
                state = "code"
                out.append(" ")
                i += 1
            else:
                out.append("\n" if c == "\n" else " ")
                i += 1
    return "".join(out)


def scan():
    hits = []
    nfiles = 0
    stripped = 0
    for path in in_scope_files():
        rel = os.path.relpath(path, REPO_ROOT)
        try:
            with open(path, "r", encoding="utf-8", errors="replace") as fh:
                raw = fh.read()
        except OSError:
            continue
        nfiles += 1
        clean = strip_noise(raw)
        if clean != raw:
            stripped += 1
        lines = clean.split("\n")
        for rid, what, pat in RULES:
            for m in pat.finditer(clean):
                lineno = clean.count("\n", 0, m.start()) + 1
                snippet = lines[lineno - 1].strip() if lineno - 1 < len(lines) else ""
                hits.append({
                    "rule": rid, "what": what, "file": rel,
                    "line": lineno, "text": snippet,
                })
    return hits, nfiles, stripped


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--json", action="store_true", help="machine-readable output")
    args = ap.parse_args()

    hits, nfiles, stripped = scan()

    if args.json:
        print(json.dumps({"files": nfiles, "violations": hits}, indent=2))
        return 1 if hits else 0

    print("AGENTS.md 9 audit -- C with classes (virtual / RTTI / exceptions / templates / type erasure)")
    print("  scanned %d file(s) in 3DS build scope" % nfiles)
    print("  comment/string-stripped before matching: %d file(s) contained prose that would"
          % stripped)
    print("  otherwise be matched as code")
    if not hits:
        print("\n  VERDICT: CONTRACT CLEAN -- 0 violations.")
        print("  NB this screens the ZERO-TOLERANCE constructs only. It cannot tell you whether")
        print("  the per-frame heap-growth rule was honoured, nor whether a converted module")
        print("  preserved gameplay state (that is the fidelity harness).")
        return 0

    print("\n  %d VIOLATION(S):" % len(hits))
    for h in sorted(hits, key=lambda x: (x["rule"], x["file"], x["line"])):
        print("    [%s] %s:%d  %s" % (h["rule"], h["file"], h["line"], h["what"]))
        print("        %s" % h["text"])
        print("        why: %s" % RULE_WHY[h["rule"]])
    print("\n  VERDICT: VIOLATIONS -- see AGENTS.md section 9.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
