#!/bin/bash
# Regenerate HD textures from the 4K Forsaken Remastered texture pack.
#
# Source priority (per texture):
#   1. $K4_PACK/<path>.bmp           — 4K Forsaken Remastered pack (preferred)
#   2. Data/Levels/<lvl>/textures/   — original per-level BMP/PNG (correct atlas)
#   3. Data/textures/<name>.png      — global original (fallback)
#
# Output: romfs/hd_old/{textures,levels/<lvl>/textures}/<name>.t3x
# Each t3x is downscaled to fit within 512x512 preserving aspect.
#
# Setup — the 4K pack is hosted on Moddb behind Cloudflare so it cannot be
# scripted-downloaded. Get it once, manually:
#
#   1. Open https://www.moddb.com/mods/upscaled-forsaken-textures/addons/4k-texture-pack
#   2. Click "Download Now". Save the .rar (~7.5 GB) anywhere convenient —
#      this script auto-detects all the common locations below.
#
# Detected paths (in order):
#   $FORSAKEN_4K_PACK   (env var override, points at the .rar OR an
#                        already-extracted dir)
#   ./4ktexturepack.rar
#   ./assets/4ktexturepack.rar
#   ~/Downloads/4ktexturepack.rar
#   /tmp/4k_pack/                           (already-extracted cache)
#
# If found as a .rar, the script extracts it into /tmp/4k_pack/ once; the
# extracted ~7.5 GB tree is cached there for subsequent runs.
set -e
cd "$(dirname "$0")"

export MAGICK_THREAD_LIMIT=1
export OMP_NUM_THREADS=1

K4_PACK=/tmp/4k_pack
OUT=romfs/hd_old
TMP=/tmp/hd_regen_$$
mkdir -p "$OUT/textures" "$OUT/levels" "$TMP"

# ─── locate / extract the 4K pack ──────────────────────────────────────
ensure_4k_pack() {
    # Already extracted at $K4_PACK with the expected layout? Done.
    if [ -d "$K4_PACK/textures" ] && [ -d "$K4_PACK/levels" ]; then
        return 0
    fi

    # Search likely locations for the .rar
    local cand candidates=(
        "$FORSAKEN_4K_PACK"
        "$(pwd)/4ktexturepack.rar"
        "$(pwd)/assets/4ktexturepack.rar"
        "$HOME/Downloads/4ktexturepack.rar"
    )
    local rar=""
    for cand in "${candidates[@]}"; do
        [ -n "$cand" ] && [ -f "$cand" ] && { rar="$cand"; break; }
    done

    if [ -z "$rar" ]; then
        cat <<EOF >&2

  ⚠  4K texture pack not found.

  This pipeline upscales Forsaken's textures from the community 4K pack.
  It's hosted on ModDB behind Cloudflare's bot challenge, so we can't fetch
  it automatically — please download once, manually:

      https://www.moddb.com/mods/upscaled-forsaken-textures/addons/4k-texture-pack

  Then either drop the .rar in any of these locations:

      ./4ktexturepack.rar
      ./assets/4ktexturepack.rar
      ~/Downloads/4ktexturepack.rar

  …or set FORSAKEN_4K_PACK=/path/to/4ktexturepack.rar  before re-running.

  (~7.5 GB; only fetched once — subsequent runs reuse /tmp/4k_pack/.)

EOF
        exit 1
    fi

    if ! command -v unrar >/dev/null 2>&1; then
        echo "Need 'unrar' to extract $rar (sudo apt install unrar)." >&2
        exit 1
    fi
    if ! command -v unzip >/dev/null 2>&1; then
        echo "Need 'unzip' to crack the .kpf archives inside the pack." >&2
        exit 1
    fi

    echo "Extracting 4K pack from $rar (~7.5 GB; one-time, ~2 min on SSD)…"
    rm -rf "$K4_PACK"
    mkdir -p "$K4_PACK"
    unrar x -o+ "$rar" "$K4_PACK/" >/dev/null
    # The .rar contains mods/4kN.kpf files; each .kpf is a zip archive of
    # textures/ + levels/<lvl>/textures/. Unpack them all into $K4_PACK so
    # the resolve_source() lookups below find the BMPs by predictable path.
    for kpf in "$K4_PACK"/mods/*.kpf; do
        [ -f "$kpf" ] || continue
        unzip -oq "$kpf" -d "$K4_PACK"
    done
    echo "4K pack ready at $K4_PACK ($(du -sh "$K4_PACK" | cut -f1))"
}

ensure_4k_pack

PARALLEL_JOBS=14
MAX_DIM=512   # Old 3DS hd_old budget; New 3DS would use 1024
TASKSET_CPUS=0-13  # leaves cores 14,15 free for OS/desktop

# Convert one source image to t3x.
# Args: src dst
convert_one() {
    local src="$1"
    local dst="$2"
    local tmp_png="$TMP/$(basename "$dst" .t3x)_$RANDOM.png"

    # Get original dimensions, calculate POT target preserving aspect.
    local sw=$(identify -format '%w' "$src" 2>/dev/null)
    local sh=$(identify -format '%h' "$src" 2>/dev/null)
    [ -z "$sw" ] && { echo "FAIL identify: $src" >&2; return 1; }

    # Round each axis DOWN to nearest power of 2, clamped to MAX_DIM.
    # This preserves the aspect ratio class (square stays square, 2:1 stays 2:1).
    local tw=$MAX_DIM th=$MAX_DIM
    # Find largest POT <= MAX_DIM for each axis based on aspect
    if [ "$sw" -ge "$sh" ]; then
        local ratio=$(( sw / sh ))
        if [ "$ratio" -ge 8 ]; then th=$(( MAX_DIM / 8 ));
        elif [ "$ratio" -ge 4 ]; then th=$(( MAX_DIM / 4 ));
        elif [ "$ratio" -ge 2 ]; then th=$(( MAX_DIM / 2 ));
        else th=$MAX_DIM; fi
        tw=$MAX_DIM
    else
        local ratio=$(( sh / sw ))
        if [ "$ratio" -ge 8 ]; then tw=$(( MAX_DIM / 8 ));
        elif [ "$ratio" -ge 4 ]; then tw=$(( MAX_DIM / 4 ));
        elif [ "$ratio" -ge 2 ]; then tw=$(( MAX_DIM / 2 ));
        else tw=$MAX_DIM; fi
        th=$MAX_DIM
    fi

    # Resize, then VERTICAL FLIP — required because the original create_texture
    # path's tile_rgba4() stores textures Y-flipped (h-1-y) for PICA200 native
    # orientation. Tex3DS_TextureImportStdio loads PNGs as-is with no flip,
    # so we have to bake the Y-flip into the source. Without this, after the
    # PICA200's 90° screen rotation the textures render mirrored horizontally.
    #
    # Texture classification:
    #   sprite    → small/effect atlas (pkups, fire, etc.) → ETC1A4, no mips
    #   wall      → big level surface (>=256², per-level, not sprite) → see below
    #   menu page → m-tpage* → ETC1 (no alpha — solid black needed), no mips
    #   global    → other Data/textures/* → ETC1A4, no mips
    #
    # Walls split further based on whether SOURCE has any pure-black pixels:
    #   wall with no black pixels   → ETC1 + mips (e.g. plain corridor walls;
    #                                  no alpha, no moiré, fast)
    #   wall WITH black pixels      → ETC1A4, NO mips (e.g. lava floors with
    #                                  cutouts showing lava through gaps;
    #                                  alpha+mip = black holes at distance,
    #                                  so we accept some moiré instead)
    local base=$(basename "$dst" .t3x)
    local is_sprite=0
    case "$base" in
        pkups|decals|sprites|weapons|effects|hud|sbcrys|crys|fire|flame|smoke|lazer*|blast*|explode*|pship|ships*)
            is_sprite=1 ;;
    esac
    local is_wall=0
    if [[ "$dst" == */levels/* ]] && [ "$tw" -ge 256 ] && [ "$th" -ge 256 ] && [ "$is_sprite" = 0 ]; then
        is_wall=1
    fi

    # Detect alpha walls via the ORIGINAL Forsaken BMP's palette (not the
    # AI-upscaled version). The original BMPs are 8-bit palettized; entries
    # at pure (0,0,0) indicate engine-recognised colorkey-transparent areas
    # (grates, lava cutouts). AI upscaling adds stray pure-black pixels to
    # solid walls that don't actually need alpha, so the upscale is unreliable
    # as a signal — but the original palette is ground truth.
    is_wall_opaque=0
    is_wall_alpha=0
    if [ "$is_wall" = 1 ]; then
        # Parse level + name from dst path (.../levels/<level>/textures/<name>.t3x)
        local lvl
        lvl=$(echo "$dst" | sed -nE 's|.*/levels/([^/]+)/textures/.*|\1|p')
        local orig_bmp
        orig_bmp=$(find Data/Levels -ipath "*/${lvl}/textures/${base}.bmp" 2>/dev/null | head -1)
        local has_pure_black=0
        if [ -n "$orig_bmp" ] && [ -f "$orig_bmp" ]; then
            local n
            n=$(convert "$orig_bmp" -unique-colors txt: 2>/dev/null | grep -c "(0,0,0)")
            [ "${n:-0}" -gt 0 ] && has_pure_black=1
        fi
        if [ "$has_pure_black" = 1 ]; then
            is_wall_alpha=1
        else
            is_wall_opaque=1
        fi
    fi

    # ─── ANTIPATTERN (necessary workaround) ─────────────────────────────
    # Hardcoded mipmap blacklist for textures that produce visible block-
    # edge artifacts at smaller mip levels.
    #
    # Why this is an antipattern: it's content-aware logic in a build script,
    # which means new levels / new texture packs may need this list updated
    # by hand. The proper fix would be either (a) per-texture quality
    # validation (encode + compare to bilinear-downsampled source, taint if
    # RMSE too high), or (b) a per-texture metadata file shipped with the
    # asset pack saying "no mips please."
    #
    # Why we ship it anyway: we tried automated detection (count pure-black
    # pixels to identify alpha textures) and AI upscaling produced enough
    # noise that nearly every wall got mis-classified, stripping mipmaps
    # everywhere. A name list is at least deterministic. If a user's texture
    # pack has a different naming convention, this list won't catch their
    # animated walls and they'll see artifacts — that's the cost.
    #
    # Names below cover Forsaken's animated POLYANIM'd surfaces:
    #   therm[a-d], thermal, adstherm[a-d], adsherm[a-d]  → lava floors
    #   nuke[a-d], nukerf                                 → radioactive walls
    #   lava*, fire*, water*, magma*, acid*, plasma*, flow*  → generic patterns
    local skip_mips=0
    case "$base" in
        therm[a-d]|thermal|adstherm[a-d]|adsherm[a-d]|nuke[a-d]|nukerf|lava*|fire*|water*|magma*|acid*|plasma*|flow*)
            skip_mips=1 ;;
    esac

    if [ "$is_wall_opaque" = 1 ] || [[ "$base" == m-tpage* ]]; then
        convert "$src" -resize "${tw}x${th}!" -flip -alpha off "PNG24:$tmp_png" 2>/dev/null
    else
        convert "$src" -resize "${tw}x${th}!" -flip -alpha set -transparent "rgb(0,0,0)" "PNG32:$tmp_png" 2>/dev/null
    fi

    # Walls: force -f etc1 (no alpha plane). Mipmap unless name-blacklisted
    # (animated/polyanim'd textures look bad downsampled).
    if [ "$is_wall_opaque" = 1 ]; then
        if [ "$skip_mips" = 1 ]; then
            tex3ds -f etc1 -q high "$tmp_png" -o "$dst" 2>/dev/null
        else
            tex3ds -f etc1 -q high -m gaussian "$tmp_png" -o "$dst" 2>/dev/null
        fi
    else
        tex3ds -f auto-etc1 -q high "$tmp_png" -o "$dst" 2>/dev/null
    fi
    rm -f "$tmp_png"
}
export -f convert_one
export TMP MAX_DIM

# Build job list.
JOB_FILE="$TMP/jobs.txt"
> "$JOB_FILE"

resolve_source() {
    local kind=$1   # "global" or "level"
    local level=$2  # for level kind, the lowercase level name
    local name=$3   # texture base name (lowercase)

    # GLOBAL textures: prefer originals — Night Dive 4K pack rearranged the
    # menu/UI atlas (m-tpage*.bmp), so its layout does NOT match the engine's
    # authored UVs (verified: 19-30% RMSE on menu pages, vs 2-5% on per-level).
    if [ "$kind" = "global" ]; then
        [ -f "Data/textures/${name}.png" ] && { echo "Data/textures/${name}.png"; return; }
        [ -f "Data/textures/${name}.bmp" ] && { echo "Data/textures/${name}.bmp"; return; }
        # 4K fallback only if original missing
        local found=$(find "$K4_PACK/textures" -maxdepth 1 -type f -iname "${name}.bmp" -o -iname "${name}.png" 2>/dev/null | head -1)
        [ -n "$found" ] && { echo "$found"; return; }
        return 1
    fi

    # LEVEL textures: per-level 4K layouts DO match original atlases (Night Dive
    # preserved level texture layouts), so prefer 4K for higher quality.
    local k4_level=$(find "$K4_PACK/levels" -maxdepth 1 -type d -iname "$level" 2>/dev/null | head -1)
    if [ -n "$k4_level" ]; then
        local found=$(find "$k4_level/textures" -maxdepth 1 -type f -iname "${name}.bmp" -o -iname "${name}.png" 2>/dev/null | head -1)
        [ -n "$found" ] && { echo "$found"; return; }
    fi

    # Fallback: original per-level BMP/PNG
    local found=$(find Data/Levels -ipath "*/${level}/textures/${name}.bmp" -o \
                                   -ipath "*/${level}/textures/${name}.png" 2>/dev/null | head -1)
    [ -n "$found" ] && { echo "$found"; return; }

    # Fallback: original global (engine itself falls back here, so atlas matches)
    [ -f "Data/textures/${name}.png" ] && { echo "Data/textures/${name}.png"; return; }
    [ -f "Data/textures/${name}.bmp" ] && { echo "Data/textures/${name}.bmp"; return; }

    # Last resort: any per-level variant of this name
    found=$(find Data/Levels -iname "${name}.bmp" -o -iname "${name}.png" 2>/dev/null | head -1)
    [ -n "$found" ] && { echo "$found"; return; }

    return 1
}

# Global textures (every texture in Data/textures/, prefer original for atlas)
for png in Data/textures/*.png; do
    [ -f "$png" ] || continue
    name=$(basename "$png" .png | tr 'A-Z' 'a-z')
    src=$(resolve_source global "" "$name")
    [ -n "$src" ] && echo "$src $OUT/textures/${name}.t3x" >> "$JOB_FILE"
done

# Per-level textures: gather every (level, name) pair the engine could request.
# Sources of truth, unioned:
#   1. Data/Levels/<lvl>/textures/*.{bmp,png}  — local per-level files
#   2. /tmp/4k_pack/levels/<lvl>/textures/*.bmp  — 4K pack per-level files
#   3. Old broken hd_old/levels/<lvl>/textures/*.t3x — engine path expectations
#      (it referenced these paths even when no per-level BMP existed locally)
{
    find Data/Levels -mindepth 3 -type f \( -iname '*.bmp' -o -iname '*.png' \) 2>/dev/null
    find /tmp/4k_pack/levels -mindepth 3 -type f \( -iname '*.bmp' -o -iname '*.png' \) 2>/dev/null
    find /tmp/backup_hd_old_squashed_*/levels -mindepth 3 -type f -name '*.t3x' 2>/dev/null
} | awk -F/ '{
    # Find the index of the "levels" or "Levels" dir, level name follows it
    for (i=1; i<=NF; i++) if (tolower($i) == "levels") { lvl=tolower($(i+1)); break }
    n=split($NF, parts, ".");
    name=tolower(parts[1]);
    for (i=2; i<n; i++) name = name "." tolower(parts[i]);
    print lvl "/" name
}' | sort -u > "$TMP/level_textures.txt"

while IFS=/ read level name; do
    out="$OUT/levels/${level}/textures/${name}.t3x"
    mkdir -p "$(dirname "$out")"
    src=$(resolve_source level "$level" "$name")
    [ -n "$src" ] && echo "$src $out" >> "$JOB_FILE"
done < "$TMP/level_textures.txt"
sort -u "$JOB_FILE" -o "$JOB_FILE"

NUM=$(wc -l < "$JOB_FILE")
NUM_4K=$(awk '{print $1}' "$JOB_FILE" | grep -c "^$K4_PACK" || true)
echo "Regenerating $NUM HD textures with $PARALLEL_JOBS parallel jobs (taskset 0-7)"
echo "Sources: $NUM_4K from 4K pack, $((NUM - NUM_4K)) from originals"

cat "$JOB_FILE" | taskset -c $TASKSET_CPUS xargs -L1 -P$PARALLEL_JOBS bash -c 'convert_one "$0" "$1"'

echo "Done."
echo "Output: $(find $OUT -name '*.t3x' | wc -l) t3x files, $(du -sh $OUT | cut -f1)"
rm -rf "$TMP"
