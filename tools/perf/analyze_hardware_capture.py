#!/usr/bin/env python3
"""analyze_hardware_capture.py — validate one Forsaken PERF artifact.

This is the parser for the *manual hardware capture* format produced by the
clean handoff binaries: a normal teardown dump bounded by
``PERF: dump begin`` / ``PERF: dump end`` and no ``PERF: === level=... ===``
marker.  It also understands the emulator sweep format (level blocks followed
by a one-frame teardown dump), so existing evidence can be used as a parser
test.

Why this exists: tools/perf/analyze_sweep.py keys its rows on level markers.
A manual handoff capture has no level marker, so its gameplay dump would be
silently ignored.  That is a validation hazard for the pending human hardware
session, not a measurement by itself.

What this tool does NOT do:
  * it does not contact or infer anything about hardware;
  * it does not clear an AGENTS.md P6 hardware claim;
  * it does not replace analyze_sweep.py for level-sweep artifacts.

Typical use after a human copies a run off the SD card:

    python3 tools/perf/analyze_hardware_capture.py \
        --perf forsaken_perf-default-powerdown.txt \
        --boot forsaken_boot-default-powerdown.log \
        --require-og-profile --out capture-default-powerdown.analysis.txt

For the opt-in bucket verifier build, add ``--require-verify``.  Add
``--require-console og`` for original/non-New 3DS hardware or
``--require-console n3ds`` for a New 3DS run.  The script exits 0 only when
its mechanical checks pass; any validation failure exits 2.

Selection rules:
  * ``--block auto`` (default): use the last level block with gameplay frames
    if one exists, otherwise the last dump block with more than one frame.
    This keeps emulator sweep artifacts on their gameplay level block and
    manual hardware artifacts on their teardown dump.
  * ``--block last-level`` / ``last-dump`` force one kind of block.
  * ``--allow-teardown-dump`` accepts a dump block with 0 or 1 frames (for
    inspecting a failed or non-gameplay run).

The bucket verifier line may be inside the selected dump block (manual handoff
format) or in the preceding level block (emulator sweep format).  The report
says which source was used.
"""

import argparse
import json
import re
import sys
from pathlib import Path

# Must stay aligned with PERF_S_* in perf_capture.h. This is intentionally a
# local copy: the hardware parser must remain independent of engine headers.
STAGES = [
    "frame", "maingame", "render", "fsbegin", "mROUT", "cam", "fsend", "vis",
    "bg", "opaque", "trans", "enemies", "spotfx", "bgobj", "models", "polys",
    "rtlights", "water", "bullets", "ships", "fires", "pickups", "xlights",
    "afterbrn", "fmpoly", "conditions", "triggers", "regen", "restart", "time",
    "multiples", "goals", "shipmode", "shipcollide", "shippre", "shippost",
    "shipmodel", "shipcam", "shipdmg", "shipnode", "losnode", "bullprim",
    "bullsec", "opqtex", "opqclip", "opqlines", "opqmodel", "opqpolys",
    "mdlinterp", "mdllight", "mdlexec", "mdlentry", "mdltransform", "mdllod",
    "mdlbounds", "mdlstatic", "mdlwrap", "interpprobe", "interptotal",
    "interponly", "interpsim", "presentloop", "present",
]

# svcGetSystemTick() on this build is the ARM11 tick (268.11184 MHz).  This is
# the same convention as tools/perf/analyze_sweep.py and perf_capture_3ds.c.
TICK_HZ = 268111840.0

STAGE_RE = re.compile(
    r"^PERF: stage=(\d+) count=(\d+) total=(\d+) max=(\d+)$")
HIST_RE = re.compile(r"^PERF: hist ms<=(\d+) count=(\d+)$")
FRAMES_RE = re.compile(r"^PERF: frames=(\d+)$")
LEVEL_RE = re.compile(r"^PERF: === level=(.+?) ===$")
HUNK_RE = re.compile(
    r"^PERF: hunk level_used=(\d+) startup_used=(\d+) "
    r"avail=(\d+) total=(\d+)$")
VERIFY_RE = re.compile(
    r"^PERF: model_bucket_verify groups=(\d+) entries=(\d+) "
    r"mismatches=(\d+) first_group=(-?\d+) "
    r"first_expected=(\d+) first_actual=(\d+)$")
PLATFORM_RE = re.compile(r"^\[platform\]\s+(.*)$")
BOOT_FIELD_RE = re.compile(r"\b([A-Za-z0-9_]+)=([^ ]+)")


class Invalid(Exception):
    """A mechanical validation failure; message is safe to print."""


def ms_from_ticks(total_ticks, frame_count):
    if frame_count <= 0:
        return 0.0
    return total_ticks / frame_count / TICK_HZ * 1000.0


class Block:
    def __init__(self, kind, name, start_line):
        self.kind = kind
        self.name = name
        self.start_line = start_line
        self.end_line = None
        self.stages = {}
        self.hists = []
        self.frames = None
        self.hunk = None
        self.verify = None
        self.verify_source = "none"
        self.complete = kind != "dump"  # level blocks end implicitly

    @property
    def gameplay_frames(self):
        acc = self.stages.get("frame")
        return acc[0] if acc else 0

    @property
    def maingame_frames(self):
        acc = self.stages.get("maingame")
        return acc[0] if acc else 0


def parse_blocks(path):
    """Return (level_blocks, dump_blocks) in file order.

    Any verifier line seen between blocks is remembered and attached to the
    next explicit block only if that block does not already have one.  This is
    deliberate: emulator level blocks emit the verifier before the final
    teardown dump, while manual handoff dumps emit it inside dump begin/end.
    """
    try:
        lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    except OSError as exc:
        raise Invalid(f"cannot read PERF artifact: {exc}") from exc

    levels = []
    dumps = []
    current = None
    in_dump = False
    last_verify = None

    def close_current(end_line):
        nonlocal current, in_dump
        if current is not None:
            current.end_line = end_line
            if current.kind == "dump":
                current.complete = in_dump is False
                dumps.append(current)
            else:
                levels.append(current)
        current = None
        in_dump = False

    for line_no, raw in enumerate(lines, 1):
        line = raw.rstrip("\r")

        m = STAGE_RE.match(line)
        if m and current is not None:
            idx, count, total, mx = map(int, m.groups())
            if idx < len(STAGES):
                current.stages[STAGES[idx]] = (count, total, mx)
            continue

        m = HIST_RE.match(line)
        if m and current is not None:
            current.hists.append((int(m.group(1)), int(m.group(2))))
            continue

        m = FRAMES_RE.match(line)
        if m and current is not None:
            current.frames = int(m.group(1))
            continue

        m = HUNK_RE.match(line)
        if m and current is not None:
            current.hunk = tuple(map(int, m.groups()))
            continue

        m = VERIFY_RE.match(line)
        if m:
            last_verify = tuple(map(int, m.groups()))
            # Attach to the current explicit block, if one is open.
            if current is not None and current.verify is None:
                current.verify = last_verify
                current.verify_source = (
                    "selected-block" if current.kind == "dump" else "level-block")
            continue

        m = LEVEL_RE.match(line)
        if m:
            close_current(line_no)
            current = Block("level", m.group(1), line_no)
            if last_verify is not None and current.verify is None:
                # Do not carry an old verifier into a brand-new level.
                last_verify = None
            continue

        if line == "PERF: dump begin":
            close_current(line_no)
            current = Block("dump", "teardown", line_no)
            in_dump = True
            continue

        if line == "PERF: dump end":
            if current is None or current.kind != "dump":
                raise Invalid(
                    f"line {line_no}: dump end without dump begin")
            # Mark closure before close_current(); it uses this to distinguish
            # a complete dump from one cut off by EOF or the next block.
            in_dump = False
            close_current(line_no)
            continue

    # EOF: close an open level block; an unterminated dump is incomplete.
    close_current(len(lines) + 1)
    if current is not None and current.kind == "dump":
        pass

    # A verifier line emitted immediately before dump begin belongs to the
    # immediately preceding level block in emulator artifacts.  If no level
    # block exists, it belongs to the following manual dump.
    if last_verify is not None and levels:
        for blk in reversed(levels):
            if blk.verify is None:
                blk.verify = last_verify
                blk.verify_source = "preceding-level"
                break

    return levels, dumps


def choose_block(levels, dumps, mode, allow_teardown_dump):
    candidates = []

    if mode in ("auto", "last-level"):
        gameplay_levels = [b for b in levels if b.gameplay_frames > 0]
        if gameplay_levels:
            candidates = [gameplay_levels[-1]]
        elif mode == "last-level":
            raise Invalid("no level block with gameplay frames")

    if not candidates and mode in ("auto", "last-dump"):
        min_frames = 0 if allow_teardown_dump else 2
        usable_dumps = [d for d in dumps if d.gameplay_frames > min_frames]
        if usable_dumps:
            candidates = [usable_dumps[-1]]
        elif mode == "last-dump":
            if dumps:
                last = dumps[-1]
                raise Invalid(
                    f"last dump has frames={last.gameplay_frames}; "
                    f"use --allow-teardown-dump to inspect it")
            raise Invalid("no dump block")

    if not candidates:
        raise Invalid(
            "no usable PERF block (no level gameplay frames and no dump "
            "with >1 frames; use --allow-teardown-dump only for a "
            "non-gameplay diagnostic)")

    return candidates[0]


def parse_boot(path, require_boot):
    if path is None:
        if require_boot:
            raise Invalid("console/profile checks need --boot")
        return None, {}

    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError as exc:
        raise Invalid(f"cannot read boot log: {exc}") from exc

    platform_lines = []
    fields = {}
    for line in text.splitlines():
        m = PLATFORM_RE.match(line.strip())
        if not m:
            continue
        platform_lines.append(m.group(1))
        for key, value in BOOT_FIELD_RE.findall(m.group(1)):
            fields[key] = value

    if not platform_lines:
        raise Invalid("no [platform] record in boot log")
    return platform_lines, fields


def console_class(boot_fields):
    """Return the console family recorded by main_3ds.c, if known."""
    value = boot_fields.get("is_n3ds")
    if value == "0":
        return "og"
    if value == "1":
        return "n3ds"
    return None


def validate_block(block, require_verify, require_og_profile, require_console,
                   boot_fields):
    errors = []
    verify_errors = []
    og_errors = []
    console_errors = []

    if block.kind == "dump" and not block.complete:
        errors.append("selected dump block is not bounded by dump end")

    if block.frames is None:
        errors.append("selected block has no PERF: frames line")
    else:
        gameplay = block.gameplay_frames
        if gameplay != block.frames:
            errors.append(
                f"frame stage count {gameplay} does not match frames= "
                f"{block.frames}")
        if gameplay == 0:
            errors.append("selected block has zero gameplay frames")

    if block.maingame_frames == 0:
        errors.append("selected block has zero MainGame frames")

    for required in ("fsbegin", "present"):
        if required not in block.stages:
            errors.append(f"selected block is missing stage={required}")

    verify_status = "not-required"
    if require_verify:
        if block.verify is None:
            verify_errors.append("model_bucket_verify result line is missing")
        else:
            groups, entries, mismatches, first_group, first_expected, \
                first_actual = block.verify
            if groups <= 0 or entries <= 0:
                verify_errors.append(
                    "model_bucket_verify has groups<=0 or entries<=0")
            if mismatches != 0:
                verify_errors.append(
                    f"model_bucket_verify mismatches={mismatches}")
            if first_group != -1:
                verify_errors.append(
                    f"model_bucket_verify first_group={first_group}")
            if first_expected != 0 or first_actual != 0:
                verify_errors.append(
                    "model_bucket_verify first_expected/first_actual are "
                    "nonzero")
        verify_status = "PASS" if not verify_errors else "FAIL"

    og_status = "not-required"
    if require_og_profile:
        # OG_PROFILE is a clock/cache/heap policy, not a console class.
        # Both an original 3DS and a New 3DS can run this policy, so
        # is_n3ds is checked separately by --require-console.
        wanted = {
            "og_profile": "1",
            "clock": "268MHz",
            "cache": "L2-off",
            "tier": "OG_PROFILE",
        }
        bad = [
            f"{key}={boot_fields.get(key, '<missing>')}"
            for key, value in wanted.items()
            if boot_fields.get(key) != value
        ]
        if bad:
            og_errors.append(
                "OG-profile boot fields failed: " + ", ".join(bad))
        og_status = "PASS" if not bad else "FAIL"

    detected_console = console_class(boot_fields)
    console_status = "not-required"
    if require_console:
        if detected_console is None:
            console_errors.append(
                "console class is missing or invalid (is_n3ds must be 0/1)")
        elif detected_console != require_console:
            expected = "OG/non-New-3DS" if require_console == "og" \
                else "New 3DS"
            actual = "OG/non-New-3DS" if detected_console == "og" \
                else "New 3DS"
            console_errors.append(
                f"console class is {actual}, expected {expected}")
        console_status = "PASS" if not console_errors else "FAIL"

    errors.extend(verify_errors)
    errors.extend(og_errors)
    errors.extend(console_errors)
    if errors:
        raise Invalid("; ".join(errors))
    return verify_status, og_status, console_status, detected_console


def frame_ms(block):
    count, total, mx = block.stages.get("frame", (0, 0, 0))
    if count <= 1:
        return ms_from_ticks(total, count), mx
    return ms_from_ticks(total - mx, count - 1), mx


def make_report(path, block, boot_path, boot_lines, boot_fields,
                verify_status, og_status, console_status, detected_console):
    fm, frame_max_ticks = frame_ms(block)
    fps = 1000.0 / fm if fm > 0 else 0.0

    lines = []
    lines.append("Forsaken PERF artifact validation — parser evidence only")
    lines.append("This does not establish an AGENTS.md P6 hardware pass.")
    lines.append("")
    lines.append(f"perf: {path}")
    lines.append(f"block: {block.kind} name={block.name} "
                 f"lines={block.start_line}-{block.end_line}")
    lines.append(f"frames: {block.gameplay_frames}")
    lines.append(f"frame ms (worst excluded): {fm:.3f}")
    lines.append(f"fps (same convention): {fps:.3f}")
    lines.append(f"frame max: {frame_max_ticks / TICK_HZ * 1000.0:.3f} ms")
    if block.hunk:
        lines.append(
            "hunk: level_used=%d startup_used=%d avail=%d total=%d"
            % block.hunk)
    if boot_path is not None:
        lines.append(f"boot: {boot_path}")
        if boot_lines:
            lines.append(f"boot platform: {boot_lines[-1]}")
    console_name = {
        "og": "OG/non-New-3DS",
        "n3ds": "New 3DS",
    }.get(detected_console, "unknown")
    lines.append(f"console class: {console_name} "
                 f"(is_n3ds={boot_fields.get('is_n3ds', '<missing>')})")
    lines.append(f"console-class check: {console_status}")
    lines.append(f"og-profile policy check: {og_status}")
    lines.append(f"bucket-verify check: {verify_status}")
    if block.verify is not None:
        lines.append("bucket-verify: groups=%d entries=%d mismatches=%d "
                     "first_group=%d first_expected=%d first_actual=%d "
                     "(source=%s)" % ((block.verify) +
                                      (block.verify_source,)))
    lines.append("")

    lines.append("stage table (ms/frame; count/frame)")
    lines.append("stage                 ms/frame   count/frame")
    for name in STAGES:
        acc = block.stages.get(name)
        if not acc:
            continue
        count, total, mx = acc
        lines.append(f"{name:<20} {ms_from_ticks(total, block.gameplay_frames):10.3f}   "
                     f"{count / block.gameplay_frames:12.3f}")
    lines.append("")

    if block.hists:
        lines.append("frame histogram (8 ms buckets)")
        for bound, count in block.hists:
            lines.append(f"ms<={bound:<5} count={count}")
        lines.append("")

    return "\n".join(lines) + "\n"


def main(argv=None):
    ap = argparse.ArgumentParser(
        description="Validate one Forsaken PERF artifact "
                    "(manual hardware format or emulator sweep format).")
    ap.add_argument("--perf", required=True, type=Path,
                    help="forsaken_perf.txt copied off the SD/emulated SD")
    ap.add_argument("--boot", type=Path,
                    help="matching forsaken_boot.log (recommended)")
    ap.add_argument("--out", type=Path,
                    help="write the report here instead of stdout")
    ap.add_argument("--block", choices=("auto", "last-level", "last-dump"),
                    default="auto")
    ap.add_argument("--allow-teardown-dump", action="store_true",
                    help="accept a dump block with 0/1 frames")
    ap.add_argument("--require-verify", action="store_true",
                    help="require a zero-mismatch MODEL_BUCKET_VERIFY line")
    ap.add_argument("--require-og-profile", action="store_true",
                    help="require the 268MHz/L2-off OG_PROFILE policy "
                         "in --boot")
    ap.add_argument("--require-console", choices=("og", "n3ds"),
                    help="require console class in --boot "
                         "(og=non-New-3DS, n3ds=New 3DS)")
    ap.add_argument("--json", type=Path,
                    help="also write a machine-readable JSON summary")
    args = ap.parse_args(argv)

    try:
        levels, dumps = parse_blocks(args.perf)
        block = choose_block(levels, dumps, args.block,
                             args.allow_teardown_dump)
        need_boot = (args.require_og_profile or
                     args.require_console is not None)
        boot_lines, boot_fields = parse_boot(args.boot, need_boot)
        verify_status, og_status, console_status, detected_console = \
            validate_block(block, args.require_verify,
                           args.require_og_profile, args.require_console,
                           boot_fields)
        report = make_report(args.perf, block, args.boot, boot_lines,
                             boot_fields, verify_status, og_status,
                             console_status, detected_console)
    except Invalid as exc:
        print(f"analyze_hardware_capture: FAIL: {exc}", file=sys.stderr)
        return 2

    if args.out:
        args.out.write_text(report, encoding="utf-8")
    else:
        sys.stdout.write(report)

    if args.json:
        fm, _ = frame_ms(block)
        payload = {
            "perf": str(args.perf),
            "boot": str(args.boot) if args.boot else None,
            "block_kind": block.kind,
            "block_name": block.name,
            "block_lines": [block.start_line, block.end_line],
            "frames": block.gameplay_frames,
            "frame_ms_worst_excluded": fm,
            "fps_same_convention": 1000.0 / fm if fm > 0 else 0.0,
            "hunk": list(block.hunk) if block.hunk else None,
            "boot_fields": boot_fields,
            "og_profile_check": og_status,
            "console_class": detected_console,
            "console_class_check": console_status,
            "bucket_verify_check": verify_status,
            "bucket_verify": list(block.verify) if block.verify else None,
            "bucket_verify_source": block.verify_source,
        }
        args.json.write_text(json.dumps(payload, indent=2) + "\n",
                             encoding="utf-8")

    return 0


if __name__ == "__main__":
    sys.exit(main())
