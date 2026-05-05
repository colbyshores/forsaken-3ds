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

shopt -s nullglob
for src in "$SRC"/*.bmp "$SRC"/*.BMP "$SRC"/*.png "$SRC"/*.PNG; do
	[[ -e "$src" ]] || continue
	name=$(basename "$src")
	stem=${name%.*}

	echo "[gen-aux] $stem"

	# High-pass via -compose mathematics so negatives don't clamp:
	#   result = 0.5 + 0.5 * (original - blurred)
	# Args 0,0.5,-0.5,0.5 evaluate to a*0 + b*0.5 + a*(-0.5) + 0.5
	# = 0.5 + 0.5*(b - a) where a = blurred (clone), b = original.
	# Output is signed-symmetric around grey 0.5: high-frequency
	# features above the local average produce >0.5 (brighten via
	# ADD_SIGNED), below produce <0.5 (darken). A naive `-compose
	# minus -evaluate add 50%` clamps the negative half to 0 first,
	# producing a detail map biased entirely upward — every pixel
	# only ever brightens, washing out walls.
	convert "$src" \
		\( +clone -blur 0x8 \) \
		-compose mathematics \
		-define compose:args="0,0.5,-0.5,0.5" \
		-composite \
		-colorspace Gray \
		"$TMP/${stem}_d.png"

	tex3ds -f auto-etc1 -q high -m gaussian "$TMP/${stem}_d.png" -o "$DST/${stem}_d.t3x"
done

echo "[gen-aux] wrote detail maps to $DST"
ls -lh "$DST"/*_d.t3x 2>/dev/null | awk '{print "  ",$NF,$5}'
