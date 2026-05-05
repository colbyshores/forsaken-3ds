#!/usr/bin/env bash
# tools/gen_wall_aux.sh — generate per-wall detail aux t3x.
#
# Detail map (option 2: per-diffuse high-pass): blur the diffuse
# heavily, subtract from original, recenter to 0.5 grey, desaturate.
# Captures the source's microstructure as a signed modulation suitable
# for ADD_SIGNED in the wall TexEnv detail stage.
#
# Normal mapping was attempted on this branch but the visible benefit
# was below the noise floor for Forsaken's hand-painted atlases (which
# lack the high-frequency height cues luminance-derived normals need).
# Detail mapping survived; normal mapping was reverted.
#
# Usage:  tools/gen_wall_aux.sh <src_dir> <dst_t3x_dir>
#         tools/gen_wall_aux.sh Data/levels/vol2/textures \
#                               romfs/hd_textures/levels/vol2/textures

set -euo pipefail

SRC=${1:?source dir, e.g. Data/levels/vol2/textures}
DST=${2:?destination dir, e.g. romfs/hd_textures/levels/vol2/textures}

mkdir -p "$DST"
TMP=$(mktemp -d)
trap "rm -rf $TMP" EXIT

# Synthesize ONE universal "metal grit" tile and reuse for every wall
# texture. Forsaken's painted atlases are too smooth to high-pass
# meaningfully — the source has no rivets, scratches, or weave for the
# extractor to find, so high-pass output is sub-perceptible. A real
# synthesized grit pattern (UT3-era convention) gives uniform surface
# microtexture that reads as "metal that's been scuffed", not "wall
# texture I painted with smooth gradients".
#
# Three superimposed layers, all centred on grey 0.5:
#   1. Fine gaussian speckle  — dust / paint-grain
#   2. Diagonal motion-blurred noise — brushed-metal scratches
#   3. Low-frequency Perlin-style — uneven wear and patina
# Mean preserved by averaging the three layers, then blended with a
# shifted copy of itself for tileability.
echo "[gen-aux] synthesising universal metal-grit tile"
convert -size 256x256 xc:gray50 -attenuate 0.5 +noise gaussian \
	-blur 0x0.4 "$TMP/_grit.png"
convert -size 256x256 xc:gray50 -attenuate 1.5 +noise gaussian \
	-motion-blur 0x6+30 -level 35%,65% "$TMP/_scratches.png"
convert -size 64x64 xc:gray50 -attenuate 1 +noise gaussian \
	-resize 256x256 -blur 0x4 "$TMP/_wear.png"
convert "$TMP/_grit.png" "$TMP/_scratches.png" "$TMP/_wear.png" \
	-evaluate-sequence mean -colorspace Gray "$TMP/_combined.png"
# Tileability: roll by half, blend with original, roll back. Edges
# match seamlessly when the tile repeats.
convert "$TMP/_combined.png" -roll +128+128 \
	\( "$TMP/_combined.png" \) -compose blend \
	-define compose:args=50 -composite \
	-roll +128+128 "$TMP/_grit_tile.png"
# Pre-pack the universal tile once; copy per-texture below.
tex3ds -f auto-etc1 -q high -m gaussian "$TMP/_grit_tile.png" \
	-o "$TMP/_grit_d.t3x"

shopt -s nullglob
for src in "$SRC"/*.bmp "$SRC"/*.BMP "$SRC"/*.png "$SRC"/*.PNG; do
	[[ -e "$src" ]] || continue
	name=$(basename "$src")
	stem=${name%.*}
	cp "$TMP/_grit_d.t3x" "$DST/${stem}_d.t3x"
done

echo "[gen-aux] wrote detail maps to $DST"
ls -lh "$DST"/*_d.t3x 2>/dev/null | head -3 | awk '{print "  ",$NF,$5}'
echo "  ($(ls "$DST"/*_d.t3x 2>/dev/null | wc -l) files, all identical universal grit tile)"
