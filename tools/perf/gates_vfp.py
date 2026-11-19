#!/usr/bin/env python3
"""VFP hot-path gates R1-R10 for the Forsaken 3DS build.

Ported from T2K's ``tools/runner/gates_vfp.py`` (rev 2026-10-19) and re-derived
against this tree. The contract lives in ``contracts/vfp_hotpath.md``; the hot
corpus is ``contracts/hotpath_corpus.json`` (a provisional static seed until a
hardware capture replaces it).

T2K machinery preserved VERBATIM (strip_comments_keep_lines, line_of,
match_braces, parse_functions, parse_exemptions, exemption_for_index,
split_loops, float_names_in, looks_float_expr, looks_int_expr,
condition_is_float, standing_covers, cold_file, is_init_symbol, classify_hot,
seed_map, the Finding/AuditResult/Function/Exemption/HotEntry dataclasses, the
exemption grammar regex, and scan_function's R2-R8 rule logic).

Forsaken adaptations (see the task brief / contract):
  * Scope is the 3DS build's source trees (forsaken_core/src + forsaken_3ds/src),
    minus EXCLUDE_SRC from Makefile.3ds and the two GL sources the default
    citro3d renderer excludes. No ``.inc`` fragments, no PC tree. Keys are
    basenames (see CXX_EXTS).
  * R1 covers main.c + music_3ds.c only (no sys::Thread wrapper exists here).
  * R6 LIBM_RE extended with floor/ceil/fmod (this tree's floorf debt).
  * R10 requires the devkitPro ARCH baseline + -fno-math-errno and bans
    -ffast-math/-Ofast/-fsingle-precision-constant; -mfpu is NOT required.
  * Adoption-debt BASELINE RATCHET replaces T2K's inc_hot baseline: findings in
    contracts/hot_findings_baseline.txt are suppressed-but-counted; anything new
    fails. audit_arm11_math.py shares this ratchet (rules TRIG / R4-D).
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
from collections import Counter
from dataclasses import asdict, dataclass, field
from datetime import date
from pathlib import Path
from typing import Iterator, Literal, Mapping, Sequence

Severity = Literal["MUST", "SHOULD"]
Status = Literal["PASS", "FAIL"]

# An @vfp-exempt pin is a CLAIM that integerizing the site changes pixels or hit
# tests, backed by a `Verified <date>`. A claim with no expiry is a claim nobody
# rechecks, so past PIN_MAX_AGE_DAYS the pin is a MUST finding and the remedy is
# a fresh measured capture then a date bump.
PIN_MAX_AGE_DAYS = int(os.environ.get("VFP_PIN_MAX_AGE_DAYS", "180"))
# The hot-path corpus is "top 15 of a CAPTURED profile". Against a stale capture
# that phrase means nothing, so the corpus must declare when it was captured and
# go stale on the same clock -- but ONLY once provisional is false (see below).
CORPUS_MAX_AGE_DAYS = int(os.environ.get("VFP_CORPUS_MAX_AGE_DAYS", "45"))

# T2K froze its project and waived the staleness clocks from a date. Forsaken is
# NOT frozen -- perf work is just starting -- so the default is empty and the
# clocks stay live (gated additionally on provisional). VFP_FROZEN_AT can still
# set a freeze date if this ever changes.
PROJECT_FROZEN_AT = os.environ.get("VFP_FROZEN_AT", "").strip()


def _frozen() -> str | None:
    """Freeze date, or None if the staleness clocks are live."""
    return PROJECT_FROZEN_AT or None


def _age_days(iso: str, today: date | None = None) -> int | None:
    try:
        y, m, d = (int(x) for x in iso.strip().split("-"))
        return (today or date.today()).toordinal() - date(y, m, d).toordinal()
    except (ValueError, AttributeError):
        return None


# tools/perf/gates_vfp.py -> parents[0]=perf, [1]=tools, [2]=repo root
REPO_ROOT = Path(__file__).resolve().parents[2]
RUNNER_DIR = Path(__file__).resolve().parent
CONTRACTS_DIR = RUNNER_DIR / "contracts"
DEFAULT_CORPUS = CONTRACTS_DIR / "hotpath_corpus.json"
DEFAULT_LOG = RUNNER_DIR / "logs" / "vfp_audit.json"
MAKEFILE = REPO_ROOT / "Makefile.3ds"

# Source trees (same shape as t2k / mdk-sdl). The 3DS build compiles CORE +
# 3DS, so that is gate scope; the PC tree is outside it, as the PC-only files
# always were (they used to be filtered out via EXCLUDE_SRC).
CORE_SRC_REL = Path("forsaken_core/src")
PLAT_3DS_SRC_REL = Path("forsaken_3ds/src")
PLAT_PC_SRC_REL = Path("forsaken_pc/src")
BUILD_TREES_REL = (CORE_SRC_REL, PLAT_3DS_SRC_REL)
ALL_TREES_REL = (CORE_SRC_REL, PLAT_3DS_SRC_REL, PLAT_PC_SRC_REL)

# The numeric contract (R1-R11) covers C and C++ alike: a converted file cannot
# slip past R2/R3/R4/R6/R11 by being a .cpp. No .inc fragments (T2K's renderer
# is split across pasted .inc files; here the renderer is a single render_c3d.c).
#
# Findings, baselines, the hot corpus and exemptions are keyed by source
# BASENAME ("ships.cpp"), not by path. Both Makefiles already key objects by
# basename, so basenames are unique across the trees by construction;
# in_scope_files() fails loudly if that ever stops being true. Basename keys
# keep every baseline entry valid when a file moves between subdirectories.
CXX_EXTS = {".c", ".h", ".cpp", ".hpp", ".cc", ".cxx", ".hh"}

INIT_NAME_RE = re.compile(
    r"(init|load|create|destroy|reset|parse|shutdown|seed)", re.IGNORECASE
)
THREAD_CREATE_RE = re.compile(r"\bthreadCreate\s*\(")
# The FIRST argument of a creation call is the worker function itself, so the
# gate can look that routine up and read its body instead of crediting whatever
# else happens to be in the file. Forsaken has no thread wrapper, so this is the
# only worker-detection pattern (T2K's sys::Thread::start matching is dropped).
THREAD_CREATE_ARG_RE = re.compile(r"\bthreadCreate\s*\(\s*([A-Za-z_]\w*)")
LOCAL_INCLUDE_RE = re.compile(r'^[ \t]*#[ \t]*include[ \t]+"([^"]+)"', re.M)
# The two helpers the contract names: vfp_set_fz_dn() on main, vfp_thread_fz_dn()
# on the music worker.
FPSCR_HELPER_RE = re.compile(r"\b(vfp_set_fz_dn|vfp_thread_fz_dn)\b")
FPSCR_ASM_RE = re.compile(
    r"\b(vmsr|vmrs)\b.*\bFPSCR\b"
    r"|mcr\s+p10\s*,\s*7"
    r"|FPSCR",
    re.IGNORECASE,
)
EXEMPT_RE = re.compile(
    r"/\*\s*@vfp-exempt\s+(?P<rules>R\d+(?:\s*,\s*R\d+)*)\s+—\s+"
    r"(?P<reason>.+?)\.\s*"
    r"(?:measured\s+(?P<measured>\S+)\s*\.\s*)?"
    r"Verified\s+(?P<verified>\d{4}-\d{2}-\d{2})\s*\.\s*\*/",
    re.DOTALL,
)
HOTPATH_ANN_RE = re.compile(r"/\*\s*@hotpath\s*\*/")
FUNC_RE = re.compile(
    r"(?P<head>^[\w:<>,\s\*&]+?\b(?P<name>[A-Za-z_]\w*)\s*\([^;{]*\))"
    r"\s*(?:const\s+|volatile\s+|&{1,2}\s*)*\{",
    re.MULTILINE,
)
FOR_FLOAT_RE = re.compile(r"\bfor\s*\(\s*(?:float|double)\b")
CAST_INT_RE = re.compile(
    r"(?:static_cast|reinterpret_cast|const_cast)\s*<\s*"
    r"(?:int|unsigned|uint32_t|int32_t|int16_t|uint16_t|long|size_t|u32|s32|u16|s16)\s*>"
    r"\s*\("
    r"|\(\s*(?:int|unsigned|uint32_t|int32_t|int16_t|uint16_t|long|size_t|u32|s32|u16|s16)\s*\)"
)
DOUBLE_TYPE_RE = re.compile(r"\b(long\s+double|double)\b")
UNSUFFIXED_FLOAT_RE = re.compile(r"(?<![\w.])(?:\d+\.\d+|\d+\.|\.\d+)(?![eE][+-]?\d*)(?![fF\w.])")
# Forsaken extension of T2K's LIBM_RE: floor/ceil/fmod count as libm calls here
# because they are this tree's actual hot-path libm debt (contract §2 R6). VFPv2
# has no float-result floor, so floorf() is a real `bl` into newlib.
LIBM_RE = re.compile(
    r"\b(?:sinf?|cosf?|tanf?|atan2f?|expf?|logf?|powf?|floorf?|ceilf?|fmodf?)\s*\("
)
SQRT_RE = re.compile(r"\bsqrtf?\s*\(")
# fast_sinf/fast_cosf and the SINR/COSR/SIND/COSD macros are float-producing
# expressions (used by looks_float_expr to prove floatness).
FAST_TRIG_RE = re.compile(r"\b(?:fast_sinf|fast_cosf|SINR|COSR|SIND|COSD)\b")
FX_IDENT_RE = re.compile(r"\b([A-Za-z_]\w*(?:_fx|_q16|_q8)|fx_[A-Za-z_]\w*)\b")
FLOAT_DECL_RE = re.compile(
    r"\b(?:float|double)\s+(?:const\s+)?(?P<names>[A-Za-z_]\w*(?:\s*,\s*[A-Za-z_]\w*)*)"
)
KEYWORD_STOP = {
    "if", "for", "while", "switch", "return", "sizeof", "static", "const",
    "void", "int", "float", "double", "bool", "char", "namespace", "class",
    "struct", "enum", "typedef", "template", "using", "inline", "extern",
    "public", "private", "protected", "virtual", "override", "operator", "new",
    "delete", "this", "true", "false", "nullptr", "NULL", "auto", "case",
    "default", "else", "do", "break", "continue", "goto",
}
# R10 required flags (contract §3 R10). -mfpu is deliberately NOT here: devkitARM's
# armv6k default is VFPv2, and the gate notes that rather than demanding a flag the
# Makefile does not (and should not) carry.
ARCH_REQUIRED = ("-march=armv6k", "-mtune=mpcore", "-mfloat-abi=hard", "-mtp=soft")
BANNED_FLAGS = ("-ffast-math", "-Ofast", "-fsingle-precision-constant")
# The two float threads in this tree (contract §2 R1 table). main runs on the
# main thread; music_thread_fn is the only created worker.
R1_THREAD_SITES = ("main.cpp", "music_3ds.cpp")
STANDING_EXEMPTIONS: tuple[dict[str, object], ...] = (
    {"id": "E-LUT-1", "file": "new3d.h", "symbols": ("fast_sinf",), "rules": ("R2", "R3", "R5")},
    {"id": "E-LUT-2", "file": "new3d.h", "symbols": ("fast_cosf",), "rules": ("R2", "R3")},
    {"id": "E-LUT-3", "file": "new3d.cpp", "symbols": ("trig_table_init",), "rules": ("R4", "R6")},
    {"id": "E-LUT-4", "file": "new3d.h", "symbols": ("fast_invsqrtf",), "rules": ("R7",)},
)


@dataclass
class Finding:
    rule: str
    severity: Severity
    file: str
    line: int
    symbol: str
    hotpath: bool
    hotpath_reason: str
    exemption: str | None
    detail: str


@dataclass
class AuditResult:
    status: Status
    blockers: int
    reviews: int
    findings: list[Finding] = field(default_factory=list)
    hotpath_source: str = "seed"
    notes: list[str] = field(default_factory=list)
    baselined: int = 0
    baselined_by_rule: dict[str, int] = field(default_factory=dict)
    new_by_rule: dict[str, int] = field(default_factory=dict)

    def to_json(self) -> dict[str, object]:
        return {
            "status": self.status,
            "blockers": self.blockers,
            "reviews": self.reviews,
            "baselined": self.baselined,
            "baselined_by_rule": self.baselined_by_rule,
            "new_by_rule": self.new_by_rule,
            "hotpath_source": self.hotpath_source,
            "notes": self.notes,
            "findings": [asdict(f) for f in self.findings],
        }


@dataclass
class Function:
    name: str
    start: int
    end: int
    body: str
    line: int
    header: str


@dataclass
class Exemption:
    rules: frozenset[str]
    reason: str
    measured: str | None
    verified: str
    standing: bool
    line: int
    applies_to: str


@dataclass
class HotEntry:
    rel: str
    symbols: tuple[str, ...]
    why: str
    source: str
    cold_symbols: tuple[str, ...] = ()


def repo_rel(path: Path) -> str:
    """Report/key name for a path: the BASENAME for a file inside a source tree
    (the gate key, see CXX_EXTS above), else the repo-relative path."""
    try:
        rel = path.resolve().relative_to(REPO_ROOT)
    except ValueError:
        return path.as_posix()
    if any(rel.parts[:len(t.parts)] == t.parts for t in ALL_TREES_REL):
        return path.name
    return rel.as_posix()


def source_trees(root: Path | None = None, *, include_pc: bool = False) -> list[Path]:
    """Absolute source-tree roots under `root` (default: the repo)."""
    root = root or REPO_ROOT
    rels = ALL_TREES_REL if include_pc else BUILD_TREES_REL
    return [root / t for t in rels]


def find_source(name: str, root: Path | None = None) -> Path | None:
    """The in-scope file whose basename is `name`, or None."""
    for path in in_scope_files(root):
        if path.name == name:
            return path
    return None


def _is_under(path: Path, prefix: Path) -> bool:
    try:
        path.relative_to(prefix.resolve())
        return True
    except ValueError:
        return False


def _join_makefile_continuations(text: str) -> list[str]:
    """Join backslash-continued Makefile lines into logical lines."""
    out: list[str] = []
    buf = ""
    for line in text.splitlines():
        if line.rstrip().endswith("\\"):
            buf += line.rstrip()[:-1] + " "
        else:
            buf += line
            out.append(buf)
            buf = ""
    if buf:
        out.append(buf)
    return out


def parse_exclude_src(makefile: Path = MAKEFILE) -> set[str]:
    """Source file names excluded from the 3DS build.

    Parses the ``EXCLUDE_SRC := \\`` continuation block (the unconditional
    desktop/networking/botai exclusions). The two GL sources the DEFAULT citro3d
    renderer excludes (``EXCLUDE_SRC += render_gl1.c render_gl_shared.c``,
    Makefile.3ds ~line 112) are added explicitly: the sibling ``+= render_c3d.c``
    line lives in the picagl else-branch and must NOT be dropped (render_c3d.c is
    the shipping renderer), so the ``+=`` lines are not parsed blindly.
    """
    excl: set[str] = set()
    try:
        text = makefile.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return excl
    for logical in _join_makefile_continuations(text):
        s = logical.strip()
        m = re.match(r"EXCLUDE_SRC\s*:=\s*(.*)$", s)
        if m:
            for tok in m.group(1).split():
                tok = tok.strip()
                if tok:
                    excl.add(tok)
    # citro3d default-renderer exclusions (see docstring).
    excl.update({"render_gl1.c", "render_gl_shared.c"})
    return excl


def in_scope_files(root: Path | None = None) -> list[Path]:
    """Sources compiled by Makefile.3ds (core + 3DS trees), ordered by basename.

    Basename order is the order the flat root used to give, so reports and
    first-hit behaviour are unchanged by the tree layout.
    """
    excl = parse_exclude_src()
    by_name: dict[str, Path] = {}
    for tree in source_trees(root):
        for path in tree.rglob("*"):
            if path.is_file() and path.suffix in CXX_EXTS and path.name not in excl:
                if path.name in by_name:
                    raise SystemExit(
                        f"duplicate source basename {path.name}: {by_name[path.name]} and "
                        f"{path} -- basenames are the gate keys and the object names; "
                        "they must be unique across the source trees"
                    )
                by_name[path.name] = path
    return [by_name[name] for name in sorted(by_name)]


def iter_cxx_files(root: Path | None = None) -> Iterator[Path]:
    yield from in_scope_files(root)


def load_json(path: Path) -> dict[str, object]:
    return json.loads(path.read_text(encoding="utf-8"))


def load_corpus(path: Path) -> dict[str, object]:
    if not path.exists():
        return {"version": 1, "provisional": True, "captured": None, "seed": [], "cold_always": []}
    data = load_json(path)
    if not isinstance(data, dict):
        raise ValueError(f"corpus is not an object: {path}")
    return data


def strip_comments_keep_lines(text: str) -> str:
    out: list[str] = []
    i = 0
    n = len(text)
    in_str = False
    in_char = False
    in_line = False
    in_block = False
    while i < n:
        ch = text[i]
        nxt = text[i + 1] if i + 1 < n else ""
        if in_line:
            if ch == "\n":
                in_line = False
                out.append("\n")
            else:
                out.append(" ")
            i += 1
            continue
        if in_block:
            if ch == "*" and nxt == "/":
                in_block = False
                out.append("  ")
                i += 2
            else:
                out.append("\n" if ch == "\n" else " ")
                i += 1
            continue
        if in_str:
            out.append(ch)
            if ch == "\\" and nxt:
                out.append(nxt)
                i += 2
                continue
            if ch == '"':
                in_str = False
            i += 1
            continue
        if in_char:
            out.append(ch)
            if ch == "\\" and nxt:
                out.append(nxt)
                i += 2
                continue
            if ch == "'":
                in_char = False
            i += 1
            continue
        if ch == "/" and nxt == "/":
            in_line = True
            out.append("  ")
            i += 2
            continue
        if ch == "/" and nxt == "*":
            in_block = True
            out.append("  ")
            i += 2
            continue
        if ch == '"':
            in_str = True
            out.append(ch)
            i += 1
            continue
        if ch == "'":
            in_char = True
            out.append(ch)
            i += 1
            continue
        out.append(ch)
        i += 1
    return "".join(out)


def line_of(text: str, index: int) -> int:
    return text.count("\n", 0, index) + 1


def match_braces(text: str, open_idx: int) -> int:
    depth = 0
    i = open_idx
    n = len(text)
    in_str = False
    in_char = False
    while i < n:
        ch = text[i]
        nxt = text[i + 1] if i + 1 < n else ""
        if in_str:
            if ch == "\\" and nxt:
                i += 2
                continue
            if ch == '"':
                in_str = False
            i += 1
            continue
        if in_char:
            if ch == "\\" and nxt:
                i += 2
                continue
            if ch == "'":
                in_char = False
            i += 1
            continue
        if ch == '"':
            in_str = True
        elif ch == "'":
            in_char = True
        elif ch == "/" and nxt == "/":
            nl = text.find("\n", i)
            i = n if nl < 0 else nl
            continue
        elif ch == "/" and nxt == "*":
            end = text.find("*/", i + 2)
            i = n if end < 0 else end + 2
            continue
        elif ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return n - 1


IF0_RE = re.compile(r"^\s*#\s*if\s+0\b")
IF_OPEN_RE = re.compile(r"^\s*#\s*(if|ifdef|ifndef)\b")
IF_END_RE = re.compile(r"^\s*#\s*endif\b")
IF_BRANCH_RE = re.compile(r"^\s*#\s*(else|elif)\b")


def strip_if0_keep_lines(text: str) -> str:
    """Blank `#if 0` dead branches (line count preserved) so dead code is not
    scanned. The `#else`/`#elif` arm of an `#if 0` chain stays live; nested
    conditionals inside a dead branch stay dead."""
    lines = text.split("\n")
    out = list(lines)
    stack: list[str] = []
    for i, line in enumerate(lines):
        top = stack[-1] if stack else None
        if top == "dead":
            out[i] = ""
        if IF_OPEN_RE.match(line):
            if top == "dead":
                stack.append("dead")
            elif IF0_RE.match(line):
                stack.append("dead")
                out[i] = ""
            else:
                stack.append("live")
        elif IF_BRANCH_RE.match(line):
            if stack and stack[-1] == "dead":
                stack[-1] = "live"
        elif IF_END_RE.match(line):
            if stack:
                stack.pop()
    return "\n".join(out)


def parse_functions(text: str) -> list[Function]:
    cleaned = strip_if0_keep_lines(strip_comments_keep_lines(text))
    functions: list[Function] = []
    for match in FUNC_RE.finditer(cleaned):
        name = match.group("name")
        if name in KEYWORD_STOP:
            continue
        brace = match.end() - 1
        if cleaned[brace] != "{":
            continue
        end = match_braces(cleaned, brace)
        start = match.start()
        functions.append(
            Function(
                name=name,
                start=start,
                end=end + 1,
                body=cleaned[brace + 1 : end],
                line=line_of(text, start),
                header=cleaned[start:brace],
            )
        )
    return functions


def parse_exemptions(text: str) -> list[Exemption]:
    found: list[Exemption] = []
    for match in EXEMPT_RE.finditer(text):
        rules = frozenset(part.strip() for part in match.group("rules").split(","))
        reason = re.sub(r"\s+", " ", match.group("reason")).strip()
        measured = match.group("measured")
        standing = "standing" in reason.lower()
        found.append(
            Exemption(
                rules=rules,
                reason=reason,
                measured=measured,
                verified=match.group("verified"),
                standing=standing,
                line=line_of(text, match.start()),
                applies_to="",
            )
        )
    return found


def exemption_for_index(text: str, exemptions: Sequence[Exemption], index: int) -> Exemption | None:
    best: Exemption | None = None
    best_dist = 10**9
    for exemption in exemptions:
        pos = 0
        for _ in range(exemption.line - 1):
            nxt = text.find("\n", pos)
            if nxt < 0:
                pos = len(text)
                break
            pos = nxt + 1
        if pos <= index and (index - pos) < best_dist:
            best = exemption
            best_dist = index - pos
    if best is None:
        return None
    if best_dist > 800:
        return None
    return best


def split_loops(body: str) -> list[tuple[int, str]]:
    loops: list[tuple[int, str]] = []
    for match in re.finditer(r"\b(for|while|do)\b", body):
        i = match.end()
        while i < len(body) and body[i] in " \t\n":
            i += 1
        if i < len(body) and body[i] == "(":
            depth = 0
            j = i
            while j < len(body):
                if body[j] == "(":
                    depth += 1
                elif body[j] == ")":
                    depth -= 1
                    if depth == 0:
                        j += 1
                        break
                j += 1
            i = j
            while i < len(body) and body[i] in " \t\n":
                i += 1
        if i < len(body) and body[i] == "{":
            end = match_braces(body, i)
            loops.append((match.start(), body[i + 1 : end]))
        else:
            semi = body.find(";", i)
            if semi >= 0:
                loops.append((match.start(), body[i : semi + 1]))
    return loops


def float_names_in(text: str) -> set[str]:
    names: set[str] = set()
    for match in FLOAT_DECL_RE.finditer(text):
        for raw in match.group("names").split(","):
            ident = raw.strip()
            if ident:
                names.add(ident)
    return names


def looks_float_expr(expr: str, float_ids: set[str]) -> bool:
    if UNSUFFIXED_FLOAT_RE.search(expr) or re.search(r"\d+(?:\.\d*)?[fF]\b", expr):
        return True
    if re.search(r"\b(?:float|double)\b", expr):
        return True
    tokens = set(re.findall(r"[A-Za-z_]\w*", expr))
    if tokens & float_ids:
        return True
    if FAST_TRIG_RE.search(expr):
        return True
    return False


def looks_int_expr(expr: str) -> bool:
    stripped = expr.strip()
    if re.fullmatch(r"\d+", stripped):
        return True
    if re.search(r"\b(?:int|unsigned|size_t|uint32_t|int32_t|u32|s32)\b", expr):
        return True
    return False


def condition_is_float(cond: str, float_ids: set[str]) -> bool:
    parts = re.split(r"(&&|\|\|)", cond)
    for part in parts:
        if re.search(r"(==|!=|<=|>=|<|>)", part) and looks_float_expr(part, float_ids):
            if looks_int_expr(part) and not looks_float_expr(part, float_ids):
                continue
            return True
        if part.strip().startswith("!") and looks_float_expr(part, float_ids):
            return True
    return looks_float_expr(cond, float_ids) and bool(
        re.search(r"(==|!=|<=|>=|<|>)", cond)
    )


def standing_covers(rel: str, symbol: str, rule: str) -> str | None:
    rel_n = rel.replace("\\", "/")
    for item in STANDING_EXEMPTIONS:
        file_n = str(item["file"]).replace("\\", "/")
        if rel_n != file_n and not rel_n.endswith("/" + file_n):
            continue
        symbols = item["symbols"]
        assert isinstance(symbols, tuple)
        if "*" not in symbols and symbol not in symbols:
            continue
        rules = item["rules"]
        assert isinstance(rules, tuple)
        if rule in rules:
            return str(item["id"])
    return None


def cold_file(rel: str, cold: Sequence[str]) -> bool:
    rel_n = rel.replace("\\", "/")
    for prefix in cold:
        p = prefix.replace("\\", "/")
        if rel_n == p or rel_n.startswith(p.rstrip("/") + "/"):
            return True
    return False


def is_init_symbol(name: str) -> bool:
    return bool(INIT_NAME_RE.search(name))


def classify_hot(
    rel: str,
    func: Function,
    seed: Mapping[str, HotEntry],
    annotated: bool,
    cold: Sequence[str],
) -> tuple[bool, str]:
    if cold_file(rel, cold):
        return False, ""
    if annotated:
        return True, "@hotpath"
    entry = seed.get(rel)
    if entry is None:
        return False, ""
    if func.name in entry.cold_symbols:
        return False, ""
    if "*" in entry.symbols:
        if is_init_symbol(func.name):
            return False, ""
        return True, entry.source
    if func.name in entry.symbols:
        return True, entry.source
    return False, ""


def seed_map(corpus: Mapping[str, object], extra: Sequence[HotEntry] | None = None) -> dict[str, HotEntry]:
    out: dict[str, HotEntry] = {}
    raw_seed = corpus.get("seed", [])
    if isinstance(raw_seed, list):
        for item in raw_seed:
            if not isinstance(item, dict):
                continue
            file_name = str(item.get("file", ""))
            symbols_raw = item.get("symbols", [])
            symbols = tuple(str(s) for s in symbols_raw) if isinstance(symbols_raw, list) else ("*",)
            why = str(item.get("why", "seed"))
            cold_raw = item.get("cold_symbols", [])
            cold_syms = tuple(str(s) for s in cold_raw) if isinstance(cold_raw, list) else ()
            out[file_name] = HotEntry(file_name, symbols, why, "seed", cold_syms)
    if extra:
        for item in extra:
            out[item.rel] = item
    return out


def ingest_perf_logs(docs_dir: Path) -> tuple[list[HotEntry], str]:
    """Parse docs/validation/perf-*.log top-15 stage names into HotEntry seeds.

    The stage-name -> file map is EMPTY until a real hardware capture defines it
    (the corpus capture_note names the log format). Kept generic so the first
    capture only has to fill the map, not rewrite the ingest.
    """
    if not docs_dir.exists():
        return [], "seed"
    logs = sorted(docs_dir.glob("perf-*.log"))
    if not logs:
        return [], "seed"
    stage_map: dict[str, list[str]] = {}  # empty until a capture defines stage->file
    if not stage_map:
        return [], "seed"
    averages: dict[str, list[float]] = {}
    stage_re = re.compile(r"\b(" + "|".join(map(re.escape, stage_map)) + r")\b[^\d]{0,12}(\d+(?:\.\d+)?)")
    for log in logs[-8:]:
        try:
            text = log.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        for match in stage_re.finditer(text):
            averages.setdefault(match.group(1), []).append(float(match.group(2)))
    if not averages:
        return [], "seed"
    ranked = sorted(
        ((name, sum(vals) / len(vals)) for name, vals in averages.items()),
        key=lambda kv: kv[1],
        reverse=True,
    )[:15]
    extra: list[HotEntry] = []
    for name, _avg in ranked:
        for rel in stage_map.get(name, []):
            extra.append(HotEntry(rel, ("*",), f"profile:{name}", f"profile:{name}"))
    return extra, ("profile" if extra else "seed")


def gate_r10(makefile: Path) -> list[Finding]:
    """R10 -- build flags are present and none of the banned ones are.

    Requires the devkitPro ARCH baseline + -fno-math-errno in the Makefile text
    and bans -ffast-math / -Ofast / -fsingle-precision-constant. -mfpu is NOT
    required (devkitARM armv6k defaults to VFPv2; run_audit notes this).
    """
    findings: list[Finding] = []
    if not makefile.exists():
        findings.append(
            Finding("R10", "MUST", repo_rel(makefile), 0, "Makefile", True, "seed", None,
                    "Makefile.3ds missing")
        )
        return findings
    text = makefile.read_text(encoding="utf-8", errors="replace")
    missing = [flag for flag in ARCH_REQUIRED if flag not in text]
    if missing:
        findings.append(
            Finding("R10", "MUST", repo_rel(makefile), 0, "ARCH", True, "seed", None,
                    "Makefile.3ds missing required flag(s): " + ", ".join(missing))
        )
    if "-fno-math-errno" not in text:
        findings.append(
            Finding("R10", "MUST", repo_rel(makefile), 0, "CFLAGS", True, "seed", None,
                    "Makefile.3ds missing -fno-math-errno")
        )
    for banned in BANNED_FLAGS:
        if banned in text:
            findings.append(
                Finding("R10", "MUST", repo_rel(makefile), 0, "CFLAGS", True, "seed", None,
                        f"banned flag present in Makefile.3ds: {banned}")
            )
    if re.search(r"-mfloat-abi=(softfp|soft)\b", text):
        findings.append(
            Finding("R10", "MUST", repo_rel(makefile), 0, "ARCH", True, "seed", None,
                    "soft/softfp float ABI present")
        )
    return findings


def fpscr_asm_write(text: str) -> bool:
    """Does this text write the FPSCR inline, rather than through a helper?"""
    if FPSCR_ASM_RE.search(text) and re.search(r"\b(FZ|DN|0x0[23]000000|1u?\s*<<\s*24|1u?\s*<<\s*25)\b", text):
        return True
    if re.search(r"vmsr\s+FPSCR", text, re.IGNORECASE) and re.search(
        r"(1u?\s*<<\s*24).*(1u?\s*<<\s*25)|(1u?\s*<<\s*25).*(1u?\s*<<\s*24)|0x03000000",
        text,
    ):
        return True
    return False


def fpscr_ok(text: str) -> bool:
    """Does this text MENTION FPSCR setup, as a helper name or as inline asm?"""
    if FPSCR_HELPER_RE.search(text):
        return True
    return fpscr_asm_write(text)


HELPER_CALL_RE = re.compile(r"\b(vfp_set_fz_dn|vfp_thread_fz_dn)\s*\(\s*\)")
HELPER_DEF_TAIL_RE = re.compile(r"\b(?:void|int|static|inline|constexpr|auto)\s*$")


def fpscr_called(text: str) -> bool:
    """Is a FZ|DN helper CALLED here, rather than merely defined here?"""
    for match in HELPER_CALL_RE.finditer(text):
        if HELPER_DEF_TAIL_RE.search(text[max(0, match.start() - 40) : match.start()]):
            continue
        return True
    return False


HELPER_DEF_RE = re.compile(
    r"\b(?:void|int|static|inline|constexpr|auto)[^\n;{}]*\b"
    r"(?:vfp_set_fz_dn|vfp_thread_fz_dn)\s*\("
)


def fpscr_established(text: str) -> bool:
    """Is FZ|DN actually ESTABLISHED here (called or written inline), not just offered?"""
    if fpscr_called(text):
        return True
    if HELPER_DEF_RE.search(text):
        return False
    return fpscr_asm_write(text)


def worker_name_at(code: str, index: int, kind: str) -> str | None:
    """The worker routine named by the creation call at byte `index`."""
    match = THREAD_CREATE_ARG_RE.search(code, index, index + 160)
    return match.group(1) if match else None


def worker_helper_verdict(name: str, scope: Sequence[Path]) -> bool | None:
    """Does the worker routine `name` establish FZ|DN at its own entry?

    True  -- definition found, body calls the helper or writes the FPSCR.
    False -- definition found, body does neither.
    None  -- definition not in scope, so undecidable here.
    """
    for path in scope:
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        for fn in parse_functions(strip_comments_keep_lines(text)):
            if fn.name == name:
                return bool(fpscr_called(fn.body) or fpscr_asm_write(fn.body))
    return None


def r1_site_ok(
    path: Path, code: str, index: int, kind: str,
    closures: Mapping[str, frozenset[Path]],
) -> bool:
    """Is FZ|DN established for the worker created at byte `index` of `code`?"""
    scope = sorted(closures.get(str(path.resolve()), frozenset({path})))
    name = worker_name_at(code, index, kind)
    verdict = worker_helper_verdict(name, scope) if name else None
    if verdict is not None:
        return verdict
    if fpscr_established(code):
        return True
    return fpscr_established(strip_comments_keep_lines(tu_text(path, closures)))


def thread_creation_sites(
    path: Path, clean: str, closures: Mapping[str, frozenset[Path]]
) -> list[tuple[int, str, str]]:
    """Every thread-creation site in comment-stripped `clean`: (byte, kind, severity).

    Forsaken has no thread wrapper, so only libctru `threadCreate` is matched --
    unambiguously a 3DS worker, and every libctru worker here does float work, so
    it is a MUST.
    """
    return [(m.start(), "threadCreate", "MUST") for m in THREAD_CREATE_RE.finditer(clean)]


def r1_site_detail(kind: str) -> str:
    return (
        "thread-creation site whose worker never establishes FPSCR FZ|DN "
        "(worker body checked, then the include closure)"
    )


def _fpscr_comment_ok(text: str) -> bool:
    lowered = text.lower()
    has_denormal = "denormal" in lowered
    has_fz_dn = ("flush-to-zero" in lowered or "flush to zero" in lowered) or (
        "fz" in lowered and "dn" in lowered
    )
    has_per_thread = (
        "per-thread" in lowered
        or "per thread" in lowered
        or "on this thread" in lowered
        or "each worker" in lowered
        or "every thread" in lowered
    )
    return (has_denormal or has_fz_dn) and has_per_thread


def include_graph(
    files: Sequence[Path],
) -> tuple[dict[Path, set[Path]], dict[Path, set[Path]]]:
    """(includes, includers) over the quoted local #include edges of `files`."""
    present = {p.resolve(): p for p in files}
    includes: dict[Path, set[Path]] = {p: set() for p in files}
    includers: dict[Path, set[Path]] = {p: set() for p in files}
    # Quoted includes resolve the way the compiler does: the including file's
    # directory first, then each source-tree root (-I), in order.
    roots = source_trees()
    for path in files:
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        for match in LOCAL_INCLUDE_RE.finditer(text):
            src = None
            for base in (path.parent, *roots):
                src = present.get((base / match.group(1)).resolve())
                if src is not None:
                    break
            if src is not None and src != path:
                includes[path].add(src)
                includers[src].add(path)
    return includes, includers


def include_closures(files: Sequence[Path]) -> dict[str, frozenset[Path]]:
    """resolved path -> the files the compiler pastes together with it, one hop out.

    Deliberately NOT transitive (T2K's lesson): a transitive closure over a shared
    header graph makes "the unit" mean "the repository" and vacuates every presence
    test. One hop up (includers) and one hop down (their includes) reproduces the
    unit R1 needs.
    """
    includes, includers = include_graph(files)
    closures: dict[str, frozenset[Path]] = {}
    for path in files:
        seen = {path} | includes[path]
        for up in includers[path]:
            seen.add(up)
            seen |= includes[up]
        closures[str(path.resolve())] = frozenset(seen)
    return closures


def tu_text(path: Path, closures: Mapping[str, frozenset[Path]]) -> str:
    """The file plus everything its include closure reaches ("" if unknown)."""
    members = closures.get(str(path.resolve()))
    if not members:
        return ""
    parts: list[str] = []
    for member in sorted(members):
        try:
            parts.append(member.read_text(encoding="utf-8", errors="replace"))
        except OSError:
            continue
    return "\n".join(parts)


def gate_r1(root: Path | None = None) -> list[Finding]:
    """R1 -- every float thread establishes FPSCR FZ|DN at its entry.

    Two listed sites (contract §2 R1):
      * main.c -- the helper present in the file or its include closure AND called.
      * music_3ds.c -- the threadCreate site names music_thread_fn, and that
        routine's body must establish FZ|DN.
    Plus a tree-wide sweep of threadCreate sites outside the listed files, so a new
    float worker cannot hide by not being on the table. Forsaken currently has no
    FPSCR code at all, so both listed sites are findings -- that is expected and
    gets baselined as adoption debt.
    """
    findings: list[Finding] = []
    base = root or REPO_ROOT
    swept = in_scope_files(base)
    closures = include_closures(swept)
    any_fpscr = False
    listed = set(R1_THREAD_SITES)
    for rel in R1_THREAD_SITES:
        path = find_source(rel, base)
        if path is None:
            findings.append(
                Finding("R1", "MUST", rel, 0, "missing", True, "seed", None,
                        "required thread site missing")
            )
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        code = strip_comments_keep_lines(text)
        tu = tu_text(path, closures)
        tu_code = strip_comments_keep_lines(tu)
        if fpscr_ok(text):
            any_fpscr = True
            if not _fpscr_comment_ok(text):
                findings.append(
                    Finding("R1", "MUST", rel, 0, "fpscr-comment", True, "seed", None,
                            "FPSCR code without rationale comment (denormal/FZ|DN, per-thread)")
                )
        if rel == "main.cpp" and not (fpscr_established(code) or fpscr_established(tu_code)):
            findings.append(
                Finding("R1", "MUST", rel, 0, "main", True, "seed", None,
                        "no FPSCR FZ|DN helper on main (file or include closure)")
            )
        for index, kind, severity in thread_creation_sites(path, code, closures):
            if not r1_site_ok(path, code, index, kind, closures):
                findings.append(
                    Finding("R1", severity, rel, line_of(text, index), kind, True, "seed", None,
                            r1_site_detail(kind))
                )
    for path in swept:
        rel = repo_rel(path)
        if rel in listed:
            continue
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        clean = strip_comments_keep_lines(text)
        for index, kind, severity in thread_creation_sites(path, clean, closures):
            if not r1_site_ok(path, clean, index, kind, closures):
                findings.append(
                    Finding("R1", severity, rel, line_of(text, index), kind, True, "seed", None,
                            r1_site_detail(kind))
                )
    if not any_fpscr and not any(f.file == "main.cpp" and f.symbol == "main" for f in findings):
        findings.append(
            Finding("R1", "MUST", "main.cpp", 0, "main", True, "seed", None,
                    "no FPSCR FZ|DN write on main or workers")
        )
    return _dedupe(findings)


def _dedupe(findings: Sequence[Finding]) -> list[Finding]:
    seen: set[tuple[object, ...]] = set()
    out: list[Finding] = []
    for item in findings:
        key = (item.rule, item.file, item.line, item.symbol, item.detail)
        if key in seen:
            continue
        seen.add(key)
        out.append(item)
    return out


def gate_exemptions(text: str, rel: str) -> list[Finding]:
    findings: list[Finding] = []
    for match in re.finditer(r"/\*\s*@vfp-exempt\b", text):
        chunk = text[match.start() : match.start() + 400]
        parsed = EXEMPT_RE.match(chunk)
        if parsed is None:
            findings.append(
                Finding("R-EXEMPT", "MUST", rel, line_of(text, match.start()), "@vfp-exempt",
                        True, "@hotpath", None, "invalid @vfp-exempt grammar")
            )
            continue
        rules = [part.strip() for part in parsed.group("rules").split(",")]
        reason = parsed.group("reason")
        measured = parsed.group("measured")
        verified = parsed.group("verified")
        standing = "standing" in reason.lower()
        must_rules = [r for r in rules if r not in {"R7", "R9"}]
        if must_rules and not measured and not standing:
            findings.append(
                Finding("R-EXEMPT", "MUST", rel, line_of(text, match.start()), "@vfp-exempt",
                        True, "@hotpath", None, "MUST exemption missing measured cost")
            )
        age = None if _frozen() else _age_days(verified)
        if age is not None and age > PIN_MAX_AGE_DAYS:
            findings.append(
                Finding("R-EXPIRED", "MUST", rel, line_of(text, match.start()),
                        "@vfp-exempt " + ",".join(rules), True, "@hotpath", None,
                        f"exemption verified {verified}, {age}d old > {PIN_MAX_AGE_DAYS}d "
                        "-- re-verify against a current OG-profile capture, then bump the date")
            )
    return findings


def scan_function(
    rel: str,
    func: Function,
    text: str,
    exemptions: Sequence[Exemption],
    hot_reason: str,
) -> list[Finding]:
    findings: list[Finding] = []
    float_ids = float_names_in(func.header + "\n" + func.body)
    loops = split_loops(func.body)

    def maybe(
        rule: str,
        severity: Severity,
        rel_off: int,
        detail: str,
        *,
        untyped: bool = False,
    ) -> None:
        abs_index = func.start + rel_off
        line = line_of(text, abs_index) if 0 <= abs_index < len(text) else func.line
        cover = standing_covers(rel, func.name, rule)
        local = exemption_for_index(text, exemptions, abs_index)
        if local and rule in local.rules:
            cover = cover or "inline"
        if cover:
            return
        sev: Severity = "SHOULD" if untyped and severity == "MUST" else severity
        findings.append(
            Finding(
                rule, sev, rel, line, func.name, True, hot_reason, None,
                detail if not untyped else f"untyped-heuristic: {detail}",
            )
        )

    for _off, loop in loops:
        loop_abs = func.body.find(loop) if loop else 0
        for match in CAST_INT_RE.finditer(loop):
            expr = loop[match.end() : match.end() + 80]
            untyped = not looks_float_expr(expr, float_ids) and "." not in expr and "float" not in expr
            if looks_int_expr(expr) and not looks_float_expr(expr, float_ids):
                continue
            maybe("R2", "MUST", max(loop_abs, 0) + match.start(), "float-to-int cast in loop body", untyped=untyped)

        for match in re.finditer(r"\b(if|while|for)\s*\(", loop):
            start = match.end() - 1
            depth = 0
            j = start
            while j < len(loop):
                if loop[j] == "(":
                    depth += 1
                elif loop[j] == ")":
                    depth -= 1
                    if depth == 0:
                        j += 1
                        break
                j += 1
            cond = loop[start:j]
            if match.group(1) == "for":
                parts = cond.strip("()").split(";")
                cond_expr = parts[1] if len(parts) >= 2 else cond
            else:
                cond_expr = cond
            if condition_is_float(cond_expr, float_ids):
                maybe(
                    "R3", "MUST", max(loop_abs, 0) + match.start(),
                    "float comparison driving control flow in hot loop",
                    untyped=not (float_ids or UNSUFFIXED_FLOAT_RE.search(cond_expr) or re.search(r"[fF]\b", cond_expr)),
                )

        for match in re.finditer(r"\?[^;]+?:", loop):
            if looks_float_expr(match.group(0), float_ids):
                maybe("R3", "MUST", max(loop_abs, 0) + match.start(), "float ternary in hot loop")

        if FOR_FLOAT_RE.search(loop) or FOR_FLOAT_RE.search(func.body[_off : _off + 80] if _off >= 0 else ""):
            maybe("R5", "MUST", _off, "float-typed induction variable")

        for match in re.finditer(r"([A-Za-z_]\w*)\s*\[([^\]]+)\]", loop):
            idx = match.group(2)
            if looks_float_expr(idx, float_ids) and not looks_int_expr(idx):
                maybe("R5", "MUST", max(loop_abs, 0) + match.start(), "float value used as array subscript")

        for match in re.finditer(r"(?<![=\!<>])/(?![/\*=])", loop):
            left = loop[max(0, match.start() - 40) : match.start()]
            right = loop[match.end() : match.end() + 40]
            if looks_float_expr(left, float_ids) or looks_float_expr(right, float_ids):
                if looks_int_expr(left) and looks_int_expr(right) and not looks_float_expr(left + right, float_ids):
                    continue
                maybe("R7", "SHOULD", max(loop_abs, 0) + match.start(), "float division in hot loop")

        for match in SQRT_RE.finditer(loop):
            maybe("R7", "SHOULD", max(loop_abs, 0) + match.start(), "sqrtf in hot loop")

    whole = func.header + "\n" + func.body
    if DOUBLE_TYPE_RE.search(whole):
        match = DOUBLE_TYPE_RE.search(whole)
        assert match is not None
        maybe("R4", "MUST", match.start(), "double/long double in hot path")
    for match in UNSUFFIXED_FLOAT_RE.finditer(whole):
        maybe("R4", "MUST", match.start(), "unsuffixed float literal promotes to double")
    for match in LIBM_RE.finditer(whole):
        maybe("R6", "MUST", match.start(), "libm transcendental in hot path")
    for match in FX_IDENT_RE.finditer(whole):
        window = whole[match.start() : match.start() + 60]
        if re.search(r"[+\-*/]", window) and looks_float_expr(window, float_ids):
            maybe("R8", "SHOULD", match.start(), "fixed-point identifier mixed with float")

    if FOR_FLOAT_RE.search(func.header + func.body[:120]):
        maybe("R5", "MUST", 0, "float-typed induction variable")

    loop_spans = []
    for off, loop in loops:
        loop_spans.append((off, off + len(loop) + 8))
    for match in re.finditer(r"(?<![=\!<>])/(?![/\*=])", func.body):
        if any(start <= match.start() <= end for start, end in loop_spans):
            continue
        left = func.body[max(0, match.start() - 40) : match.start()]
        right = func.body[match.end() : match.end() + 40]
        if looks_float_expr(left, float_ids) or looks_float_expr(right, float_ids):
            if looks_int_expr(left) and looks_int_expr(right) and not looks_float_expr(left + right, float_ids):
                continue
            maybe("R7", "SHOULD", match.start(), "float division inner_loop=false")
    for match in SQRT_RE.finditer(func.body):
        if any(start <= match.start() <= end for start, end in loop_spans):
            continue
        maybe("R7", "SHOULD", match.start(), "sqrtf inner_loop=false")

    return findings


def scan_file(
    path: Path,
    seed: Mapping[str, HotEntry],
    cold: Sequence[str],
    *,
    force_hot: bool = False,
) -> list[Finding]:
    rel = repo_rel(path) if _is_under(path.resolve(), REPO_ROOT) else path.name
    rel_key = rel if force_hot else repo_rel(path)
    text = path.read_text(encoding="utf-8", errors="replace")
    findings = gate_exemptions(text, rel_key)
    exemptions = parse_exemptions(text)
    annotated_lines = {line_of(text, m.start()) for m in HOTPATH_ANN_RE.finditer(text)}
    functions = parse_functions(text)
    for func in functions:
        annotated = any(func.line - 3 <= ln <= func.line for ln in annotated_lines)
        hot, reason = classify_hot(rel_key, func, seed, annotated or force_hot, cold)
        if force_hot:
            hot, reason = True, reason or "seed"
        if not hot:
            continue
        findings.extend(scan_function(rel_key, func, text, exemptions, reason))
    if force_hot and not functions:
        dummy = Function("body", 0, len(text), strip_comments_keep_lines(text), 1, "")
        findings.extend(scan_function(rel_key, dummy, text, exemptions, "seed"))
    return findings


# ---------------------------------------------------------------------------
# Adoption-debt BASELINE RATCHET (replaces T2K's inc_hot baseline).
#
# contracts/hot_findings_baseline.txt holds one key per line:
#     <rule>|<file>|<symbol>|<normalized-detail>
# normalized-detail is whitespace-collapsed with digit runs replaced by N, so a
# finding survives line shifts and renumbering (the same lesson T2K's R11 baseline
# learned). A finding whose key is in the baseline is SUPPRESSED but counted; a
# finding not in the baseline (and not exempted) is NEW and fails the build. The
# baseline is a burn-down list: shrinking it is the audit's job, growing it is a
# build failure. audit_arm11_math.py shares this file under rules TRIG / R4-D.
# ---------------------------------------------------------------------------

HOT_FINDINGS_BASELINE = CONTRACTS_DIR / "hot_findings_baseline.txt"
# Rules owned by audit_arm11_math.py; gates_vfp --bless preserves their lines.
ARM11_MATH_RULES = frozenset({"TRIG", "R4-D"})

BASELINE_HEADER = """\
# Adoption-debt hot-path baseline -- contracts/vfp_hotpath.md §4 (Baseline != exemption).
#
# One finding key per line:  <rule>|<file>|<symbol>|<normalized-detail>
# normalized-detail is whitespace-collapsed and digit runs replaced with N, so a
# key survives line shifts. Findings whose key is here are SUPPRESSED but counted
# ("baselined: N"); any finding NOT here (and not exempted) is NEW and FAILS.
# This is tracked debt with a ratchet: it may only SHRINK. Regenerate with:
#     python3 tools/perf/gates_vfp.py --bless
# (audit_arm11_math.py --bless writes the TRIG / R4-D lines and preserves these.)
"""


def normalize_detail(detail: str) -> str:
    collapsed = " ".join(detail.split())
    return re.sub(r"\d+", "N", collapsed)


def finding_key(f: Finding) -> str:
    return f"{f.rule}|{f.file}|{f.symbol}|{normalize_detail(f.detail)}"


def _rule_of_key(key: str) -> str:
    return key.split("|", 1)[0]


def load_baseline(path: Path) -> Counter:
    """Multiset of baselined finding keys (see HOT_FINDINGS_BASELINE)."""
    counts: Counter = Counter()
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return counts
    for line in text.splitlines():
        if not line.strip() or line.startswith("#"):
            continue
        counts[line.strip()] += 1
    return counts


def write_baseline(path: Path, findings: Sequence[Finding], owned_rules: set[str]) -> int:
    """Rewrite the baseline lines owned by `owned_rules`, preserving the rest.

    Returns the number of findings this caller blessed. Lines whose rule is NOT in
    owned_rules are kept verbatim, so gates_vfp and audit_arm11_math can bless the
    same file without clobbering each other's rules.
    """
    existing = load_baseline(path)
    merged: Counter = Counter()
    for k, c in existing.items():
        if _rule_of_key(k) not in owned_rules:
            merged[k] = c
    new = Counter(finding_key(f) for f in findings)
    merged.update(new)
    lines: list[str] = []
    for k in sorted(merged):
        lines.extend([k] * merged[k])
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(BASELINE_HEADER + "\n".join(lines) + ("\n" if lines else ""), encoding="utf-8")
    return sum(new.values())


def gates_owned_rules(existing: Counter) -> set[str]:
    """Every rule present in the baseline except the ones audit_arm11_math owns."""
    return {_rule_of_key(k) for k in existing} - set(ARM11_MATH_RULES)


def filter_baseline(findings: Sequence[Finding], baseline_path: Path) -> tuple[list[Finding], list[Finding]]:
    """Split findings into (new, baselined) using the multiset baseline."""
    allowed = load_baseline(baseline_path)
    new: list[Finding] = []
    baselined: list[Finding] = []
    for f in findings:
        k = finding_key(f)
        if allowed.get(k, 0) > 0:
            allowed[k] -= 1
            baselined.append(f)
        else:
            new.append(f)
    return new, baselined


def scan_tree(
    root: Path,
    corpus: Mapping[str, object],
    *,
    ingest_profile: bool = True,
) -> tuple[list[Finding], str]:
    extra: list[HotEntry] = []
    source = "seed"
    if ingest_profile:
        extra, source = ingest_perf_logs(root / "docs" / "validation")
    seed = seed_map(corpus, extra)
    cold_raw = corpus.get("cold_always", [])
    cold = [str(x) for x in cold_raw] if isinstance(cold_raw, list) else []
    findings: list[Finding] = []
    targets = {entry.rel for entry in seed.values()}
    for path in in_scope_files(root):
        rel = path.name
        if rel not in targets:
            try:
                if not HOTPATH_ANN_RE.search(path.read_text(encoding="utf-8", errors="replace")):
                    continue
            except OSError:
                continue
        findings.extend(scan_file(path, seed, cold))
    return findings, source


def gate_r9(build_dir: Path) -> tuple[list[Finding], str | None]:
    """R9 -- VFP register pressure, from a disassembly. OPTIONAL, SHOULD only.

    Pointed at build/3ds/obj/*.o. Skips with a note when there are no objects or no
    arm-none-eabi-objdump (the host objdump cannot read ARM objects). Never fails.
    """
    if not build_dir.exists():
        return [], "R9 omitted (no build/3ds/obj/*.o)"
    objects = sorted(build_dir.glob("*.o"))
    if not objects:
        return [], "R9 omitted (no build/3ds/obj/*.o)"
    objdump = shutil.which("arm-none-eabi-objdump")
    if not objdump:
        # devkitARM is not always on PATH (e.g. non-login shells); the installed
        # toolchain location is the honest fallback, same binary.
        devkitarm = os.environ.get("DEVKITPRO", "/opt/devkitpro")
        candidate = Path(devkitarm) / "devkitARM" / "bin" / "arm-none-eabi-objdump"
        if candidate.exists():
            objdump = str(candidate)
    if not objdump:
        return [], "R9 omitted (no arm-none-eabi-objdump)"
    findings: list[Finding] = []
    s_reg = re.compile(r"\bs([0-9]|[12][0-9]|3[01])\b")
    for obj in objects[:20]:
        try:
            proc = subprocess.run(
                [objdump, "-d", str(obj)],
                check=False, capture_output=True, text=True, timeout=20,
            )
        except (OSError, subprocess.TimeoutExpired):
            continue
        live = {m.group(0) for m in s_reg.finditer(proc.stdout)}
        if len(live) > 24:
            findings.append(
                Finding("R9", "SHOULD", repo_rel(obj), 0, obj.stem, True, "seed", None,
                        f"live VFP s-regs {len(live)} > 24")
            )
    return findings, None


def gate_corpus_freshness(corpus: Mapping[str, object], docs_dir: Path) -> list[Finding]:
    """The corpus is a MEASUREMENT, not a preference list.

    Only runs when provisional is false (run_audit handles the provisional note).
    Once a capture lands, the corpus must declare `captured` and go stale on the
    same clock, and the date cannot be bumped without a log actually on disk.
    """
    findings: list[Finding] = []
    corpus_rel = repo_rel(DEFAULT_CORPUS)
    captured = corpus.get("captured")
    if not isinstance(captured, str) or not captured.strip():
        findings.append(
            Finding("R-CORPUS", "MUST", corpus_rel, 0, "hotpath_corpus", True, "corpus", None,
                    "corpus declares no `captured` date -- hot is a measurement, so the "
                    "corpus must say when it was measured (or set provisional=true)")
        )
        return findings
    age = None if _frozen() else _age_days(captured)
    if age is not None and age > CORPUS_MAX_AGE_DAYS:
        findings.append(
            Finding("R-CORPUS", "MUST", corpus_rel, 0, "hotpath_corpus", True, "corpus", None,
                    f"hot-path corpus captured {captured}, {age}d old > {CORPUS_MAX_AGE_DAYS}d "
                    "-- 'top 15 of a captured profile' is meaningless against a stale capture")
        )
    newest = ""
    if docs_dir.exists():
        for log in docs_dir.glob("perf-*.log"):
            stamp = re.search(r"(\d{4}-\d{2}-\d{2})", log.name)
            if stamp and stamp.group(1) > newest:
                newest = stamp.group(1)
    if newest and captured > newest:
        findings.append(
            Finding("R-CORPUS", "SHOULD", corpus_rel, 0, "hotpath_corpus", True, "corpus", None,
                    f"corpus claims captured {captured} but the newest perf log on disk is "
                    f"{newest} -- the date was bumped without a capture")
        )
    return findings


def run_audit(
    root: Path | None = None,
    *,
    corpus_path: Path | None = None,
    makefile: Path | None = None,
    ingest_profile: bool = True,
    scan_sources: bool = True,
    baseline_path: Path | None = None,
    use_baseline: bool = True,
) -> AuditResult:
    root = root or REPO_ROOT
    corpus = load_corpus(corpus_path or DEFAULT_CORPUS)
    findings: list[Finding] = []
    notes: list[str] = []
    findings.extend(gate_r10(makefile or MAKEFILE))
    findings.extend(gate_r1(root))
    if corpus.get("provisional") is True:
        notes.append(
            "corpus is a static seed, not a profile capture; top-15 criterion dormant; "
            "take a capture and set provisional=false"
        )
    else:
        findings.extend(gate_corpus_freshness(corpus, root / "docs" / "validation"))
    source = "seed"
    if scan_sources:
        scanned, source = scan_tree(root, corpus, ingest_profile=ingest_profile)
        findings.extend(scanned)
    r9, r9note = gate_r9(root / "build" / "3ds" / "obj")
    findings.extend(r9)
    if r9note:
        notes.append(r9note)
    notes.append(
        "R10: -mfpu not required (devkitARM armv6k defaults to VFPv2); verify the "
        "effective FPU with `arm-none-eabi-gcc -Q --target-help`, not from this gate."
    )
    frozen = _frozen()
    if frozen:
        notes.append(
            f"staleness clocks waived (corpus age, pin age): project frozen at {frozen}. "
            "Substantive rules still run. VFP_FROZEN_AT='' restores them."
        )
    findings = _dedupe(findings)

    bp = baseline_path or HOT_FINDINGS_BASELINE
    if use_baseline:
        if not bp.exists():
            notes.append(
                f"baseline missing ({repo_rel(bp)}): treating as empty -- every finding is "
                "new; run --bless to adopt the current set as debt"
            )
        new, baselined = filter_baseline(findings, bp)
    else:
        new, baselined = list(findings), []

    new_by_rule = Counter(f.rule for f in new)
    baselined_by_rule = Counter(f.rule for f in baselined)
    blockers = sum(1 for f in new if f.severity == "MUST")
    reviews = sum(1 for f in new if f.severity == "SHOULD")
    status: Status = "FAIL" if new else "PASS"
    return AuditResult(
        status, blockers, reviews, new, source, notes,
        baselined=len(baselined),
        baselined_by_rule=dict(baselined_by_rule),
        new_by_rule=dict(new_by_rule),
    )


def write_result(result: AuditResult, dest: Path) -> None:
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_text(json.dumps(result.to_json(), indent=2) + "\n", encoding="utf-8")


def _cli(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="gates_vfp", description="VFP hot-path gates R1-R10 (Forsaken)")
    parser.add_argument("--root", type=Path, default=REPO_ROOT)
    parser.add_argument("--corpus", type=Path, default=DEFAULT_CORPUS)
    parser.add_argument("--makefile", type=Path, default=None)
    parser.add_argument("--out", type=Path, default=DEFAULT_LOG)
    parser.add_argument("--baseline", type=Path, default=HOT_FINDINGS_BASELINE)
    parser.add_argument("--no-profile", action="store_true")
    parser.add_argument("--json", action="store_true")
    parser.add_argument("--no-baseline", action="store_true",
                        help="ignore the baseline: report every finding as new (proves the ratchet)")
    parser.add_argument("--bless", action="store_true",
                        help="rewrite the baseline from the current full finding set and exit")
    parser.add_argument("--show-baseline", action="store_true", help="print baseline size and exit")
    args = parser.parse_args(argv)

    if args.show_baseline:
        n = sum(load_baseline(args.baseline).values())
        print(f"baseline {repo_rel(args.baseline)}: {n} entr{'y' if n == 1 else 'ies'}")
        return 0

    if args.bless:
        result = run_audit(
            args.root, corpus_path=args.corpus, makefile=args.makefile,
            ingest_profile=not args.no_profile, use_baseline=False,
        )
        owned = gates_owned_rules(load_baseline(args.baseline)) | {f.rule for f in result.findings}
        n = write_baseline(args.baseline, result.findings, owned)
        print(f"blessed {n} finding(s) into {repo_rel(args.baseline)}")
        return 0

    result = run_audit(
        args.root, corpus_path=args.corpus, makefile=args.makefile,
        ingest_profile=not args.no_profile,
        baseline_path=args.baseline, use_baseline=not args.no_baseline,
    )
    write_result(result, args.out)
    if args.json:
        json.dump(result.to_json(), sys.stdout, indent=2)
        sys.stdout.write("\n")
        return 1 if result.status == "FAIL" else 0

    print(f"{result.status} blockers={result.blockers} reviews={result.reviews} "
          f"baselined={result.baselined}")
    print(f"baselined: {result.baselined} (adoption debt — burn down, never grow)")
    rules = sorted(set(result.new_by_rule) | set(result.baselined_by_rule))
    for rule in rules:
        print(f"  {rule}: new={result.new_by_rule.get(rule, 0)} "
              f"baselined={result.baselined_by_rule.get(rule, 0)}")
    for note in result.notes:
        print(f"note: {note}")
    for finding in result.findings:
        print(f"{finding.rule} {finding.severity} {finding.file}:{finding.line} "
              f"{finding.symbol} {finding.detail}")
    return 1 if result.status == "FAIL" else 0


if __name__ == "__main__":
    try:
        raise SystemExit(_cli())
    except BrokenPipeError:
        # Piping into `head`/`less` closes the pipe early; a gate tool should not
        # traceback over that. Silence the shutdown flush and keep the FAIL exit.
        _devnull = os.open(os.devnull, os.O_WRONLY)
        os.dup2(_devnull, sys.stdout.fileno())
        raise SystemExit(1)
