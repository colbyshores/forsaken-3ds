#!/usr/bin/env python3
"""
extract_remaster_levels.py - Import additional levels from Forsaken Remastered (Night Dive)

The Remaster's ForsakenEX.kpf is a ZIP archive containing the original 1998
level data (BSP/MXV/GOL/...) plus extra levels not shipped in the original
release. The data files are byte-identical to the 1998 originals, so the
3DS engine loader handles them with no code changes — we only need to copy
the per-level directories into Data/Levels/ and have the build pipeline
convert per-level BMP textures to PNG.

Levels imported:
    SP   - defend2, stableizers, powerdown, starship, battlebase, munitions,
           biolab, ramqan, temple
    N64  - nuken64, shipn64, aztec64, blackhole, genstation, tuben64, fishy,
           final
    MP   - dabiz, fourball, gas, smalls, storm, sunk, tworooms, astro,
           tunnels, ians, geodome

Skipped: placeholder1, testmap, unknownN64Level1, unknownN64Level2 (incomplete
or test-only — missing critical files like .stp / .gol / .mis).

The 1280x720 loading-screen .png at each level's root is also skipped — the
engine uses the existing .pic file for in-engine loading screens.

Usage:
    python3 extract_remaster_levels.py <ForsakenEX.kpf> [--output Data/Levels]
"""

import argparse
import os
import shutil
import sys
import tempfile
import zipfile


NEW_LEVELS = [
    # SP campaign extras
    "defend2", "stableizers", "powerdown", "starship", "battlebase",
    "munitions", "biolab", "ramqan", "temple",
    # N64-secret levels
    "nuken64", "shipn64", "aztec64", "blackhole", "genstation",
    "tuben64", "fishy", "final",
    # MP arenas
    "dabiz", "fourball", "gas", "smalls", "storm", "sunk", "tworooms",
    "astro", "tunnels", "ians", "geodome",
]

# Levels referenced as MP_Level in the remaster's defs/mapInfo.txt.
# Appended to battle.dat so the multiplayer level select sees them.
NEW_MP_LEVELS = [
    "dabiz", "fourball", "gas", "smalls", "storm", "sunk", "tworooms",
    "astro", "tunnels", "ians", "geodome",
]


def kpf_levels(kpf_path):
    """Return {lowercase_basename: actual_dirname} for every level in the KPF."""
    out = {}
    with zipfile.ZipFile(kpf_path) as z:
        for name in z.namelist():
            parts = name.split("/")
            if len(parts) < 2 or parts[0] != "levels" or not parts[1]:
                continue
            out.setdefault(parts[1].lower(), parts[1])
    return out


def extract_level(z, src_dir, dst_dir):
    """Extract a level directory from the open zip into dst_dir.

    Skips the level-root .png (1280x720 loading screen — not used) and
    every per-locale subdirectory (de/es/fr/it/jp/ru). Per-level texture
    PNGs (under textures/) are kept since they're real game textures.
    """
    locale_prefixes = tuple(
        f"{src_dir}/{loc}/" for loc in ("de", "es", "fr", "it", "jp", "ru")
    )
    src_prefix = src_dir + "/"
    for info in z.infolist():
        if not info.filename.startswith(src_prefix):
            continue
        rel = info.filename[len(src_prefix):]
        if not rel:
            continue
        if info.is_dir():
            continue
        # Skip per-locale subdirs.
        if info.filename.startswith(locale_prefixes):
            continue
        # Skip level-root loading-screen PNG (e.g., levels/biolab/biolab.png).
        # Per-level texture PNGs (textures/*.png) are kept.
        if "/" not in rel and rel.lower().endswith(".png"):
            continue
        out_path = os.path.join(dst_dir, rel)
        os.makedirs(os.path.dirname(out_path) or ".", exist_ok=True)
        with z.open(info) as src, open(out_path, "wb") as dst:
            shutil.copyfileobj(src, dst)


def append_unique(dat_path, names):
    """Append each name to dat_path on its own line if not already present."""
    existing = set()
    if os.path.exists(dat_path):
        with open(dat_path) as f:
            for line in f:
                existing.add(line.strip().lower())
    new_lines = [n for n in names if n.lower() not in existing]
    if not new_lines:
        return 0
    # Make sure the file ends with a newline before appending.
    if os.path.exists(dat_path):
        with open(dat_path, "rb") as f:
            f.seek(0, os.SEEK_END)
            size = f.tell()
            if size > 0:
                f.seek(-1, os.SEEK_END)
                last = f.read(1)
            else:
                last = b"\n"
    else:
        last = b"\n"
    with open(dat_path, "ab") as f:
        if last != b"\n":
            f.write(b"\n")
        for n in new_lines:
            f.write(n.encode() + b"\n")
    return len(new_lines)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("kpf", help="Path to ForsakenEX.kpf from Forsaken Remastered")
    ap.add_argument("--output", default="Data/Levels",
                    help="Destination level directory (default: Data/Levels)")
    args = ap.parse_args()

    if not os.path.isfile(args.kpf):
        print(f"ERROR: KPF not found: {args.kpf}", file=sys.stderr)
        return 1

    if not os.path.isdir(args.output):
        print(f"ERROR: Output directory does not exist: {args.output}",
              file=sys.stderr)
        return 1

    available = kpf_levels(args.kpf)
    copied = []
    skipped_existing = []
    missing = []

    with zipfile.ZipFile(args.kpf) as z:
        for level in NEW_LEVELS:
            actual = available.get(level.lower())
            if not actual:
                missing.append(level)
                continue
            dst = os.path.join(args.output, actual)
            if os.path.isdir(dst):
                skipped_existing.append(actual)
                continue
            os.makedirs(dst, exist_ok=True)
            extract_level(z, f"levels/{actual}", dst)
            copied.append(actual)

    levels_dat = os.path.join(args.output, "levels.dat")
    battle_dat = os.path.join(args.output, "battle.dat")
    added_levels = append_unique(levels_dat, NEW_LEVELS)
    added_battle = append_unique(battle_dat, NEW_MP_LEVELS)

    print(f"Copied {len(copied)} levels: {', '.join(copied) if copied else '(none)'}")
    if skipped_existing:
        print(f"Already present, left untouched: "
              f"{', '.join(skipped_existing)}")
    if missing:
        print(f"WARNING: not found in KPF: {', '.join(missing)}")
    print(f"Appended {added_levels} entries to {levels_dat}")
    print(f"Appended {added_battle} entries to {battle_dat}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
