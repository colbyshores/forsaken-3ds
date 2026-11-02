#!/usr/bin/env bash
# emu_sweep.sh — run the Forsaken 3DSX under Mandarine and collect the guest
# PERF artifact. This is the EMULATOR replacement for the netload_stub capture
# loop (tools/perf/ralph_loop.sh), used while the hardware console is
# unreliable.
#
# Why the emulator and not the console: the hardware capture path needs a
# working netload_stub push (3dslink) and a hand-launched console, and the
# renderer-rewrite phase was blocked by console freezes. Under Mandarine the
# guest writes sdmc:/forsaken_perf.txt into the emulated SD card, which is a
# plain directory on this host — so the artifact is a file read, not a network
# fetch, and the whole capture is unattended.
#
# ⚠ AN EMULATOR NUMBER IS NOT A HARDWARE NUMBER. Mandarine runs the guest CPU
# at host speed, has a different linear heap (~5 MB vs 22-24 MB on hardware),
# lenient texture clamping and different f24 precision. Emulator timings are
# DIRECTIONAL evidence for ranking and regression-spotting inside this loop.
# They never clear an AGENTS.md P6 hardware claim (see
# docs/validation/capture-protocol.md §6).
#
# Multi-tenant safety (vault note multi-tenant-emulator-harness-isolate-by-display):
#   - The isolation key is the X DISPLAY, read from /proc/PID/environ. We launch
#     on a private Xvfb and only ever kill processes whose DISPLAY is ours.
#   - A co-tenant emulator on another display is NEVER touched, and its FUSE
#     mount is never unmounted.
#   - Per-display flock: two runs of ours on one display would collect each
#     other (the victim looks like an emulator crash).
#   - Private XDG_CONFIG_HOME: the emulator config (resolution, stereo, audio)
#     is shared and rewritten on every graceful exit.
#   - Private XDG_DATA_HOME: the emulated SD card, so the artifact cannot be
#     confused with another run's, and so this harness needs no write access
#     to the operator's real SD.
#
# Exit codes: 0 ok, 2 setup/launch failed, 3 missing or STALE artifact,
#             4 emulator died before producing an artifact, 5 timed out.

set -u

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO_ROOT"

DSX="${FORSAKEN_DSX:-$REPO_ROOT/forsaken3ds.3dsx}"
DISPLAY_NUM="${EMU_DISPLAY:-131}"
TIMEOUT="${EMU_TIMEOUT:-900}"
APPIMAGE="${MANDARINE:-/mnt/nas/Development/mandarine.AppImage}"
OUT="${EMU_OUT:-}"
HOME_DIR="${EMU_HOME:-$REPO_ROOT/build_3ds/emu-home}"
# Logs live under the emulated home (inside the repo, group-writable), NOT in
# /tmp: the loop runs as piagent while a smoke test may run as the operator, and
# a root-sticky /tmp file created by one user cannot be opened by the other.
STDOUT_LOG="${EMU_STDOUT_LOG:-$HOME_DIR/emu_stdout.log}"
SD_ROOT="$HOME_DIR/mandarine-emu/sdmc"
ARTIFACT="$SD_ROOT/forsaken_perf.txt"
TRACE="$SD_ROOT/forsaken_trace.txt"
BOOTLOG="$SD_ROOT/forsaken_boot.log"
CFGHOME="${EMU_CFG:-$HOME_DIR/config}"
ROMFS_SRC="${EMU_ROMFS:-$REPO_ROOT/forsaken.romfs}"
KEEP_EMU="${EMU_KEEP:-0}"

say() { echo "[$(date -u +%H:%M:%S)] emu_sweep: $*"; }

while [ $# -gt 0 ]; do
	case "$1" in
		--dsx)     DSX="$2"; shift 2 ;;
		--display) DISPLAY_NUM="$2"; shift 2 ;;
		--timeout) TIMEOUT="$2"; shift 2 ;;
		--out)     OUT="$2"; shift 2 ;;
		--home)    HOME_DIR="$2"; shift 2 ;;
		--keep)    KEEP_EMU=1; shift ;;
		-h|--help) sed -n '2,30p' "$0"; exit 0 ;;
		*) echo "emu_sweep: unknown argument: $1" >&2; exit 2 ;;
	esac
done

[ -f "$DSX" ] || { say "no such build: $DSX"; exit 2; }
[ -x "$APPIMAGE" ] || { say "no mandarine AppImage at $APPIMAGE"; exit 2; }

# ---- (1) one run per display ------------------------------------------------------------------
# The lock must be usable by BOTH the operator (coleshores) and the loop's
# builder (piagent): /tmp is sticky, so a lock file created by one user cannot
# be opened by the other. Create it world-writable; if that is impossible (the
# file already exists with restrictive modes), fall back to a per-uid lock.
_TMPD="${EMU_TMPDIR:-${TMPDIR:-/tmp}}"
_LOCK="$_TMPD/forsaken-emu-$DISPLAY_NUM.lock"
if ! ( umask 000; : > "$_LOCK" ) 2>/dev/null; then
	_LOCK="$_TMPD/forsaken-emu-$DISPLAY_NUM-$(id -u).lock"
	: > "$_LOCK" || { say "cannot create lock $_LOCK"; exit 2; }
fi
chmod 666 "$_LOCK" 2>/dev/null || true
exec 9>"$_LOCK"
if ! flock -n 9; then
	say "another Forsaken emulator run holds display :$DISPLAY_NUM (lock $_LOCK)"
	exit 2
fi

# ---- (2) only ever touch processes on OUR display -----------------------------------------------
emu_own_pids() {
	local p env
	for p in $(pgrep -f 'mandarine\.AppImage|AppRun\.wrapped' 2>/dev/null); do
		env=$( { tr '\0' '\n' < "/proc/$p/environ"; } 2>/dev/null || true )
		case "$env" in *"DISPLAY=:$DISPLAY_NUM"*) echo "$p" ;; esac
	done
	return 0
}

emu_mount_in_use() {
	local m="$1" p exe
	for p in $(pgrep -f 'mandarine\.AppImage|AppRun\.wrapped' 2>/dev/null); do
		exe=$(readlink -f "/proc/$p/exe" 2>/dev/null || true)
		case "$exe" in "$m"/*) return 0 ;; esac
	done
	return 1
}

emu_cleanup() {
	[ "$KEEP_EMU" = "1" ] && return 0
	local p
	for p in $(emu_own_pids); do kill -9 "$p" 2>/dev/null || true; done
	sleep 1
	# Enumerate from /proc/mounts, not a glob guarded by -d: a dead FUSE endpoint
	# makes stat() fail, so a glob survey skips exactly the mounts that need
	# reclaiming. A DEAD endpoint has no user by definition -> always safe. A
	# LIVE endpoint keeps the co-tenant guard.
	for m in $(awk '$2 ~ /mount_mandar/ {print $2}' /proc/mounts 2>/dev/null); do
		if timeout 3 stat "$m" >/dev/null 2>&1; then
			emu_mount_in_use "$m" && continue
		fi
		fusermount -uz "$m" 2>/dev/null || true
		rmdir "$m" 2>/dev/null || true
	done
	return 0
}
trap emu_cleanup EXIT

emu_cleanup
n=$(emu_own_pids | grep -c . || true); [ -n "$n" ] || n=0
[ "$n" -eq 0 ] || { say "$n of OUR mandarine procs survived cleanup; refusing to launch"; exit 2; }

# ---- (3) private offscreen display --------------------------------------------------------------
if ! DISPLAY=":$DISPLAY_NUM" xdpyinfo >/dev/null 2>&1; then
	nohup Xvfb ":$DISPLAY_NUM" -screen 0 1280x960x24 -nolisten tcp >"$_TMPD/xvfb$DISPLAY_NUM.log" 2>&1 &
	for _ in $(seq 1 15); do
		DISPLAY=":$DISPLAY_NUM" xdpyinfo >/dev/null 2>&1 && break
		sleep 1
	done
fi
DISPLAY=":$DISPLAY_NUM" xdpyinfo >/dev/null 2>&1 || { say "Xvfb :$DISPLAY_NUM did not come up"; exit 2; }

# ---- (4) private emulated SD + private config ---------------------------------------------------
mkdir -p "$SD_ROOT/3ds/forsaken" "$CFGHOME/mandarine-emu"
if [ ! -f "$SD_ROOT/3ds/forsaken/forsaken.romfs" ]; then
	[ -f "$ROMFS_SRC" ] || { say "no romfs at $ROMFS_SRC (build it with: make -f Makefile.3ds romfs)"; exit 2; }
	say "seeding emulated SD romfs (one time, ~298 MB)"
	cp "$ROMFS_SRC" "$SD_ROOT/3ds/forsaken/forsaken.romfs" || { say "romfs copy failed"; exit 2; }
fi
cp "$DSX" "$SD_ROOT/3ds/forsaken/forsaken3ds.3dsx" || { say "3dsx copy into emulated SD failed"; exit 2; }

_cfg="$CFGHOME/mandarine-emu/qt-config.ini"
if [ ! -f "$_cfg" ] && [ -f "$HOME/.config/mandarine-emu/qt-config.ini" ]; then
	cp "$HOME/.config/mandarine-emu/qt-config.ini" "$_cfg"
fi
set_cfg() {
	if grep -q "^$1=" "$_cfg" 2>/dev/null; then
		sed -i "s|^$1=.*|$1=$2|" "$_cfg"
	else
		printf '%s=%s\n' "$1" "$2" >> "$_cfg"
	fi
	# Citra writes a "<key>\default=true" companion and IGNORES the value when
	# it is true — the #1 reason "I set it and nothing happened".
	if grep -q "^$1\\\\default=" "$_cfg" 2>/dev/null; then
		sed -i "s|^$1\\\\default=.*|$1\\\\default=false|" "$_cfg"
	fi
}
set_cfg volume 0
set_cfg output_engine null
set_cfg output_type 0
set_cfg enable_realtime_audio false

# ---- (5) prove the artifact is absent before the run --------------------------------------------
PREV_MD5=""
[ -f "$ARTIFACT" ] && PREV_MD5=$(md5sum "$ARTIFACT" | cut -d' ' -f1)
rm -f "$ARTIFACT" "$TRACE" "$BOOTLOG"
[ -f "$ARTIFACT" ] && { say "could not remove $ARTIFACT; a stale artifact would be reported as fresh"; exit 3; }

# ---- (6) launch ----------------------------------------------------------------------------------
# dbus-run-session -- : Mandarine-family emulators abort on a dbus_connection_unref
# assertion when they run without a session bus (measured on another project on
# this box, same emulator family). Software GL keeps the user's GPU free.
LAUNCH_EPOCH=$(date +%s)
say "launching mandarine on :$DISPLAY_NUM (dsx=$(basename "$DSX"), timeout=${TIMEOUT}s)"
env DISPLAY=":$DISPLAY_NUM" \
	XDG_CONFIG_HOME="$CFGHOME" \
	XDG_DATA_HOME="$HOME_DIR" \
	LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
	SDL_AUDIODRIVER=dummy ALSA_CARD=null AUDIODEV=null \
	dbus-run-session -- "$APPIMAGE" "$SD_ROOT/3ds/forsaken/forsaken3ds.3dsx" \
	>"$STDOUT_LOG" 2>&1 &
EMUPID=$!

# ---- (7) wait for the guest to exit and flush the artifact ---------------------------------------
# The guest writes the PERF dump ONCE at teardown (perf_capture_3ds.c opens its
# own fd at sdmc:/forsaken_perf.txt). So the artifact appears only when the
# sweep ends and the game exits — wait for "PERF: dump end", not for the file.
T0=$(date +%s)
while :; do
	if [ -f "$ARTIFACT" ] && tail -1 "$ARTIFACT" 2>/dev/null | grep -q '^PERF: dump end$'; then
		break
	fi
	if ! kill -0 "$EMUPID" 2>/dev/null && [ ! -f "$ARTIFACT" ]; then
		say "emulator exited before producing an artifact (rc=$?)"
		tail -20 "$STDOUT_LOG" 2>/dev/null
		exit 4
	fi
	if [ $(( $(date +%s) - T0 )) -gt "$TIMEOUT" ]; then
		say "TIMEOUT after ${TIMEOUT}s waiting for the perf artifact"
		tail -20 "$BOOTLOG" 2>/dev/null
		exit 5
	fi
	sleep 2
done
sleep 1
emu_cleanup

NOW_MD5=$(md5sum "$ARTIFACT" | cut -d' ' -f1)
[ -n "$PREV_MD5" ] && [ "$NOW_MD5" = "$PREV_MD5" ] && { say "STALE artifact (md5 unchanged: $NOW_MD5)"; exit 3; }

if [ -n "$OUT" ]; then
	mkdir -p "$(dirname "$OUT")"
	cp "$ARTIFACT" "$OUT"
	cp "$TRACE" "$OUT.trace" 2>/dev/null || true
	cp "$BOOTLOG" "$OUT.boot" 2>/dev/null || true
fi
say "artifact OK: $ARTIFACT ($(wc -l < "$ARTIFACT") lines, md5=$NOW_MD5, run=$(( $(date +%s) - LAUNCH_EPOCH ))s)"
[ -n "$OUT" ] && say "copied to $OUT"
exit 0
