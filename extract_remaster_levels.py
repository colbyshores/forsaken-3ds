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

    Per-locale subdirectories (de/es/fr/it/jp/ru) are skipped. The
    level-root .png is the 1280x720 loading-screen banner — the engine
    expects 256x128 at <level>.png (the 1998 dimensions; loaded via
    MissionTextPics → tload.c → Change_Ext to .PNG → 3DS texture
    pipeline). We downscale here so the banner appears in the
    crate-menu green screen for Remaster levels.

    Per-level texture PNGs (under textures/) are kept untouched —
    they're real game textures, not banners.
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
        out_path = os.path.join(dst_dir, rel)
        os.makedirs(os.path.dirname(out_path) or ".", exist_ok=True)
        # Level-root .png is the loading-screen banner — downscale to
        # the 1998 256x128 dimension so MissionTextPics resolution
        # matches what tload.c / FSCreateTexture expect.
        if "/" not in rel and rel.lower().endswith(".png"):
            with z.open(info) as src_f:
                _downscale_banner_png(src_f, out_path)
            continue
        with z.open(info) as src, open(out_path, "wb") as dst:
            shutil.copyfileobj(src, dst)


def _downscale_banner_png(src_fileobj, out_path):
    """Read a PNG from src_fileobj, resize to 256x128 with high-quality
    Lanczos resampling, and write as 8-bit RGB PNG to out_path. Matches
    the 1998 banner layout (256x128, RGB)."""
    from PIL import Image
    img = Image.open(src_fileobj)
    # Drop any alpha — banners are opaque, and the engine's BMP-style
    # loader doesn't read RGBA.
    if img.mode != "RGB":
        img = img.convert("RGB")
    img = img.resize((256, 128), Image.Resampling.LANCZOS)
    img.save(out_path, format="PNG", optimize=True)


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


def import_banners(kpf_path, output_dir):
    """Idempotent banner import for the EDITION=remaster build: walk
    every level directory under output_dir and (re-)write its 256x128
    crate-menu banner from the matching 1280x720 PNG in the KPF. This
    intentionally overwrites the original 1998 ISO's greyscale banners
    with Remaster's colorized art — for a Remaster build, the original
    banners shouldn't show up alongside the new ones.

    Levels with no matching KPF entry (intro/cutscene maps like
    accworld, endscene, probeworld) keep their existing banner.

    Lives outside import_levels() so it can run against a fully-
    populated Data/Levels/ — existing banners get overwritten in place,
    matching how the romfs-staging rsync expects to see them."""
    if not os.path.isdir(output_dir):
        print(f"ERROR: --levels-output dir does not exist: {output_dir}",
              file=sys.stderr)
        return False

    written = []
    no_kpf_match = []

    # Build a lowercase → actual-case index of every level-root PNG in
    # the KPF so we can match against local dir names case-insensitively.
    with zipfile.ZipFile(kpf_path) as z:
        kpf_banners = {}
        for name in z.namelist():
            parts = name.split("/")
            if (len(parts) == 3 and parts[0] == "levels"
                    and parts[2].lower().endswith(".png")):
                kpf_banners[parts[1].lower()] = name

        # Walk every local level dir and overwrite its banner if the
        # KPF has a colorized source.
        for entry in sorted(os.listdir(output_dir)):
            local_dir = os.path.join(output_dir, entry)
            if not os.path.isdir(local_dir):
                continue
            kpf_src = kpf_banners.get(entry.lower())
            if not kpf_src:
                no_kpf_match.append(entry)
                continue
            # Output filename matches the local dir's basename, lowercased
            # (the romfs staging step lowercases everything anyway, so
            # this stays in sync regardless of the dir's stored case).
            out_path = os.path.join(local_dir, f"{entry.lower()}.png")
            with z.open(kpf_src) as src_f:
                _downscale_banner_png(src_f, out_path)
            written.append(entry)

    print(f"Banners: wrote {len(written)} 256x128 PNGs (Remaster colorized)")
    if no_kpf_match:
        print(f"  No KPF source — kept existing banner: "
              f"{', '.join(no_kpf_match)}")
    return True


# ----- N64 enemy / pickup model extraction -----------------------------------
#
# The Remaster ships single-mesh .mx files for the simpler N64 enemies
# (Cargodrone, Enforcer, Ghost) and pickups (StablizerCrystal, BlackHole-
# GunPart1..4). Pull just those into Data/models/n64/ so the engine's
# template-copy step can point ModelFilename / ModelType at the real
# meshes instead of substituting 1998-era visuals.
#
# Multi-component bosses (Manmech, Maldroid, Ramqan, DreadNaught, Shield-
# Turret) and N64-specific FX (n64Shockwave) are skipped — they need
# component-tree authoring that the 1998 engine doesn't know about, and
# "Remaster N64 Enemy/Pickup Slot Approximation" for that follow-up scope.

N64_SINGLE_MESH_FILES = [
    # enemies (single-mesh)
    "models/n64/cargodrone.mx",
    "models/n64/enforcer.mx",
    "models/n64/ghost.mx",
    # pickups (all single-mesh)
    "models/n64/stcrys.mx",
    "models/n64/bhgun1.mx",
    "models/n64/bhgun2.mx",
    "models/n64/bhgun3.mx",
    "models/n64/bhgun4.mx",
]

# Boss .cob component files. KEX .cob and 1998 .cob share the PRJX magic
# + version 2 layout (verified empirically: same NumModels int16 + null-
# terminated path strings + binary tree section). The 1998 PreLoadCompObj
# / LoadCompObj loaders should accept these without code changes.
N64_BOSS_COB_FILES = [
    "bgobjects/n64/cargodrone.cob",   # already covered by single-mesh path,
                                      # included for completeness / .cob parity
    "bgobjects/n64/dreadnaught.cob",
    "bgobjects/n64/maldroid.cob",
    "bgobjects/n64/manmech.cob",
    "bgobjects/n64/ramqan.cob",
    "bgobjects/n64/shieldTurret.cob",
]

# Per-limb .mx prefixes for the multi-component bosses. extract_kpf_dir()
# pulls everything under each prefix into the matching local dir, preserving
# subdirectory structure (the .cob files reference paths like
# n64/manmech/manmech_body.mx so the on-disk layout has to match).
N64_BOSS_MESH_DIRS = [
    "models/n64/dreadnaught/",
    "models/n64/maldroid/",
    "models/n64/manmech/",
    "models/n64/ramqan/",
    "models/n64/shieldTurret/",
]


def _copy_kpf_files(kpf_path, paths, dst_dirs_relative_to, dst_root_label):
    """Copy a list of KPF entries into local dirs, preserving structure
    relative to dst_dirs_relative_to (e.g., 'models/' so models/n64/foo.mx
    lands under <dst_root>/n64/foo.mx)."""
    copied = []
    skipped_existing = []
    missing = []
    with zipfile.ZipFile(kpf_path) as z:
        names = set(z.namelist())
        for src in paths:
            if src not in names:
                missing.append(src)
                continue
            rel = src[len(dst_dirs_relative_to):]
            dst = os.path.join(dst_root_label, rel)
            if os.path.exists(dst):
                skipped_existing.append(rel)
                continue
            os.makedirs(os.path.dirname(dst) or ".", exist_ok=True)
            with z.open(src) as src_f, open(dst, "wb") as dst_f:
                shutil.copyfileobj(src_f, dst_f)
            copied.append(rel)
    return copied, skipped_existing, missing


def import_n64_models(kpf_path, models_output):
    """Extract N64 enemy/pickup/boss .mx files into models_output/n64/."""
    dst_root = os.path.join(models_output, "n64")
    if not os.path.isdir(models_output):
        print(f"ERROR: --models-output dir does not exist: {models_output}",
              file=sys.stderr)
        return False
    os.makedirs(dst_root, exist_ok=True)

    # Single-mesh files: directly into dst_root (n64/foo.mx).
    copied = []
    skipped_existing = []
    missing = []

    with zipfile.ZipFile(kpf_path) as z:
        names = set(z.namelist())
        for src in N64_SINGLE_MESH_FILES:
            basename = os.path.basename(src)
            dst = os.path.join(dst_root, basename)
            if os.path.exists(dst):
                skipped_existing.append(basename)
                continue
            if src not in names:
                missing.append(src)
                continue
            with z.open(src) as src_f, open(dst, "wb") as dst_f:
                shutil.copyfileobj(src_f, dst_f)
            copied.append(basename)

        # Multi-part boss meshes: preserve the per-boss subdir layout.
        # KEX .cob files reference n64/<boss>/<part>.mx so the engine's
        # PreInitModel will look for data\\models\\n64\\<boss>\\<part>.mx.
        for prefix in N64_BOSS_MESH_DIRS:
            for src in sorted(names):
                if not src.startswith(prefix) or src.endswith("/"):
                    continue
                rel = src[len("models/"):]
                dst = os.path.join(models_output, rel)
                if os.path.exists(dst):
                    skipped_existing.append(rel)
                    continue
                os.makedirs(os.path.dirname(dst) or ".", exist_ok=True)
                with z.open(src) as src_f, open(dst, "wb") as dst_f:
                    shutil.copyfileobj(src_f, dst_f)
                copied.append(rel)

    print(f"N64 models: copied {len(copied)} files -> {models_output}")
    if copied:
        for c in copied:
            print(f"    {c}")
    if skipped_existing:
        print(f"  Already present, left untouched: "
              f"{len(skipped_existing)} file(s)")
    if missing:
        print(f"  WARNING: not found in KPF: {', '.join(missing)}")
    return True


def import_n64_cobs(kpf_path, bgobjects_output):
    """Extract N64 boss .cob component files into bgobjects_output/n64/."""
    dst_root = os.path.join(bgobjects_output, "n64")
    if not os.path.isdir(bgobjects_output):
        print(f"ERROR: --bgobjects-output dir does not exist: {bgobjects_output}",
              file=sys.stderr)
        return False
    os.makedirs(dst_root, exist_ok=True)

    copied, skipped_existing, missing = _copy_kpf_files(
        kpf_path, N64_BOSS_COB_FILES, "bgobjects/", bgobjects_output)

    print(f"N64 .cob files: copied {len(copied)} -> {dst_root}")
    if copied:
        for c in copied:
            print(f"    {c}")
    if skipped_existing:
        print(f"  Already present, left untouched: {', '.join(skipped_existing)}")
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
    ap.add_argument("--models-output", default="Data/models",
                    help="Destination models directory (default: Data/models)")
    ap.add_argument("--bgobjects-output", default="Data/bgobjects",
                    help="Destination bgobjects directory for boss .cob files "
                         "(default: Data/bgobjects)")
    ap.add_argument("--skip-levels", action="store_true",
                    help="Don't import level data")
    ap.add_argument("--skip-music", action="store_true",
                    help="Don't convert OGG music tracks")
    ap.add_argument("--skip-models", action="store_true",
                    help="Don't import N64 enemy/pickup models")
    ap.add_argument("--skip-banners", action="store_true",
                    help="Don't (re-)import the 256x128 crate-menu banners")
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
    if not args.skip_models:
        ok = import_n64_models(args.kpf, args.models_output) and ok
        ok = import_n64_cobs(args.kpf, args.bgobjects_output) and ok
    if not args.skip_banners:
        ok = import_banners(args.kpf, args.levels_output) and ok

    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
