#!/usr/bin/env bash
# Build banner_test.cia: a hello-world app (UniqueId 0xff4c0, title
# "Banner Test") carrying the animated banner from assets/banner.cgfx, for
# testing the banner on hardware without replacing the Forsaken install.
# See banner/SPEC.md section 9.
set -euo pipefail
cd "$(dirname "$0")"
REPO=$(cd ../.. && pwd)
BT=$REPO/tools/bannertool
MR=$REPO/tools/makerom
OUT=build_cia
mkdir -p $OUT romfs
touch romfs/.keep
nice -n 19 make -j8 >/dev/null
$BT makesmdh -s "Banner Test" -l "Forsaken animated banner test" -p "ForsakenX" \
	-i "$REPO/assets/icon.png" -f "visible,allow3d,recordusage,extendedbanner" \
	-o $OUT/banner_test.smdh >/dev/null
$BT makebanner -ci "$REPO/assets/banner.cgfx" -a "$REPO/assets/banner.wav" -o $OUT/banner_test.bnr >/dev/null
$MR -f cia -o $OUT/banner_test.cia -rsf banner_test.rsf -target t -exefslogo \
	-elf test_app.elf -icon $OUT/banner_test.smdh -banner $OUT/banner_test.bnr \
	-DAPP_ROMFS="$PWD/romfs"
ls -la $OUT/banner_test.cia
