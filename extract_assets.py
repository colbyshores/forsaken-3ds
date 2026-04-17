#!/usr/bin/env python3
"""
extract_assets.py - Extract Forsaken (1998) game assets from disc image

Extracts game data and CD audio from a Forsaken ISO or BIN/CUE disc image
and sets up the romfs/ directory for building the 3DS port.

Usage:
    python3 extract_assets.py <image_file> [--output romfs]

    image_file:  Path to .iso, .bin (with matching .cue), or .cue file

Examples:
    python3 extract_assets.py "Forsaken (USA).iso"
    python3 extract_assets.py "Forsaken (USA).bin"
    python3 extract_assets.py "Forsaken (USA).cue"

Requirements:
    - Python 3.6+
    - 7z      (p7zip-full)   - for ISO data extraction
    - bchunk  (bchunk)       - for BIN/CUE splitting (only if using BIN/CUE)
    - ffmpeg  (ffmpeg)       - for CD audio conversion

Install on Debian/Ubuntu:
    sudo apt install p7zip-full bchunk ffmpeg
"""

import argparse
import glob
import os
import re
import shutil
import subprocess
import sys
import tempfile


# ── CD audio track info ──────────────────────────────────────────────
# Track 01 is always the data track.  Tracks 02-10 are audio.
# We convert audio tracks to 16-bit signed LE PCM, mono, 32 kHz WAV
# for playback via ndsp on 3DS.

AUDIO_TRACKS = {
    2:  "Forsaken",
    3:  "Gargantuan",
    4:  "Sanctuary of Tloloc",
    5:  "Volcano",
    6:  "Reactor",
    7:  "Pure Bitch Power",
    8:  "Condemned",
    9:  "Flameout",
    10: "The Dead System",
}

# Game data directories to copy from the data track
GAME_DATA_DIRS = ["Data", "Configs", "Pilots", "Scripts", "Demos"]


def check_tool(name):
    """Check if an external tool is available on PATH."""
    if shutil.which(name) is None:
        print(f"ERROR: '{name}' not found. Install it first.", file=sys.stderr)
        print(f"  Debian/Ubuntu: sudo apt install {name}", file=sys.stderr)
        return False
    return True


def run(cmd, **kwargs):
    """Run a command, printing it first. Returns CompletedProcess."""
    if isinstance(cmd, list):
        display = " ".join(cmd)
    else:
        display = cmd
    print(f"  $ {display}")
    return subprocess.run(cmd, check=True, **kwargs)


def find_cue_for_bin(bin_path):
    """Given a .bin file, find the matching .cue file."""
    base = os.path.splitext(bin_path)[0]
    for ext in [".cue", ".CUE", ".Cue"]:
        cue = base + ext
        if os.path.isfile(cue):
            return cue
    # Try same directory
    d = os.path.dirname(bin_path) or "."
    for f in os.listdir(d):
        if f.lower().endswith(".cue"):
            candidate = os.path.join(d, f)
            # Check if the CUE references this BIN
            with open(candidate, "r", errors="replace") as fh:
                content = fh.read()
                bin_name = os.path.basename(bin_path)
                if bin_name in content or bin_name.upper() in content:
                    return candidate
    return None


def find_bin_for_cue(cue_path):
    """Given a .cue file, extract the BIN filename from it."""
    with open(cue_path, "r", errors="replace") as f:
        for line in f:
            m = re.match(r'^\s*FILE\s+"?([^"]+)"?\s+BINARY', line, re.IGNORECASE)
            if m:
                bin_name = m.group(1)
                # Resolve relative to the CUE's directory
                cue_dir = os.path.dirname(cue_path) or "."
                bin_path = os.path.join(cue_dir, bin_name)
                if os.path.isfile(bin_path):
                    return bin_path
                # Try case-insensitive match
                for entry in os.listdir(cue_dir):
                    if entry.lower() == bin_name.lower():
                        return os.path.join(cue_dir, entry)
    return None


def extract_iso(iso_path, dest_dir):
    """Extract game data from an ISO (or data track ISO) using 7z."""
    print(f"\n── Extracting game data from {os.path.basename(iso_path)} ──")
    run(["7z", "x", "-y", f"-o{dest_dir}", iso_path],
        stdout=subprocess.DEVNULL)


def split_bincue(bin_path, cue_path, work_dir):
    """
    Split a BIN/CUE into individual tracks using bchunk.
    Returns (data_iso_path, list_of_audio_bin_paths).
    """
    print(f"\n── Splitting BIN/CUE with bchunk ──")
    basename = os.path.join(work_dir, "track")

    # -w produces WAV audio tracks directly
    # Without -w, audio tracks are raw PCM .cdr files
    # We use raw mode (no -w) so we can control the conversion ourselves
    run(["bchunk", bin_path, cue_path, basename])

    # bchunk names output: track01.iso, track02.cdr, track03.cdr, ...
    data_iso = None
    audio_files = []

    for f in sorted(os.listdir(work_dir)):
        full = os.path.join(work_dir, f)
        if f.startswith("track") and f.endswith(".iso"):
            data_iso = full
        elif f.startswith("track") and (f.endswith(".cdr") or f.endswith(".wav")):
            # Extract track number
            m = re.match(r"track(\d+)\.", f)
            if m:
                track_num = int(m.group(1))
                if track_num >= 2:  # skip data track
                    audio_files.append((track_num, full))

    return data_iso, audio_files


def convert_audio_track(input_path, output_path, input_format="cdr"):
    """
    Convert a raw CD audio track to 16-bit signed LE PCM, mono, 32 kHz WAV.

    CD audio is always 44100 Hz, 16-bit signed LE, stereo (2 channels).
    bchunk .cdr files are raw PCM (no header).
    """
    # Raw CD audio: 16-bit signed LE, 44100 Hz, stereo
    cmd = [
        "ffmpeg", "-y",
        "-f", "s16le", "-ar", "44100", "-ac", "2",
        "-i", input_path,
        "-ar", "32000", "-ac", "1",
        output_path,
    ]
    run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def convert_wav_audio_track(input_path, output_path):
    """
    Convert a WAV CD audio track to 16-bit signed LE PCM, mono, 32 kHz WAV.
    Used when bchunk is run with -w flag or when input is already WAV.
    """
    cmd = [
        "ffmpeg", "-y",
        "-i", input_path,
        "-ar", "32000", "-ac", "1",
        "-acodec", "pcm_s16le",
        output_path,
    ]
    run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def lowercase_tree(path):
    """Recursively lowercase all file and directory names."""
    # Process depth-first so child renames don't break parent paths
    for root, dirs, files in os.walk(path, topdown=False):
        for name in files + dirs:
            lower = name.lower()
            if name != lower:
                src = os.path.join(root, name)
                dst = os.path.join(root, lower)
                if not os.path.exists(dst):
                    os.rename(src, dst)


def setup_romfs(extracted_dir, output_dir):
    """
    Copy game data directories from extracted ISO contents into romfs/.
    Handles case-insensitive matching of directory names.
    """
    print(f"\n── Setting up romfs directory ──")
    os.makedirs(output_dir, exist_ok=True)

    # Build a case-insensitive map of what's in the extracted directory
    if not os.path.isdir(extracted_dir):
        print(f"ERROR: Extracted directory not found: {extracted_dir}",
              file=sys.stderr)
        return False

    entries = {}
    for entry in os.listdir(extracted_dir):
        entries[entry.lower()] = entry

    found_any = False
    for dirname in GAME_DATA_DIRS:
        key = dirname.lower()
        if key in entries:
            src = os.path.join(extracted_dir, entries[key])
            dst = os.path.join(output_dir, key)
            if os.path.isdir(src):
                print(f"  Copying {entries[key]}/ -> {dst}/")
                if os.path.exists(dst):
                    shutil.rmtree(dst)
                shutil.copytree(src, dst)
                found_any = True
        else:
            print(f"  Warning: '{dirname}' not found in extracted data")

    if not found_any:
        # Maybe the files are nested one level deep (common with some ISOs)
        for sub in os.listdir(extracted_dir):
            subpath = os.path.join(extracted_dir, sub)
            if os.path.isdir(subpath):
                sub_entries = {e.lower(): e for e in os.listdir(subpath)}
                if "data" in sub_entries or "configs" in sub_entries:
                    print(f"  Found game data inside '{sub}/', retrying...")
                    return setup_romfs(subpath, output_dir)

        print("ERROR: Could not find game data directories in extracted image.",
              file=sys.stderr)
        return False

    # Lowercase everything
    print("  Lowercasing all filenames...")
    lowercase_tree(output_dir)

    return True


def setup_music(audio_files, output_dir):
    """Convert and copy audio tracks to romfs/music/."""
    print(f"\n── Converting CD audio tracks ──")
    music_dir = os.path.join(output_dir, "music")
    os.makedirs(music_dir, exist_ok=True)

    for track_num, audio_path in sorted(audio_files):
        if track_num not in AUDIO_TRACKS:
            continue

        out_path = os.path.join(music_dir, f"track{track_num:02d}.wav")
        name = AUDIO_TRACKS[track_num]
        print(f"  Track {track_num:02d}: \"{name}\"")

        if audio_path.lower().endswith(".wav"):
            convert_wav_audio_track(audio_path, out_path)
        else:
            # Raw PCM (.cdr) from bchunk
            convert_audio_track(audio_path, out_path)

    converted = len(glob.glob(os.path.join(music_dir, "track*.wav")))
    print(f"  {converted} audio tracks converted to romfs/music/")
    return converted > 0


def extract_from_iso(iso_path, output_dir):
    """Full extraction pipeline for a standalone .iso file."""
    if not check_tool("7z") or not check_tool("ffmpeg"):
        return False

    with tempfile.TemporaryDirectory(prefix="forsaken_") as tmp:
        extract_dir = os.path.join(tmp, "iso_contents")
        os.makedirs(extract_dir)
        extract_iso(iso_path, extract_dir)

        if not setup_romfs(extract_dir, output_dir):
            return False

    # ISO-only: no CD audio tracks available
    print("\n  Note: ISO files do not contain CD audio tracks.")
    print("  To extract music, use the original BIN/CUE disc image instead.")
    print("  You can also manually place WAV files in romfs/music/track02.wav - track10.wav")
    return True


def extract_from_bincue(bin_path, cue_path, output_dir):
    """Full extraction pipeline for a BIN/CUE disc image."""
    if not check_tool("7z") or not check_tool("bchunk") or not check_tool("ffmpeg"):
        return False

    with tempfile.TemporaryDirectory(prefix="forsaken_") as tmp:
        # Step 1: Split BIN/CUE into tracks
        data_iso, audio_files = split_bincue(bin_path, cue_path, tmp)

        if not data_iso:
            print("ERROR: bchunk did not produce a data track (.iso)",
                  file=sys.stderr)
            return False

        # Step 2: Extract game data from the data track
        extract_dir = os.path.join(tmp, "iso_contents")
        os.makedirs(extract_dir)
        extract_iso(data_iso, extract_dir)

        if not setup_romfs(extract_dir, output_dir):
            return False

        # Step 3: Convert audio tracks
        if audio_files:
            setup_music(audio_files, output_dir)
        else:
            print("\n  Warning: No audio tracks found in BIN/CUE.")

    return True


def main():
    parser = argparse.ArgumentParser(
        description="Extract Forsaken (1998) game assets from a disc image.",
        epilog="Supports .iso, .bin/.cue formats. "
               "Outputs to romfs/ for the 3DS port build system.",
    )
    parser.add_argument(
        "image",
        help="Path to .iso, .bin, or .cue file",
    )
    parser.add_argument(
        "--output", "-o",
        default="romfs",
        help="Output directory (default: romfs/)",
    )
    parser.add_argument(
        "--music-only",
        action="store_true",
        help="Only extract and convert CD audio tracks (requires BIN/CUE)",
    )

    args = parser.parse_args()
    image_path = os.path.abspath(args.image)
    output_dir = os.path.abspath(args.output)

    if not os.path.isfile(image_path):
        print(f"ERROR: File not found: {image_path}", file=sys.stderr)
        sys.exit(1)

    ext = os.path.splitext(image_path)[1].lower()

    print("=" * 60)
    print("  Forsaken Asset Extractor")
    print("=" * 60)
    print(f"  Input:  {image_path}")
    print(f"  Output: {output_dir}")

    # ── Determine image type and resolve companion files ──

    if ext == ".iso":
        if args.music_only:
            print("ERROR: --music-only requires a BIN/CUE image.", file=sys.stderr)
            sys.exit(1)
        ok = extract_from_iso(image_path, output_dir)

    elif ext == ".bin":
        cue_path = find_cue_for_bin(image_path)
        if not cue_path:
            print(f"ERROR: Cannot find .cue file for {image_path}", file=sys.stderr)
            print("  Place the .cue file next to the .bin file.", file=sys.stderr)
            sys.exit(1)
        print(f"  CUE:    {cue_path}")

        if args.music_only:
            if not check_tool("bchunk") or not check_tool("ffmpeg"):
                sys.exit(1)
            with tempfile.TemporaryDirectory(prefix="forsaken_") as tmp:
                _, audio_files = split_bincue(image_path, cue_path, tmp)
                ok = setup_music(audio_files, output_dir) if audio_files else False
        else:
            ok = extract_from_bincue(image_path, cue_path, output_dir)

    elif ext == ".cue":
        bin_path = find_bin_for_cue(image_path)
        if not bin_path:
            print(f"ERROR: Cannot find .bin file referenced by {image_path}",
                  file=sys.stderr)
            sys.exit(1)
        print(f"  BIN:    {bin_path}")

        if args.music_only:
            if not check_tool("bchunk") or not check_tool("ffmpeg"):
                sys.exit(1)
            with tempfile.TemporaryDirectory(prefix="forsaken_") as tmp:
                _, audio_files = split_bincue(bin_path, image_path, tmp)
                ok = setup_music(audio_files, output_dir) if audio_files else False
        else:
            ok = extract_from_bincue(bin_path, image_path, output_dir)

    else:
        print(f"ERROR: Unsupported file type: {ext}", file=sys.stderr)
        print("  Supported: .iso, .bin, .cue", file=sys.stderr)
        sys.exit(1)

    # ── Summary ──

    print()
    print("=" * 60)
    if ok:
        print("  Extraction complete!")
        print()
        if not args.music_only:
            print("  Your romfs/ directory is ready. Next steps:")
            print()
            print("    # Clean romfs staging (forces rebuild with new assets)")
            print("    make -f Makefile.3ds romfs-clean")
            print()
            print("    # Build the 3DS binary")
            print("    make -f Makefile.3ds RENDERER=citro3d")
        else:
            print("  Music tracks extracted to romfs/music/")
        print()

        # Show what was extracted
        if os.path.isdir(output_dir):
            print("  Contents of", output_dir + "/")
            for entry in sorted(os.listdir(output_dir)):
                full = os.path.join(output_dir, entry)
                if os.path.isdir(full):
                    count = sum(1 for _ in os.walk(full)
                                for __ in _[2])
                    print(f"    {entry + '/':20s} ({count} files)")
    else:
        print("  Extraction failed. Check the errors above.")
        sys.exit(1)

    print("=" * 60)


if __name__ == "__main__":
    main()
