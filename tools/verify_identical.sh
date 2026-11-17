#!/usr/bin/env bash
# verify_identical.sh -- prove a source change is functionally identical.
#
#   tools/verify_identical.sh [BASE_REF]      (default BASE_REF=HEAD)
#
# Builds BASE_REF (in a cached git worktree) and the WORKING TREE in every
# shipped configuration, then compares every object file's instructions,
# relocations and non-debug data, and the linked binaries' loadable sections.
# A cleanup (dead code, include hygiene, header reorganisation, comment
# work) must come out IDENTICAL; anything that differs is listed by object so
# it can be explained or reverted. This is the proof standard for "no
# behaviour change" short of a hardware fidelity A/B.
#
# Two build-only normalisations make the comparison about code, not text:
#   -D__LINE__=0               line shifts (removed dead code, moved comments)
#                              do not change __LINE__ immediates
#   -fmacro-prefix-map=ROOT/=  __FILE__ does not embed the checkout path
#   -DNDEBUG (PC host only)    glibc's C++ assert() uses __builtin_LINE()
# Both go in through VERIFY_CFLAGS / ADD_CFLAGS, which only this script sets
# (VERIFY_CFLAGS is in Makefile.3ds FORBIDDEN_FLAGS: it can never ship).
#
# Configurations: 3DS {remaster,original}, and the PC host build
# (EDITION=remaster) as `host` (the PC Makefile's default DEBUG=1: -O0, and the
# only build compiling DEBUG_ON/DEBUG_COMP/DEBUG_VIEWPORT/DEBUG_SPOTFX_SOUND)
# and `host-release` (DEBUG=0: -O3). Env: JOBS (default nproc), VERIFY_DIR (default
# /tmp/forsaken-verify; base builds are cached per commit there), CONFIGS
# (subset, e.g. CONFIGS="3ds-remaster host"), INCREMENTAL=1 (reuse work objects).
set -u
cd "$(dirname "$0")/.."
ROOT=$PWD
BASE_REF=${1:-HEAD}
JOBS=${JOBS:-$(nproc)}
VERIFY_DIR=${VERIFY_DIR:-/tmp/forsaken-verify}
CONFIGS=${CONFIGS:-"3ds-remaster 3ds-original host host-release"}
OD3DS=${DEVKITARM:-/opt/devkitpro/devkitARM}/bin/arm-none-eabi-objdump
export DEVKITPRO=${DEVKITPRO:-/opt/devkitpro} DEVKITARM=${DEVKITARM:-/opt/devkitpro/devkitARM}

base_hash=$(git rev-parse --short=12 "$BASE_REF") || exit 2
BASE_TREE=$VERIFY_DIR/tree-$base_hash
# Bump FLAGS_REV whenever the verification flags below change: base builds are
# cached per (commit, flag revision), never compared across flag sets.
FLAGS_REV=2
BASE_OUT=$VERIFY_DIR/out-$base_hash-f$FLAGS_REV
WORK_OUT=$VERIFY_DIR/work
mkdir -p "$VERIFY_DIR"

if [ ! -d "$BASE_TREE" ]; then
	git worktree add -q --detach "$BASE_TREE" "$base_hash" || exit 2
fi

# build TREE OUTDIR CONFIG  -> 0 on success; objects in OUTDIR/CONFIG
build() {
	local tree=$1 out=$2 cfg=$3 log
	log=$out/$cfg.log
	mkdir -p "$out/$cfg"
	local vf="-D__LINE__=0 -fmacro-prefix-map=$tree/="
	# Work builds start clean unless INCREMENTAL=1: a stale object compares
	# equal to the base and hides exactly the change being checked. (Base
	# builds are always fresh: one directory per commit and flag revision.)
	if [ "$out" = "$WORK_OUT" ] && [ "${INCREMENTAL:-0}" != 1 ]; then
		rm -rf "${out:?}/${cfg:?}"; mkdir -p "$out/$cfg"
	fi
	case $cfg in
	3ds-*)
		local ed=${cfg#3ds-}
		(cd "$tree" && make -f Makefile.3ds EDITION="$ed" BUILD="$out/$cfg" \
			VERIFY_CFLAGS="$vf" "$tree/forsaken3ds.elf" -j"$JOBS") > "$log" 2>&1 \
			&& cp "$tree/forsaken3ds.elf" "$out/$cfg/linked.elf"
		;;
	host|host-release)
		local dbg=; [ "$cfg" = host-release ] && dbg="DEBUG=0"
		# The PC Makefile has no flag stamp: never reuse host objects.
		rm -rf "${out:?}/$cfg"; mkdir -p "$out/$cfg"
		# -DNDEBUG: in C++ glibc's assert() passes __builtin_LINE(), which
		# -D__LINE__=0 cannot pin; assert compiles away identically instead.
		(cd "$tree" && make EDITION=remaster $dbg BUILD_PC="$out/$cfg" BIN="$out/$cfg/linked.elf" \
			ADD_CFLAGS="$vf -DNDEBUG" "$out/$cfg/linked.elf" -j"$JOBS") > "$log" 2>&1
		;;
	esac
}

# disasm OBJDUMP FILE -> instructions + relocations, one line each.
# NORMALIZE_LINKAGE=1 demangles and drops C++ parameter lists (and maps
# mangled names inside section names, incl. file-static _ZL), so moving a
# declaration into an extern "C" block (symbol _Z3foov -> foo) compares equal
# while any instruction or operand change still differs. Off by default; a
# change that needs it must say so in its commit message.
disasm() {
	if [ "${NORMALIZE_LINKAGE:-0}" = 1 ]; then
		# Section names embed mangled names under -ffunction-sections; drop
		# the headers (functions still compare in emission order).
		"$1" -d -r -C "$2" | tail -n +4 | grep -v '^Disassembly of section' |
			perl -pe 's/\b([A-Za-z_][\w:~]*)(\((?:[^()]++|(?2))*\))( const)?/$1/g' |
			sed -E 's/ \[clone (\.[a-z_]+\.[0-9]+)\]/\1/g; s/_GLOBAL__sub_I_[A-Za-z0-9_]+/_GLOBAL__sub_I_/g' |
			sed -E 's/\(anonymous namespace\):://g; s/\.(bss|data|rodata)\._ZN12_GLOBAL__N_1L?[0-9]+([A-Za-z_][A-Za-z0-9_]*)E/\2/g' |
			perl -pe 's{_ZZ(\d+)(\w+)}{my($n,$r)=($1,$2); my $f=substr($r,0,$n); ($r=~/E(\d+)([A-Za-z_]\w*)$/ && length($2)==$1) ? "_ZZ${n}${f}E$1$2" : "_ZZ$n$r"}ge; s{\.(rodata|bss|data|text)\._ZL?(\d+)(\w+)}{".$1.".substr($3,0,$2)}ge'
	else
		"$1" -d -r "$2" | tail -n +4
	fi
}

# data OBJDUMP FILE -> hex dump of every loadable non-code section, in order.
# Code (.text*) is covered by disasm, which prints its bytes too.
data() {
	local secs
	secs=$("$1" -h "$2" | awk '$2 ~ /^\./ && $2 !~ /^\.text|debug|comment|ARM.attributes|note|gnu_debug/ {print "-j " $2}')
	[ -z "$secs" ] && return 0
	if [ "${NORMALIZE_LINKAGE:-0}" = 1 ]; then
		"$1" -s $secs "$2" 2>/dev/null | tail -n +4 | grep -v '^Contents of section'
	else
		"$1" -s $secs "$2" 2>/dev/null | tail -n +4
	fi
}

# lost_overrides NM BASE_ELF WORK_ELF -> names strongly defined in the base
# binary that are only weak (or gone) in the work binary: a library's weak
# default won. This is how a C-named override (e.g. libctru's
# __system_allocateHeaps) dies when its definition gets a mangled C++ name --
# it compiles, links, and every per-function comparison under demangled
# names still matches. Always checked, whatever else differs.
lost_overrides() {
	python3 - "$1" "$2" "$3" <<'PYEOF'
import re, subprocess, sys
def syms(nm, elf):
    out = {}
    for l in subprocess.run([nm, "-C", elf], capture_output=True, text=True).stdout.splitlines():
        m = re.match(r"^(?:[0-9a-f]+)?\s+([A-Za-z])\s+(.+)$", l)
        if not m: continue
        name = re.sub(r"\(.*$", "", m.group(2)).strip()  # demangled base name
        t = m.group(1)
        if out.get(name) in ("T", "D", "B", "R"): continue  # keep the strongest
        out[name] = t
    return out
a, b = syms(sys.argv[1], sys.argv[2]), syms(sys.argv[1], sys.argv[3])
STRONG, WEAK = ("T", "D", "B", "R"), ("W", "w", "V", "v")
weak = [f"{n}({t}->{b[n]})" for n, t in sorted(a.items()) if t in STRONG and b.get(n) in WEAK]
gone = [f"{n}({t}->{b.get(n, 'MISSING')})" for n, t in sorted(a.items())
        if t in STRONG and b.get(n, "MISSING") not in STRONG + WEAK]
print(" ".join(weak) + "|" + " ".join(gone))
PYEOF
}

# cmp_obj OBJDUMP A B -> 0 if code+relocs+non-debug data match
cmp_obj() {
	cmp -s <(disasm "$1" "$2") <(disasm "$1" "$3") && cmp -s <(data "$1" "$2") <(data "$1" "$3")
}

fail=0
for cfg in $CONFIGS; do
	od=$OD3DS; [ "${cfg#host}" != "$cfg" ] && od=objdump
	if [ ! -f "$BASE_OUT/$cfg/linked.elf" ]; then
		build "$BASE_TREE" "$BASE_OUT" "$cfg" || { echo "BASE BUILD FAILED $cfg ($BASE_OUT/$cfg.log)"; fail=1; continue; }
	fi
	build "$ROOT" "$WORK_OUT" "$cfg" || { echo "WORK BUILD FAILED $cfg ($WORK_OUT/$cfg.log)"; fail=1; continue; }
	a=$BASE_OUT/$cfg b=$WORK_OUT/$cfg
	only_a=$(comm -23 <(cd "$a" && ls *.o) <(cd "$b" && ls *.o) | tr '\n' ' ')
	only_b=$(comm -13 <(cd "$a" && ls *.o) <(cd "$b" && ls *.o) | tr '\n' ' ')
	diffs=
	for o in $(comm -12 <(cd "$a" && ls *.o) <(cd "$b" && ls *.o)); do
		cmp_obj "$od" "$a/$o" "$b/$o" || diffs="$diffs $o"
	done
	linked=same
	cmp_obj "$od" "$a/linked.elf" "$b/linked.elf" || linked=DIFF
	nm=${OD3DS%objdump}nm; [ "${cfg#host}" != "$cfg" ] && nm=nm
	ov=$(lost_overrides "$nm" "$a/linked.elf" "$b/linked.elf")
	if [ -n "${ov%%|*}" ]; then
		echo "LOST OVERRIDE $cfg: ${ov%%|*}  (a library weak default replaced ours)"
		fail=1
	fi
	if [ -n "${ov#*|}" ]; then
		echo "NOTE      $cfg: no longer a global symbol: ${ov#*|}"
	fi
	if [ -z "$only_a$only_b$diffs" ] && [ $linked = same ]; then
		echo "IDENTICAL  $cfg ($(ls "$b"/*.o | wc -l) objects + linked binary)"
	else
		echo "DIFFERS    $cfg  objects:${diffs:- none}  linked:$linked"
		[ -n "$only_a" ] && echo "           only in base: $only_a"
		[ -n "$only_b" ] && echo "           only in work: $only_b"
		fail=1
	fi
done
exit $fail
