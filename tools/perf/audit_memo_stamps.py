#!/usr/bin/env python3
"""audit_memo_stamps.py — opt5b fill-epoch coverage gate.

The stereo memo (stereomemo.c) is safe only if every shared exec-buffer
fill stamps its content identity (memoEpochPass/Group/TPage); an
unstamped shared fill compares UNSET==UNSET and can false-hit another
batch's recorded vertices (the original 78k-mismatch aliasing bug).

This screen makes that enumeration mechanical instead of manual:
  1. Every known shared fill core must assign memoEpochPass in its body.
  2. Any *new* fill-shaped function (name matches *DispGroup* and it
     writes textureGroups or locks a vertex buffer) that is neither
     stamped nor explicitly allowlisted fails the gate.

Allowlist entries must state why the path can never reach the memo
(ortho-only draws are excluded from memo lookup by draw_render_object).
Pure stdlib; exit 0 = PASS.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
from gates_vfp import find_source  # noqa: E402  (files are keyed by basename)

# file -> {function: expected SMEMO_PASS_* constant}
REQUIRED = {
    "polys.cpp": {
        "PolyDispGroupClipped": "SMEMO_PASS_POLYS_CLIPPED",
        "PolyDispGroupUnclipped": "SMEMO_PASS_POLYS_UNCLIPPED",
    },
    "2dpolys.cpp": {
        "FmPolyDispGroupClipped": "SMEMO_PASS_FM_CLIPPED",
        "FmPolyDispGroupUnclipped": "SMEMO_PASS_FM_UNCLIPPED",
    },
    "lines.cpp": {
        "LinesDispGroup": "SMEMO_PASS_LINES",
    },
}

# Fill-shaped but can never reach the (non-ortho) memo lookup.
ALLOWLIST = {
    "ScrPolyDispSolid": "ortho screen polys: draw_2d_object only, memo is non-ortho only",
    "ScrPolyDispNonSolid": "ortho screen polys: draw_2d_object only, memo is non-ortho only",
}

SCAN_FILES = ["polys.cpp", "2dpolys.cpp", "lines.cpp", "screenpolys.cpp",
              "transexe.cpp", "water.cpp", "mxaload.cpp", "mxload.cpp", "mload.cpp"]

FUNC_RE = re.compile(r"^bool\s+([A-Za-z_]\w*)\s*\(", re.MULTILINE)
FILL_HINT_RE = re.compile(r"FSLockVertexBuffer|textureGroups\[|numTextureGroups\s*=\s*0")


def function_bodies(text):
    """Yield (name, body) using brace matching from each `bool name(`."""
    for m in FUNC_RE.finditer(text):
        brace = text.find("{", m.end())
        if brace < 0:
            continue
        depth = 0
        i = brace
        while i < len(text):
            if text[i] == "{":
                depth += 1
            elif text[i] == "}":
                depth -= 1
                if depth == 0:
                    break
            i += 1
        yield m.group(1), text[brace:i + 1]


def main():
    failures = []
    bodies = {}  # (file, func) -> body
    for fname in SCAN_FILES:
        p = find_source(fname)
        if p is None:
            # A scan file that cannot be found must FAIL, not skip: a skip
            # turns this gate into a silent no-op the moment files move.
            failures.append(f"{fname}: not found in the source trees (moved/renamed? update gate)")
            continue
        for name, body in function_bodies(p.read_text(errors="replace")):
            bodies[(fname, name)] = body

    # 1. Required stamps present with the right pass constant.
    for fname, funcs in REQUIRED.items():
        for func, want in funcs.items():
            body = bodies.get((fname, func))
            if body is None:
                failures.append(f"{fname}:{func} not found (renamed/removed? update gate)")
                continue
            if f"memoEpochPass = {want}" not in body:
                failures.append(f"{fname}:{func} missing stamp memoEpochPass = {want}")
            if "memoEpochGroup" not in body or "memoEpochResume" not in body:
                failures.append(f"{fname}:{func} stamp incomplete (need Group+TPage)")

    # 2. No unstamped, unallowlisted fill-shaped function anywhere scanned.
    stamped = {(f, fn) for f, fns in REQUIRED.items() for fn in fns}
    for (fname, name), body in bodies.items():
        if (fname, name) in stamped or name in ALLOWLIST:
            continue
        if "DispGroup" not in name and "DispSolid" not in name and "DispNonSolid" not in name:
            continue
        if not FILL_HINT_RE.search(body):
            continue
        failures.append(
            f"{fname}:{name} looks like a shared fill but has no memo stamp "
            f"and is not allowlisted — stamp it or allowlist with a reason")

    if failures:
        print("FAIL memo-stamp coverage:")
        for f in failures:
            print(f"  {f}")
        return 1
    n = sum(len(v) for v in REQUIRED.values())
    print(f"PASS memo stamps: {n} fill cores stamped, "
          f"{len(ALLOWLIST)} allowlisted, 0 unstamped fills")
    return 0


if __name__ == "__main__":
    sys.exit(main())
