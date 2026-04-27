#!/usr/bin/env python3
"""
extract_remaster_levels.py - Import Forsaken Remastered (Night Dive) extras

Pulls 28 extra levels and 9 new music tracks out of Forsaken Remastered's
ForsakenEX.kpf and the Steam install's music/OGG/ directory, in shape for an
EDITION=remaster build of the 3DS port. The Remaster's level data files
(BSP/MXV/GOL/...) are byte-identical to the 1998 originals, so the engine
loader handles them with no code changes — we only need to copy the per-level
directories into Data/Levels/ so the Makefile staging step picks them up.

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

Music: the Remaster ships an 18-track OGG library in music/OGG/. Tracks 1-9
are the same compositions as the 1998 CD audio (which extract_assets.py has
already extracted to track02.dsp..track10.dsp), so this script only converts
the 9 tracks unique to the Remaster — Labyrinth, Nubia, Pyrolite, the Force
Of Angels remix of The Dead System, four N64 arrangements, and Cyclotron —
into track11.dsp..track19.dsp at romfs/music/. Requires ffmpeg and a
gc-dspadpcm-encode binary on PATH (or in the locations extract_assets.py
also probes).

Usage:
    python3 extract_remaster_levels.py <ForsakenEX.kpf> \\
        [--steam-dir /path/to/Forsaken\\ Remastered] \\
        [--levels-output Data/Levels] \\
        [--music-output romfs/music] \\
        [--skip-music] [--skip-levels]

The script auto-detects the Steam install dir adjacent to ForsakenEX.kpf if
--steam-dir is omitted.
"""

import argparse
import os
import shutil
import subprocess
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


# Music tracks unique to the Remaster (compositions 10-18 of the OGG library).
# OGG basename → output DSP track number on disk. Output numbering keeps
# CD-style 1-offset alignment so the user's existing track02-10 (from the
# 1998 CD rip via extract_assets.py) stays valid for tracks 1-9 of the
# Remaster's library, which are the same compositions.
NEW_MUSIC_TRACKS = [
    ("10_Labyrinth.ogg",                                  11),
    ("11_Nubia.ogg",                                      12),
    ("12_Pyrolite.ogg",                                   13),
    ("13_The Dead System [Force Of Angels Remix].ogg",    14),
    ("14_The_Dead_System [N64 Version].ogg",              15),
    ("15_Condemned [N64 Version].ogg",                    16),
    ("16_Pure_Power [N64 Version].ogg",                   17),
    ("17_Flame_Out [N64 Version].ogg",                    18),
    ("18_Cyclotron.ogg",                                  19),
]


# ----- KPF level extraction --------------------------------------------------

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


def import_levels(kpf_path, output_dir):
    if not os.path.isdir(output_dir):
        print(f"ERROR: --levels-output dir does not exist: {output_dir}",
              file=sys.stderr)
        return False

    available = kpf_levels(kpf_path)
    copied = []
    skipped_existing = []
    missing = []

    with zipfile.ZipFile(kpf_path) as z:
        for level in NEW_LEVELS:
            actual = available.get(level.lower())
            if not actual:
                missing.append(level)
                continue
            dst = os.path.join(output_dir, actual)
            if os.path.isdir(dst):
                skipped_existing.append(actual)
                continue
            os.makedirs(dst, exist_ok=True)
            extract_level(z, f"levels/{actual}", dst)
            copied.append(actual)

    print(f"Levels: copied {len(copied)} "
          f"({', '.join(copied) if copied else 'none'})")
    if skipped_existing:
        print(f"  Already present, left untouched: "
              f"{', '.join(skipped_existing)}")
    if missing:
        print(f"  WARNING: not found in KPF: {', '.join(missing)}")
    return True


# ----- OGG → DSP-ADPCM music conversion --------------------------------------

def find_dspadpcm_encoder():
    """Reproduce extract_assets.py's encoder search order, plus our
    persistent build cache under ~/.cache/."""
    found = shutil.which("dspadpcm")
    if found:
        return found
    candidates = [
        "/tmp/dspadpcm/dspadpcm",
        "./dspadpcm",
        os.path.expanduser("~/dspadpcm"),
        os.path.expanduser("~/.cache/forsaken-3ds-port/gc-dspadpcm-encode/dspadpcm"),
    ]
    for p in candidates:
        if os.path.isfile(p) and os.access(p, os.X_OK):
            return p
    return None


def build_dspadpcm_encoder():
    """Clone and build gc-dspadpcm-encode into ~/.cache/, returning the
    path to the produced binary. Cache survives reboots — unlike
    /tmp/dspadpcm/ where extract_assets.py historically dropped it.

    The upstream repo (jackoalan/gc-dspadpcm-encode) ships a Qt qmake
    project file rather than a Makefile, so we invoke the compiler
    directly with the two .c sources and the alsa library it needs.

    Returns None on failure (missing git / cc, missing libasound, or
    compile error)."""
    cache_root = os.path.expanduser("~/.cache/forsaken-3ds-port")
    src_dir = os.path.join(cache_root, "gc-dspadpcm-encode")
    binary = os.path.join(src_dir, "dspadpcm")

    if os.path.isfile(binary) and os.access(binary, os.X_OK):
        return binary

    if shutil.which("git") is None:
        print("  ERROR: 'git' not on PATH — can't fetch "
              "gc-dspadpcm-encode source.", file=sys.stderr)
        return None
    cc = shutil.which("cc") or shutil.which("gcc")
    if cc is None:
        print("  ERROR: no C compiler ('cc' or 'gcc') on PATH.",
              file=sys.stderr)
        return None

    os.makedirs(cache_root, exist_ok=True)

    # Clone (or refresh a stale partial checkout).
    if not os.path.isfile(os.path.join(src_dir, "main.c")):
        if os.path.isdir(src_dir):
            shutil.rmtree(src_dir)
        print(f"  Cloning gc-dspadpcm-encode -> {src_dir}")
        try:
            subprocess.run(
                ["git", "clone", "--depth=1",
                 "https://github.com/jackoalan/gc-dspadpcm-encode", src_dir],
                check=True,
                stdout=subprocess.DEVNULL, stderr=subprocess.PIPE,
            )
        except subprocess.CalledProcessError as e:
            print(f"  git clone failed: {e.stderr.decode(errors='replace')}",
                  file=sys.stderr)
            return None

    sources = [os.path.join(src_dir, "main.c"),
               os.path.join(src_dir, "grok.c")]
    for s in sources:
        if not os.path.isfile(s):
            print(f"  ERROR: expected source {s} missing after clone.",
                  file=sys.stderr)
            return None

    print(f"  Compiling gc-dspadpcm-encode -> {binary}")
    try:
        subprocess.run(
            [cc, "-O2", "-o", binary, *sources, "-lasound", "-lm"],
            check=True,
            stdout=subprocess.DEVNULL, stderr=subprocess.PIPE,
        )
    except subprocess.CalledProcessError as e:
        msg = e.stderr.decode(errors="replace")
        if "asound" in msg or "alsa" in msg.lower():
            msg += ("\n  Hint: install ALSA dev headers — "
                    "Debian/Ubuntu: 'sudo apt install libasound2-dev'")
        print(f"  compile failed: {msg}", file=sys.stderr)
        return None

    if not (os.path.isfile(binary) and os.access(binary, os.X_OK)):
        print(f"  ERROR: compile reported success but binary missing "
              f"at {binary}", file=sys.stderr)
        return None

    return binary


def ensure_dspadpcm_encoder():
    """Find or build gc-dspadpcm-encode. Returns path or None."""
    found = find_dspadpcm_encoder()
    if found:
        return found
    print("  gc-dspadpcm-encode not found — building from source...")
    return build_dspadpcm_encoder()


def convert_ogg_to_dsp(ogg_path, dsp_path, encoder):
    """Decode OGG -> 32 kHz mono PCM16 WAV with ffmpeg, then encode to DSP."""
    with tempfile.TemporaryDirectory(prefix="forsaken-music-") as td:
        wav = os.path.join(td, "track.wav")
        # 32 kHz mono signed 16-bit LE PCM — exactly what music_3ds.c expects
        # and what extract_assets.py also produces for the 1998 CD audio.
        try:
            subprocess.run([
                "ffmpeg", "-y", "-loglevel", "error",
                "-i", ogg_path,
                "-ac", "1", "-ar", "32000",
                "-acodec", "pcm_s16le",
                wav,
            ], check=True)
        except (subprocess.CalledProcessError, FileNotFoundError) as e:
            print(f"  ffmpeg failed for {os.path.basename(ogg_path)}: {e}",
                  file=sys.stderr)
            return False
        try:
            subprocess.run([encoder, wav, dsp_path], check=True,
                           stdout=subprocess.DEVNULL,
                           stderr=subprocess.DEVNULL)
        except (subprocess.CalledProcessError, FileNotFoundError) as e:
            print(f"  dspadpcm-encode failed for {os.path.basename(ogg_path)}: {e}",
                  file=sys.stderr)
            return False
    return True


def import_music(steam_dir, music_output):
    ogg_dir = os.path.join(steam_dir, "music", "OGG")
    if not os.path.isdir(ogg_dir):
        print(f"ERROR: Remaster OGG dir not found: {ogg_dir}\n"
              f"  Pass --steam-dir explicitly, or use --skip-music.",
              file=sys.stderr)
        return False

    if not shutil.which("ffmpeg"):
        print("ERROR: ffmpeg not on PATH. "
              "Install it (Debian/Ubuntu: apt install ffmpeg) or pass --skip-music.",
              file=sys.stderr)
        return False

    encoder = ensure_dspadpcm_encoder()
    if not encoder:
        print("ERROR: gc-dspadpcm-encode unavailable. Build it manually with:\n"
              "  git clone https://github.com/jackoalan/gc-dspadpcm-encode\n"
              "  cd gc-dspadpcm-encode && make\n"
              "Or pass --skip-music to skip music conversion.",
              file=sys.stderr)
        return False

    os.makedirs(music_output, exist_ok=True)

    converted = 0
    skipped = 0
    failed = []
    for ogg_name, track_num in NEW_MUSIC_TRACKS:
        ogg_path = os.path.join(ogg_dir, ogg_name)
        dsp_path = os.path.join(music_output, f"track{track_num:02d}.dsp")
        if not os.path.isfile(ogg_path):
            failed.append(ogg_name)
            continue
        if os.path.isfile(dsp_path):
            skipped += 1
            continue
        print(f"  {ogg_name} -> track{track_num:02d}.dsp")
        if convert_ogg_to_dsp(ogg_path, dsp_path, encoder):
            converted += 1
        else:
            failed.append(ogg_name)

    print(f"Music: converted {converted}, "
          f"already-present {skipped}"
          + (f", failed {len(failed)}" if failed else ""))
    if failed:
        print(f"  Failed/missing OGGs: {', '.join(failed)}")
    return True


# ----- entry point -----------------------------------------------------------

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("kpf", help="Path to ForsakenEX.kpf from Forsaken Remastered")
    ap.add_argument("--steam-dir", default=None,
                    help="Forsaken Remastered Steam install dir "
                         "(default: directory containing the KPF)")
    ap.add_argument("--levels-output", default="Data/Levels",
                    help="Destination level directory (default: Data/Levels)")
    ap.add_argument("--music-output", default="romfs/music",
                    help="Destination music directory (default: romfs/music)")
    ap.add_argument("--skip-levels", action="store_true",
                    help="Don't import level data")
    ap.add_argument("--skip-music", action="store_true",
                    help="Don't convert OGG music tracks")
    args = ap.parse_args()

    if not os.path.isfile(args.kpf):
        print(f"ERROR: KPF not found: {args.kpf}", file=sys.stderr)
        return 1

    steam_dir = args.steam_dir or os.path.dirname(os.path.abspath(args.kpf))

    ok = True
    if not args.skip_levels:
        ok = import_levels(args.kpf, args.levels_output) and ok
    if not args.skip_music:
        ok = import_music(steam_dir, args.music_output) and ok

    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
