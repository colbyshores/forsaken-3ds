#!/usr/bin/env bash
# Forsaken 3DS performance gates -- one entry point.
#
# Runs the three ARM11 perf screens from the repo root, aggregates their exit
# codes, prints one summary line per script, and exits nonzero if any fail.
# The adoption-debt baselines (contracts/hot_findings_baseline.txt and
# contracts/r11_candidates_baseline.txt) are what make a clean tree pass; a NEW
# finding fails until it is fixed or deliberately blessed (burn-down only).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$REPO_ROOT"

overall=0

run_gate() {
	local label="$1"; shift
	local log
	log="$(mktemp)"
	local code=0
	if python3 "$@" >"$log" 2>&1; then
		code=0
	else
		code=$?
	fi
	# One representative line from the script's own output.
	local summary
	summary="$(grep -m1 -E '^(PASS|FAIL)|VERDICT:|baselined:' "$log" | head -1 || true)"
	[ -z "$summary" ] && summary="$(head -1 "$log")"
	if [ "$code" -eq 0 ]; then
		printf 'PASS  %-18s exit=%d  %s\n' "$label" "$code" "$summary"
	else
		printf 'FAIL  %-18s exit=%d  %s\n' "$label" "$code" "$summary"
		overall=1
	fi
	rm -f "$log"
}

run_gate "gates_vfp"        tools/perf/gates_vfp.py
run_gate "audit_arm11_math" tools/perf/audit_arm11_math.py
run_gate "audit_r11_idiv"   tools/perf/audit_r11_idiv.py

if [ "$overall" -eq 0 ]; then
	echo "ALL GATES PASS"
else
	echo "ONE OR MORE GATES FAILED"
fi
exit "$overall"
