#!/usr/bin/env bash
#
# build_banner_audio.sh — deterministically rebuild assets/banner.wav from
# the original 1990s Forsaken commercial on YouTube.
#
# Pipeline:
#   1. yt-dlp downloads the ad video, extracts to WAV
#   2. demucs (mdx_extra model, CPU) isolates vocals from music/SFX
#   3. ffmpeg trims to "The Future is Forsaken" line, normalises format
#   4. boosts gain, applies tiny fades, cuts a leading silent second,
#      then drops the level by 6 dB to taste
#
# Output: assets/banner.wav (mono PCM16 22050 Hz, ~2.5 s, ~ -14 dB mean)
#
# Reproduces the exact file that ships with the CIA. Re-run after any
# upstream change to the script or the source video.
set -euo pipefail
cd "$(dirname "$0")"

# ── tunables ───────────────────────────────────────────────────────────
SOURCE_URL="https://www.youtube.com/watch?v=xhlmGpsQNT0"
TRIM_START=33.0           # seconds into the ad where the line begins
TRIM_LENGTH=3.5           # raw clip length before head-trim
HEAD_TRIM=1.5             # seconds to drop from the start of the boosted clip
GAIN_BOOST_DB=18          # boost applied to the demucs vocals (they're quiet)
FINAL_TRIM_DB=-10         # final level cut for taste
FADE_DURATION=0.2         # fade-in / fade-out duration on the FINAL clip
DEMUCS_MODEL=mdx_extra    # gives noticeably cleaner voice than the default htdemucs

OUT=assets/banner.wav
WORK=$(mktemp -d /tmp/forsaken_banner.XXXXXX)
trap 'rm -rf "$WORK"' EXIT

# ── prerequisites ──────────────────────────────────────────────────────
need() { command -v "$1" >/dev/null 2>&1 || MISSING+=("$1"); }
MISSING=()
need yt-dlp
need ffmpeg
need demucs
if [ ${#MISSING[@]} -ne 0 ]; then
    echo "Missing tools: ${MISSING[*]}" >&2
    echo "Install with:" >&2
    echo "  pip install --user yt-dlp demucs" >&2
    echo "  apt install ffmpeg" >&2
    exit 1
fi

# ── 1. Download the ad's audio ────────────────────────────────────────
echo "[1/4] Downloading source audio…"
yt-dlp -q --no-warnings -x --audio-format wav -o "$WORK/ad.%(ext)s" "$SOURCE_URL"

# ── 2. Vocal isolation (CPU; HIP/AMD GPUs hit kernel mismatches) ──────
echo "[2/4] Running demucs vocal isolation ($DEMUCS_MODEL, CPU)…"
demucs --device cpu --two-stems=vocals -n "$DEMUCS_MODEL" -o "$WORK/sep" "$WORK/ad.wav" >/dev/null 2>&1
VOCALS="$WORK/sep/$DEMUCS_MODEL/ad/vocals.wav"
[ -f "$VOCALS" ] || { echo "demucs did not produce $VOCALS" >&2; exit 1; }

# ── 3. Trim to the line + normalise to 3DS banner format ──────────────
# IMPORTANT: this MUST be a separate ffmpeg invocation from the gain stage.
# Combining `-ss` with `afade` in one command drives afade off the source
# timeline (not the trimmed timeline) and silences everything.
echo "[3/4] Trimming to $TRIM_START s + $TRIM_LENGTH s, mono 22050 Hz…"
ffmpeg -hide_banner -loglevel error -y \
    -i "$VOCALS" \
    -ss "$TRIM_START" -t "$TRIM_LENGTH" \
    -ac 1 -ar 22050 \
    -acodec pcm_s16le \
    "$WORK/clip.wav"

# ── 4. Gain, head-trim, fades, final level ───────────────────────────
# Fades are applied AFTER the head-trim so they land on the final clip's
# timeline (not the pre-trim clip's — the fade-in used to live in the
# cut-off leading seconds and never made it into the output).
FINAL_LENGTH=$(awk "BEGIN { print $TRIM_LENGTH - $HEAD_TRIM }")
FADE_OUT_START=$(awk "BEGIN { print $FINAL_LENGTH - $FADE_DURATION }")
echo "[4/4] Gain +${GAIN_BOOST_DB} dB → head-trim ${HEAD_TRIM}s → fades ${FADE_DURATION}s → final ${FINAL_TRIM_DB} dB (output ${FINAL_LENGTH}s)…"

# Pass 1: raw gain on the full 3.5 s clip
ffmpeg -hide_banner -loglevel error -y \
    -i "$WORK/clip.wav" \
    -af "volume=${GAIN_BOOST_DB}dB" \
    -acodec pcm_s16le \
    "$WORK/boosted.wav"

# Pass 2: drop the leading $HEAD_TRIM seconds (separate invocation so
# the new PTS starts at 0; combining -ss with afade in one command
# drives the fade filter off the source timeline and silences output).
ffmpeg -hide_banner -loglevel error -y \
    -i "$WORK/boosted.wav" \
    -ss "$HEAD_TRIM" \
    -acodec pcm_s16le \
    "$WORK/cut.wav"

# Pass 3: final level + symmetric fade in/out on the trimmed clip.
ffmpeg -hide_banner -loglevel error -y \
    -i "$WORK/cut.wav" \
    -af "volume=${FINAL_TRIM_DB}dB,afade=t=in:st=0:d=${FADE_DURATION},afade=t=out:st=${FADE_OUT_START}:d=${FADE_DURATION}" \
    -acodec pcm_s16le \
    "$OUT"

# ── verify ─────────────────────────────────────────────────────────────
DUR=$(ffprobe -v error -show_entries format=duration -of default=nw=1:nk=1 "$OUT")
LEVELS=$(ffmpeg -hide_banner -loglevel info -i "$OUT" -af volumedetect -f null - 2>&1 \
         | grep -oE '(mean|max)_volume: -?[0-9.]+ dB' | tr '\n' ' ')
echo "Wrote $OUT"
echo "  duration : ${DUR}s"
echo "  levels   : ${LEVELS}"
echo
echo "Run \`make -f Makefile.3ds cia\` to repackage the CIA."
