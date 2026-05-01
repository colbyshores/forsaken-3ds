#!/usr/bin/env python3
"""
extract_remaster_levels.py - Build a complete EDITION=remaster Data/ tree
from Forsaken Remastered's ForsakenEX.kpf

Designed to fully replace extract_assets.py for a Remaster build — the user
does NOT need the 1998 ISO. The Remaster KPF is a complete superset of the
1998 game data: every .mx model, .cob component file, .bsp/.mxv level binary,
.bmp/.png texture, and sound sample is byte-for-byte identical to the 1998
ISO contents (verified empirically against the original disc image). The
Remaster also adds 28 new levels (SP additions, N64-secret levels, MP arenas),
9 new music compositions, N64 enemy/pickup .mx meshes, and N64 boss .cob
component files.

The only 1998-runtime data NOT shipped in the KPF is engine-required runtime
config: ~50 .off sprite-offset files used for fonts/effects, the engine's
enemies.txt and statsmessages.txt tuning files, and the projectx-32x32.bmp
splash icon. KEX dropped these because its renderer doesn't use them. We
ship them with the port itself in assets/engine_runtime_1998/ — about 110 KB,
treated as part of the engine port, not as game content.

Pipeline (no 1998 ISO required):

    python3 extract_remaster_levels.py <ForsakenEX.kpf>

    → Populates Data/{Levels,models,bgobjects,textures,sound,splash,Demos}
      from the KPF, downscaled crate-menu banners (256x128) per level,
      Data/{offsets,txt} + Data/projectx-32x32.bmp from the port repo,
      and 9 new music tracks at romfs/music/track11..19.dsp.

If you'd rather build from the 1998 ISO, use extract_assets.py against the
disc image instead. The two pipelines are mutually exclusive — pick one.

Compatibility note: the EDITION=remaster build's per-level enemy spawns
(.nme files) reference 8 N64 enemy types (IDs 100-108) and 6 N64 pickup
types (IDs 100-105) that the original 1998 game doesn't define. The engine
template-copies a tonally-similar 1998 enemy/pickup at runtime to fill those
slots and overrides ModelFilename to point at the real Remaster mesh —
behaviourally approximate, visually correct.

Usage:
    python3 extract_remaster_levels.py <ForsakenEX.kpf> \\
        [--steam-dir /path/to/Forsaken\\ Remastered] \\
        [--levels-output Data/Levels] \\
        [--models-output Data/models] \\
        [--bgobjects-output Data/bgobjects] \\
        [--data-output Data] \\
        [--music-output romfs/music] \\
        [--skip-music] [--skip-levels] [--skip-models] \\
        [--skip-banners] [--skip-bulk] [--skip-runtime]

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
        # Lowercase the output path so this pipeline produces the same
        # tree shape as extract_assets.py's lowercase_tree() pass on the
        # 1998 ISO. The engine lowercases at fopen time anyway.
        out_path = os.path.join(dst_dir, rel.lower())
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
    # Create the output dir if missing — keeps the script self-bootstrapping
    # when running against an empty Data/ tree. The bulk extract usually
    # creates this already, but the per-level overlay can run standalone too.
    os.makedirs(output_dir, exist_ok=True)

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
            # Output dir is always lowercase to align with the bulk
            # extractor and extract_assets.py's lowercase_tree() pass.
            dst = os.path.join(output_dir, actual.lower())
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
        # Banners are level-specific overwrites — if the dir doesn't
        # exist there's nothing to write. Skip without erroring; this
        # happens when the user passes --skip-bulk --skip-levels and
        # also has no pre-existing Data/Levels.
        print(f"  (no levels at {output_dir}, skipping banners)")
        return True

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
    lands under <dst_root>/n64/foo.mx). Output paths are lowercased to
    match the bulk extractor + extract_assets.py's lowercase_tree() pass."""
    copied = []
    skipped_existing = []
    missing = []
    with zipfile.ZipFile(kpf_path) as z:
        names = set(z.namelist())
        for src in paths:
            if src not in names:
                missing.append(src)
                continue
            rel = src[len(dst_dirs_relative_to):].lower()
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
            basename = os.path.basename(src).lower()
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
        # Output paths lowercased to match the bulk extractor; the engine's
        # convert_path() lowercases at fopen time so the .cob's mixed-case
        # internal references resolve correctly against the lowercased disk.
        for prefix in N64_BOSS_MESH_DIRS:
            for src in sorted(names):
                if not src.startswith(prefix) or src.endswith("/"):
                    continue
                rel = src[len("models/"):].lower()
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


# ----- Bulk KPF asset extraction (full Data/ tree from KPF) ------------------
#
# A full Remaster build doesn't need the 1998 ISO at all — the KPF is a
# complete superset of the original game data. This step walks the KPF's
# top-level dirs and lays them out under the local Data/ tree using 1998
# path conventions (e.g., KPF "sounds/" → "Data/sound/").
#
# Skipped: KEX-engine-only directories (defs/, particles/, sprites/, gfx/,
# progs/, trails/, fonts/, lensflares/, localization/, localized/) and
# per-locale subdirectories under levels/ and other paths.
#
# Run once per asset refresh; subsequent runs are idempotent (overwrites
# existing files in place — fine because content is byte-identical to the
# previous extract). To redo from scratch, rm -rf Data/ first.

# Map KPF top-level dir → local Data/ subdir. Anything not in this map
# is skipped (KEX-only assets the 1998 engine can't read anyway). Output
# names are lowercase to match extract_assets.py's lowercase_tree() output
# and the Makefile's romfs-staging expectations (BMP→PNG find pattern,
# EDITION=remaster mission.dat copy target both use lowercase paths).
KPF_DIR_MAP = {
    "levels":    "levels",
    "models":    "models",
    "bgobjects": "bgobjects",
    "textures":  "textures",
    "sounds":    "sound",     # name change: KPF plural → 1998 singular
    "splash":    "splash",
    "demos":     "demos",
}

# Per-locale subdir prefixes inside levels/<level>/ that we always skip.
LOCALE_SUBDIRS = ("de", "es", "fr", "it", "jp", "ru")


def _is_locale_path(rel_path):
    """True if a path is under a localized subdirectory we should skip."""
    parts = rel_path.split("/")
    return any(p in LOCALE_SUBDIRS for p in parts[:-1])


def import_kpf_assets(kpf_path, data_output):
    """Bulk-extract every asset directory the 1998 engine reads from the
    KPF into data_output. Replaces what extract_assets.py would copy from
    the 1998 ISO (Data/{models,bgobjects,levels,textures,sound,splash,
    Demos}). Idempotent — re-running overwrites files in place.

    All output paths are lowercased to match extract_assets.py's
    lowercase_tree() pass — the engine's convert_path() lowercases at
    fopen time so the on-disk casing isn't load-bearing, but matching
    the 1998 pipeline's shape keeps both pipelines interchangeable."""
    if not os.path.isdir(data_output):
        os.makedirs(data_output, exist_ok=True)

    written = 0
    skipped_locale = 0
    skipped_other_dir = set()

    with zipfile.ZipFile(kpf_path) as z:
        for info in z.infolist():
            if info.is_dir():
                continue
            parts = info.filename.split("/", 1)
            if len(parts) < 2:
                continue
            top = parts[0]
            rest = parts[1]
            if top not in KPF_DIR_MAP:
                skipped_other_dir.add(top)
                continue
            if _is_locale_path(rest):
                skipped_locale += 1
                continue
            # Skip per-level loading-screen .png at level root —
            # import_banners() handles those (downscales to 256x128).
            if top == "levels":
                level_parts = rest.split("/")
                if (len(level_parts) == 2
                        and level_parts[1].lower().endswith(".png")):
                    continue
            # All paths lowercased to match extract_assets.py's
            # lowercase_tree() output. The Makefile's romfs-staging
            # BMP→PNG step and EDITION=remaster mission.dat copy
            # target both expect lowercase, so an uppercase Levels/
            # would (a) miss the find-pattern conversion and (b)
            # collide with the lowercase mission.dat copy at the
            # lowercase pass that follows, getting rm -rf'd.
            local_top = KPF_DIR_MAP[top]  # already lowercase per the map
            out_path = os.path.join(data_output, local_top, rest.lower())
            os.makedirs(os.path.dirname(out_path) or ".", exist_ok=True)
            with z.open(info) as src, open(out_path, "wb") as dst:
                shutil.copyfileobj(src, dst)
            written += 1

    print(f"KPF bulk extract: wrote {written} files -> {data_output}/")
    if skipped_locale:
        print(f"  Skipped {skipped_locale} per-locale entries (de/es/fr/it/jp/ru)")
    if skipped_other_dir:
        print(f"  Skipped KEX-only top-level dirs: "
              f"{', '.join(sorted(skipped_other_dir))}")
    return True


def import_engine_runtime_1998(repo_root, data_output):
    """Copy the 1998-engine-required runtime data from
    assets/engine_runtime_1998/ (committed to the port repo) into
    data_output.

    KEX's renderer doesn't use the .off sprite-offset files, the
    engine's per-gun tuning .txt, or the projectx-32x32.bmp splash
    icon, so the KPF doesn't ship them (~110 KB). We treat these as
    part of the engine port.

    Anything under assets/engine_runtime_1998/ ends up at the
    matching path under data_output. Idempotent."""
    src = os.path.join(repo_root, "assets", "engine_runtime_1998")
    if not os.path.isdir(src):
        print(f"WARNING: {src} missing — engine-runtime files not staged. "
              f"Build will fail to load fonts / .off effects.",
              file=sys.stderr)
        return False
    if not os.path.isdir(data_output):
        os.makedirs(data_output, exist_ok=True)

    written = 0
    for root, _dirs, files in os.walk(src):
        rel = os.path.relpath(root, src)
        out_dir = data_output if rel == "." else os.path.join(data_output, rel)
        os.makedirs(out_dir, exist_ok=True)
        for f in files:
            # copy() not copy2() — copy2 tries to preserve xattrs which
            # fails on some filesystems (NAS-mounted cifs/nfs).
            shutil.copy(os.path.join(root, f), os.path.join(out_dir, f))
            written += 1
    print(f"Engine runtime (1998-required): wrote {written} files -> "
          f"{data_output}/ (offsets, txt, projectx-32x32.bmp)")
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


# ----- visibility-table repair (Night-Dive levels) ---------------------------

def repair_visibility_tables(data_output):
    """Scan every extracted .mxv under data_output/levels/ and rebuild the
    Connected/Visible/IndirectVisibleGroup tables on any level whose
    IndirectVisibleGroup section is zeroed out.

    KEX's level cooker omits the IndirectVisibleGroup section (writes zeros)
    for levels it authored from scratch — defend2, stableizers, powerdown,
    starship, battlebase, plus most other Night-Dive-only maps. Without the
    repair, the 1998 engine reads those zeros and two systems break:

      - BuildVisibleLightList -> VisibleOverlap returns 0 for every non-
        immediate-neighbour group pair, so dynamic blaster XLights are
        filtered out -> walls don't get colored light.
      - FindVisible's per-portal walk terminates one hop too shallow ->
        through-portal rooms render as black voids until the player flies
        across the portal.

    The repair runs BFS over the file's preserved per-portal VISTREE edges
    (1 hop = ConnectedGroup, 2 hops = VisibleGroup, 4 hops =
    IndirectVisibleGroup) and writes the rebuilt tables back into the .mxv.
    Approximation of the original 1998 cooker's geometry-based PVS — over-
    includes some groups (harmless: at worst one extra dynamic light passes
    the filter), never under-includes (the visible bug).

    1998 originals and the 9 KPF-preserved 1998 SP-campaign levels (asubchb,
    bio-sphere, fedbankv, military, nps-sp01, nukerf, pship, space, thermal)
    have IndirectVisibleGroup populated correctly and skip the repair. The
    diagnostic gate is the .mxv's own IndirectVisibleGroup byte total — no
    edition flag needed."""
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    try:
        from mxv_visi_repair import parse_mxv, repair_mxv
        import struct as _s
    except ImportError as e:
        print(f"WARNING: mxv_visi_repair unavailable ({e}); "
              f"runtime visi.c repair will handle it.", file=sys.stderr)
        return True

    levels_dir = os.path.join(data_output, "levels")
    if not os.path.isdir(levels_dir):
        return True

    repaired = []
    skipped = 0
    failed = []
    for lvl in sorted(os.listdir(levels_dir)):
        lvl_path = os.path.join(levels_dir, lvl)
        if not os.path.isdir(lvl_path):
            continue
        mxv_files = [f for f in os.listdir(lvl_path) if f.lower().endswith('.mxv')]
        if not mxv_files:
            continue
        mxv_path = os.path.join(lvl_path, mxv_files[0])
        try:
            with open(mxv_path, 'rb') as f:
                data = f.read()
            parsed = parse_mxv(data)
            # Repair if EITHER the IndirectVisibleGroup table is empty
            # (KEX never wrote it) OR the per-portal VISTREE is flat
            # (KEX wrote zero children on every portal). Both are
            # KEX-authored markers; 1998 cooker output sets both.
            # Walk the vistree section: scan each portal's root
            # num_visible field. If any > 0, the file already has a
            # populated VISTREE.
            r_off = parsed['vistree'][0]
            any_portal_has_children = False
            for g in range(parsed['num_groups']):
                for _p in range(parsed['groups'][g]['num_portals']):
                    r_off += 2  # visible_group
                    nv = _s.unpack_from('<H', data, r_off)[0]; r_off += 2
                    if nv > 0:
                        any_portal_has_children = True
                    # Skip over children tree without parsing — sized in bytes
                    # by the existing recursive structure. Simpler: parse it.
                    for _ in range(nv):
                        # Each child node: u16+u16+u16 + recursive children.
                        # Walk via the existing helper.
                        from mxv_visi_repair import walk_vistree as _walk
                        r_off, _ = _walk(data, r_off)
            _, list_start, _ = parsed['indirect']
            o = list_start
            indir_total = 0
            for _g in range(parsed['num_groups']):
                cnt = _s.unpack_from('<H', data, o)[0]
                o += 2 + cnt * 2
                indir_total += cnt
            needs_repair = (indir_total == 0) or (not any_portal_has_children)
            if not needs_repair:
                skipped += 1
                continue
            new_data = repair_mxv(data)
            with open(mxv_path, 'wb') as f:
                f.write(new_data)
            repaired.append(lvl)
        except Exception as e:
            failed.append((lvl, str(e)))

    print(f"Visibility-table repair: rebuilt {len(repaired)} levels, "
          f"left {skipped} healthy levels alone")
    if repaired:
        print(f"  Repaired: {', '.join(repaired)}")
    if failed:
        print(f"  WARNING: failed on {len(failed)}: "
              f"{', '.join(f'{n} ({e})' for n, e in failed)}")
    return True


# ----- entry point -----------------------------------------------------------

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("kpf", help="Path to ForsakenEX.kpf from Forsaken Remastered")
    ap.add_argument("--steam-dir", default=None,
                    help="Forsaken Remastered Steam install dir "
                         "(default: directory containing the KPF)")
    ap.add_argument("--levels-output", default="Data/levels",
                    help="Destination level directory (default: Data/levels)")
    ap.add_argument("--music-output", default="romfs/music",
                    help="Destination music directory (default: romfs/music)")
    ap.add_argument("--models-output", default="Data/models",
                    help="Destination models directory (default: Data/models)")
    ap.add_argument("--bgobjects-output", default="Data/bgobjects",
                    help="Destination bgobjects directory for boss .cob files "
                         "(default: Data/bgobjects)")
    ap.add_argument("--data-output", default="Data",
                    help="Destination Data/ root for bulk asset extraction "
                         "(default: Data). The bulk pass populates "
                         "Data/{Levels,models,bgobjects,textures,sound,"
                         "splash,Demos} from the KPF and Data/{offsets,txt}+"
                         "Data/projectx-32x32.bmp from "
                         "assets/engine_runtime_1998/.")
    ap.add_argument("--skip-levels", action="store_true",
                    help="Don't import the per-level overlay (NEW_LEVELS list)")
    ap.add_argument("--skip-music", action="store_true",
                    help="Don't convert OGG music tracks")
    ap.add_argument("--skip-models", action="store_true",
                    help="Don't import N64 enemy/pickup/boss meshes + .cobs")
    ap.add_argument("--skip-banners", action="store_true",
                    help="Don't (re-)import the 256x128 crate-menu banners")
    ap.add_argument("--skip-bulk", action="store_true",
                    help="Don't bulk-extract the full Data/ tree from the KPF "
                         "(use if you've already populated Data/ via "
                         "extract_assets.py and just want Remaster overlays)")
    ap.add_argument("--skip-runtime", action="store_true",
                    help="Don't copy assets/engine_runtime_1998/ into Data/. "
                         "Implies you've populated those bits some other way.")
    ap.add_argument("--skip-visi-repair", action="store_true",
                    help="Don't rebuild Connected/Visible/IndirectVisibleGroup "
                         "tables on .mxv files with zeroed IndirectVisibleGroup. "
                         "Use only if you want the runtime visi.c rebuild to "
                         "handle them instead.")
    args = ap.parse_args()

    if not os.path.isfile(args.kpf):
        print(f"ERROR: KPF not found: {args.kpf}", file=sys.stderr)
        return 1

    steam_dir = args.steam_dir or os.path.dirname(os.path.abspath(args.kpf))
    repo_root = os.path.dirname(os.path.abspath(__file__))

    ok = True
    # Bulk first — lays down the full Data/ tree from the KPF.
    if not args.skip_bulk:
        ok = import_kpf_assets(args.kpf, args.data_output) and ok
    if not args.skip_runtime:
        ok = import_engine_runtime_1998(repo_root, args.data_output) and ok
    # Per-feature passes layered on top (idempotent — overwrite specific
    # files inside the bulk-extracted tree). Banners need import_levels to
    # have created the per-level dirs, so order matters.
    if not args.skip_levels:
        ok = import_levels(args.kpf, args.levels_output) and ok
    if not args.skip_music:
        ok = import_music(steam_dir, args.music_output) and ok
    if not args.skip_models:
        ok = import_n64_models(args.kpf, args.models_output) and ok
        ok = import_n64_cobs(args.kpf, args.bgobjects_output) and ok
    if not args.skip_banners:
        ok = import_banners(args.kpf, args.levels_output) and ok
    # Visi-table repair must run last — after every pass that could touch a
    # .mxv. Operates in-place on the final on-disk files.
    if not args.skip_visi_repair:
        ok = repair_visibility_tables(args.data_output) and ok

    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
