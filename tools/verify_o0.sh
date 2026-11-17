#!/usr/bin/env bash
# verify_o0.sh CFG OBJ... -- second-level proof for objects verify_identical.sh
# reports as DIFFERS. Recompiles each object's translation unit at -O0 in the
# base tree and the working tree (same command lines, taken from the
# verify_identical build logs) and compares them with linkage normalisation.
# At -O0 there is no scheduling or register-allocation heuristic, so equal
# -O0 objects mean the compiler front end produced the same program and an
# -O2 difference is the optimiser choosing differently over identical
# semantics (typically: a declaration now seen earlier in the TU, or more
# precise type/alignment information). Run verify_identical.sh first.
set -u
cfg=$1; shift
VERIFY_DIR=${VERIFY_DIR:-/tmp/forsaken-verify}
base_out=$(ls -td "$VERIFY_DIR"/out-*/ | head -1); base_out=${base_out%/}
base_tree=$VERIFY_DIR/tree-$(echo "${base_out##*/out-}" | sed "s/-f[0-9]*$//")
work_tree=$(cd "$(dirname "$0")/.." && pwd)
OD=${DEVKITARM:-/opt/devkitpro/devkitARM}/bin/arm-none-eabi-objdump; [ "$cfg" = host ] && OD=objdump
export PATH=${DEVKITARM:-/opt/devkitpro/devkitARM}/bin:$PATH
tmp=$(mktemp -d)
norm() { "$OD" -d -r -C "$1" | tail -n +4 | grep -v '^Disassembly of section' |
	sed -E ':a; s/\b([A-Za-z_][A-Za-z0-9_:~]*)\([^()]*\)( const)?/\1/g; ta' |
	perl -pe 's{_ZZ(\d+)(\w+)}{my($n,$r)=($1,$2); my $f=substr($r,0,$n); ($r=~/E(\d+)([A-Za-z_]\w*)$/ && length($2)==$1) ? "_ZZ${n}${f}E$1$2" : "_ZZ$n$r"}ge; s{\.(rodata|bss|data|text)\._ZL?(\d+)(\w+)}{".$1.".substr($3,0,$2)}ge'; }
fail=0
for o in "$@"; do
	for side in base work; do
		if [ $side = base ]; then tree=$base_tree log=$base_out/$cfg.log; else tree=$work_tree log=$VERIFY_DIR/work/$cfg.log; fi
		cmd=$(grep -E -- "-o [^ ]*/$o( |$)" "$log" | tail -1)
		if [ -z "$cmd" ] && [ $side = work ]; then
			# incremental work build compiled nothing: the base command differs
			# only by the tree path
			cmd=$(grep -E -- "-o [^ ]*/$o( |$)" "$base_out/$cfg.log" | tail -1 | sed "s#$base_tree#$work_tree#g")
		fi
		[ -z "$cmd" ] && { echo "no compile line for $o in $log"; fail=1; continue 2; }
		cmd=$(echo "$cmd" | sed -E "s/ -O[0-3s]\b/ -O0/g; s# -o [^ ]*/$o# -o $tmp/$side-$o#; s# -MMD -MP -MF [^ ]*##")
		(cd "$tree" && eval "$cmd") > "$tmp/$side-$o.log" 2>&1 || { echo "O0 compile failed ($side $o)"; fail=1; continue 2; }
	done
	if cmp -s <(norm "$tmp/base-$o") <(norm "$tmp/work-$o"); then echo "O0-IDENTICAL  $cfg $o"; else echo "O0-DIFFERS    $cfg $o"; fail=1; fi
done
rm -rf "$tmp"; exit $fail
