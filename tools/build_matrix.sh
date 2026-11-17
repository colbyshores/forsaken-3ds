#!/usr/bin/env bash
# Build-matrix smoke: every shipped configuration must compile and link.
#
# Why this exists: EDITION=original sat broken for six months (f42146e..d9be192)
# because every validation build used EDITION=remaster, and tests/Makefile sat
# broken since Cleanup 5 because nobody ran it. Config-gated rot is invisible
# unless every configuration is built on every check. Run this before pushing;
# the pre-push hook does. First run is slow (cold caches); rows use persistent
# per-config object dirs so later runs are incremental.
#
# Rows are compile+link only (3DS rows link forsaken3ds.elf and skip romfs
# staging/3dsx/CIA packaging; packaging is a release step, not a rot check).
set -u
cd "$(dirname "$0")/.."
JOBS=${JOBS:-$(nproc)}
LOGDIR=${LOGDIR:-/tmp/build-matrix}
BUILDDIR=${BUILDDIR:-$PWD/.build-matrix}
mkdir -p "$LOGDIR"
fail=0

run() {
	local name="$1"; shift
	printf '== %s ... ' "$name"
	if "$@" > "$LOGDIR/$name.log" 2>&1; then
		echo "PASS"
	else
		echo "FAIL (log: $LOGDIR/$name.log)"
		fail=1
	fi
}

run 3ds-original        make -f Makefile.3ds EDITION=original BUILD="$BUILDDIR/3ds-original" "$PWD/forsaken3ds.elf" -j"$JOBS"
run 3ds-remaster        make -f Makefile.3ds EDITION=remaster BUILD="$BUILDDIR/3ds-remaster" "$PWD/forsaken3ds.elf" -j"$JOBS"
run host-original       make EDITION=original -j"$JOBS"
run host-remaster       make EDITION=remaster -j"$JOBS"
run tests               make -C tests
run gates               tools/perf/check.sh

if [ "$fail" -eq 0 ]; then echo "BUILD MATRIX: all rows pass"; else echo "BUILD MATRIX: FAILURES"; fi
exit "$fail"
