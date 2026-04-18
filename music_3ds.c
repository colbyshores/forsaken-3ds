/*
 * music_3ds.c - CD audio music playback for 3DS via ndsp.
 *
 * Plays Nintendo DSP-ADPCM (.dsp) tracks from romfs:/music/.
 *
 * Design notes:
 *   - DSP-ADPCM is decoded by 3DS hardware (zero CPU during playback).
 *   - Each track (~5 MB) is read into heap RAM ONCE at music_play time
 *     and played from RAM. Avoids SD-card streaming, which competes with
 *     texture loads / sound-effect loads and caused the music to break
 *     up under heavy gameplay (the "broken record" symptom).
 *   - Track data is `malloc`'d, NOT `linearAlloc`'d — the DSP only DMAs
 *     from the small wave-buffer scratch (which IS linearAlloc'd); the
 *     full track buffer is just a memcpy source. Saves linear memory
 *     for HD textures.
 *   - A background thread refills wave buffers on a 25 ms tick, decoupled
 *     from the main game loop so heavy SFX or render frames cannot starve
 *     music updates.
 *   - Each wave buffer carries its own `ndspAdpcmData` start context;
 *     after writing each buffer we CPU-decode its frames just enough to
 *     compute the END predictor history, so the next buffer starts where
 *     this one left off (eliminates buffer-boundary glitches).
 */

#ifdef __3DS__
#ifdef SOUND_SUPPORT

#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"
#include "util.h"
#include "music.h"

/* ---- configuration ---- */

#define MUSIC_SAMPLE_RATE   32000
#define MUSIC_CHANNEL       23
#define STREAM_BUF_FRAMES   4700                          /* ~2 s @ 32 kHz */
#define STREAM_BUF_BYTES    (STREAM_BUF_FRAMES * 8)        /* 37 600 B per buffer */
#define NUM_WAVEBUFS        4                              /* deep queue */

/* ---- state ---- */

static void       *s_stream_buf = NULL;          /* linearAlloc'd scratch */
static ndspWaveBuf s_wavebufs[NUM_WAVEBUFS];

static u_int8_t   *s_track_data = NULL;          /* heap-allocated, full track */
static u_int32_t   s_track_size = 0;
static u_int32_t   s_track_pos  = 0;

static int         s_current_track = -1;
static bool        s_initialized = false;
static bool        s_playing = false;
static float       s_volume = 1.0f;

static u_int16_t     s_adpcm_coefs[16];
static ndspAdpcmData s_adpcm_per_buf[NUM_WAVEBUFS];
static ndspAdpcmData s_adpcm_running;
static ndspAdpcmData s_adpcm_initial;

static Thread        s_thread = NULL;
static LightLock     s_lock;
static volatile bool s_thread_exit = false;

/* ---- level→track mapping ---- */

static const int s_level_track[] = {
	5, 3, 6, 4, 8, 7, 9, 3, 5, 4, 6, 8, 9, 7, 10, 2,
};
#define NUM_LEVEL_TRACKS (int)(sizeof(s_level_track) / sizeof(s_level_track[0]))

/* ---- byte-swap helpers (DSP headers are big-endian) ---- */

static inline u_int16_t bs16(u_int16_t v) { return (v >> 8) | (v << 8); }
static inline u_int32_t bs32(u_int32_t v) {
	return ((v >> 24) & 0xFF) | ((v >> 8) & 0xFF00) |
	       ((v << 8) & 0xFF0000) | ((v << 24) & 0xFF000000);
}

/* ---- DSP-ADPCM CPU decoder for tracking end-of-buffer context ---- */

static void adpcm_advance(const u_int8_t *frames, u_int32_t num_frames,
                          const u_int16_t *coefs, ndspAdpcmData *ctx)
{
	int32_t hist1 = (int16_t)ctx->history0;
	int32_t hist2 = (int16_t)ctx->history1;
	u_int32_t f;
	for (f = 0; f < num_frames; f++)
	{
		const u_int8_t *frame = frames + f * 8;
		u_int8_t header = frame[0];
		u_int8_t predictor = (header >> 4) & 0x07;
		u_int8_t scale = header & 0x0F;
		int16_t c1 = (int16_t)coefs[predictor * 2 + 0];
		int16_t c2 = (int16_t)coefs[predictor * 2 + 1];
		int n;
		for (n = 0; n < 14; n++)
		{
			int byte_idx = 1 + (n >> 1);
			int32_t nibble = (n & 1) ? (frame[byte_idx] & 0x0F)
			                         : ((frame[byte_idx] >> 4) & 0x0F);
			if (nibble & 0x08) nibble |= 0xFFFFFFF0;
			int32_t sample = nibble << scale;
			int32_t prediction = (hist1 * c1 + hist2 * c2) >> 11;
			int32_t decoded = prediction + sample;
			if (decoded > 32767) decoded = 32767;
			else if (decoded < -32768) decoded = -32768;
			hist2 = hist1;
			hist1 = decoded;
		}
	}
	ctx->history0 = (int16_t)hist1;
	ctx->history1 = (int16_t)hist2;
}

/* ---- buffer fill (memcpy from in-RAM track) ---- */

static bool fill_buffer(int buf_index)
{
	ndspWaveBuf *wb = &s_wavebufs[buf_index];
	void *dst = (u_int8_t *)s_stream_buf + (buf_index * STREAM_BUF_BYTES);
	u_int32_t remaining, to_copy, num_frames;

	if (!s_track_data || !s_playing) return false;

	remaining = s_track_size - s_track_pos;
	if (remaining == 0) {
		s_track_pos = 0;
		remaining = s_track_size;
		s_adpcm_running = s_adpcm_initial;
	}

	to_copy = remaining < STREAM_BUF_BYTES ? remaining : STREAM_BUF_BYTES;
	to_copy = (to_copy / 8) * 8;
	if (to_copy == 0) to_copy = 8;

	memcpy(dst, s_track_data + s_track_pos, to_copy);
	s_track_pos += to_copy;
	num_frames = to_copy / 8;

	DSP_FlushDataCache(dst, to_copy);

	memset(wb, 0, sizeof(ndspWaveBuf));
	wb->data_adpcm = (u8*)dst;
	wb->nsamples = num_frames * 14;
	wb->looping = false;
	wb->status = NDSP_WBUF_FREE;

	s_adpcm_per_buf[buf_index] = s_adpcm_running;
	wb->adpcm_data = &s_adpcm_per_buf[buf_index];
	adpcm_advance((u_int8_t*)dst, num_frames, s_adpcm_coefs, &s_adpcm_running);

	ndspChnWaveBufAdd(MUSIC_CHANNEL, wb);
	return true;
}

/* ---- track open: load entire .dsp into heap RAM, parse header ---- */

static bool open_track(int track)
{
	char path[128];
	FILE *f;
	u_int8_t hdr[96];
	u_int32_t total_nibbles, num_frames, audio_size;
	int i;

	snprintf(path, sizeof(path), "romfs:/music/track%02d.dsp", track);
	f = fopen(path, "rb");
	if (!f) { DebugPrintf("music: cannot open %s\n", path); return false; }

	if (fread(hdr, 1, 96, f) != 96) { fclose(f); return false; }

	total_nibbles = bs32(*(u_int32_t*)&hdr[4]);

	for (i = 0; i < 16; i++)
		s_adpcm_coefs[i] = bs16(*(u_int16_t*)&hdr[28 + i*2]);

	s_adpcm_initial.index    = bs16(*(u_int16_t*)&hdr[62]);
	s_adpcm_initial.history0 = (s16)bs16(*(u_int16_t*)&hdr[64]);
	s_adpcm_initial.history1 = (s16)bs16(*(u_int16_t*)&hdr[66]);
	s_adpcm_running = s_adpcm_initial;

	num_frames = total_nibbles / 14;
	audio_size = num_frames * 8;

	/* Use heap (NOT linearAlloc) — track data is a memcpy source, never
	 * DMA-read directly. Linear memory is reserved for HD textures. */
	s_track_data = (u_int8_t*)malloc(audio_size);
	if (!s_track_data) {
		DebugPrintf("music: malloc(%u) failed for track %02d\n", audio_size, track);
		fclose(f);
		return false;
	}
	if (fread(s_track_data, 1, audio_size, f) != audio_size) {
		free(s_track_data); s_track_data = NULL;
		fclose(f);
		return false;
	}
	fclose(f);

	s_track_size = audio_size;
	s_track_pos = 0;

	DebugPrintf("music: loaded track %02d into RAM (%u bytes ADPCM)\n",
		track, audio_size);
	return true;
}

static void close_track(void)
{
	if (s_track_data) { free(s_track_data); s_track_data = NULL; }
	s_track_size = 0;
	s_track_pos = 0;
	s_current_track = -1;
}

/* ---- background streaming thread ---- */

static void music_thread_fn(void *arg)
{
	(void)arg;
	while (!s_thread_exit) {
		LightLock_Lock(&s_lock);
		if (s_initialized && s_playing && s_track_data) {
			int i;
			for (i = 0; i < NUM_WAVEBUFS; i++)
				if (s_wavebufs[i].status == NDSP_WBUF_DONE)
					fill_buffer(i);

			if (!ndspChnIsPlaying(MUSIC_CHANNEL)) {
				int j;
				for (j = 0; j < NUM_WAVEBUFS; j++)
					memset(&s_wavebufs[j], 0, sizeof(ndspWaveBuf));
				for (j = 0; j < NUM_WAVEBUFS; j++)
					fill_buffer(j);
			}
		}
		LightLock_Unlock(&s_lock);
		svcSleepThread(25 * 1000 * 1000ULL);
	}
}

/* ---- public API ---- */

bool music_init(void)
{
	if (s_initialized) return true;

	s_stream_buf = linearAlloc(STREAM_BUF_BYTES * NUM_WAVEBUFS);
	if (!s_stream_buf) {
		DebugPrintf("music: linearAlloc failed for stream buffer\n");
		return false;
	}
	memset(s_stream_buf, 0, STREAM_BUF_BYTES * NUM_WAVEBUFS);

	/* Initial channel config — leave format on a safe default; music_play
	 * switches to ADPCM and supplies coefs once a track is opened. */
	ndspChnReset(MUSIC_CHANNEL);
	ndspChnSetInterp(MUSIC_CHANNEL, NDSP_INTERP_LINEAR);
	ndspChnSetRate(MUSIC_CHANNEL, (float)MUSIC_SAMPLE_RATE);
	ndspChnSetFormat(MUSIC_CHANNEL, NDSP_FORMAT_MONO_PCM16);

	{
		float mix[12] = {0};
		mix[0] = s_volume; mix[1] = s_volume;
		ndspChnSetMix(MUSIC_CHANNEL, mix);
	}

	s_initialized = true;

	LightLock_Init(&s_lock);
	s_thread_exit = false;
	s_thread = threadCreate(music_thread_fn, NULL,
	                        16 * 1024, 0x28, -2, false);
	if (!s_thread)
		DebugPrintf("music: WARNING threadCreate failed\n");

	DebugPrintf("music: initialized (%u-byte scratch x %d, thread=%p)\n",
		STREAM_BUF_BYTES, NUM_WAVEBUFS, (void*)s_thread);
	return true;
}

void music_shutdown(void)
{
	if (!s_initialized) return;

	if (s_thread) {
		s_thread_exit = true;
		threadJoin(s_thread, U64_MAX);
		threadFree(s_thread);
		s_thread = NULL;
	}

	music_stop();

	if (s_stream_buf) { linearFree(s_stream_buf); s_stream_buf = NULL; }

	s_initialized = false;
}

void music_play(int track)
{
	int i;

	if (!s_initialized) return;
	if (track < MUSIC_FIRST_TRACK || track > MUSIC_LAST_TRACK) return;

	LightLock_Lock(&s_lock);
	if (s_current_track == track && s_playing) {
		LightLock_Unlock(&s_lock);
		return;
	}
	LightLock_Unlock(&s_lock);

	music_stop();          /* takes the lock itself */

	LightLock_Lock(&s_lock);
	if (!open_track(track)) {
		LightLock_Unlock(&s_lock);
		return;
	}

	/* Full reconfigure — channel may be in PCM16 from init or a previous
	 * track. Reset clears any stale buffer queue, then re-apply rate /
	 * format / mix / coefs in order. */
	ndspChnReset(MUSIC_CHANNEL);
	ndspChnSetInterp(MUSIC_CHANNEL, NDSP_INTERP_LINEAR);
	ndspChnSetRate(MUSIC_CHANNEL, (float)MUSIC_SAMPLE_RATE);
	ndspChnSetFormat(MUSIC_CHANNEL, NDSP_FORMAT_MONO_ADPCM);
	ndspChnSetAdpcmCoefs(MUSIC_CHANNEL, s_adpcm_coefs);

	{
		float mix[12] = {0};
		mix[0] = s_volume; mix[1] = s_volume;
		ndspChnSetMix(MUSIC_CHANNEL, mix);
	}

	s_current_track = track;
	s_playing = true;
	for (i = 0; i < NUM_WAVEBUFS; i++)
		fill_buffer(i);
	LightLock_Unlock(&s_lock);
}

void music_stop(void)
{
	if (!s_initialized) return;

	LightLock_Lock(&s_lock);
	s_playing = false;
	ndspChnWaveBufClear(MUSIC_CHANNEL);
	close_track();
	LightLock_Unlock(&s_lock);
}

bool music_is_playing(void)
{
	if (!s_initialized) return false;
	return s_playing && ndspChnIsPlaying(MUSIC_CHANNEL);
}

void music_set_volume(float vol)
{
	float mix[12] = {0};
	if (vol < 0.0f) vol = 0.0f;
	if (vol > 1.0f) vol = 1.0f;
	s_volume = vol;
	if (!s_initialized) return;
	mix[0] = vol; mix[1] = vol;
	ndspChnSetMix(MUSIC_CHANNEL, mix);
}

void music_play_for_level(int level_num)
{
	int track;
	if (level_num < 0) return;
	if (level_num < NUM_LEVEL_TRACKS)
		track = s_level_track[level_num];
	else
		track = MUSIC_FIRST_TRACK + (level_num % MUSIC_NUM_TRACKS);
	music_play(track);
}

void music_play_title(void) { music_play(MUSIC_TITLE_TRACK); }

/* No-op — background thread does the refill work. */
void music_update(void) {}

#else /* !SOUND_SUPPORT */

bool music_init(void) { return false; }
void music_shutdown(void) {}
void music_play(int track) { (void)track; }
void music_stop(void) {}
bool music_is_playing(void) { return false; }
void music_set_volume(float vol) { (void)vol; }
void music_play_for_level(int level_num) { (void)level_num; }
void music_play_title(void) {}
void music_update(void) {}

#endif /* SOUND_SUPPORT */
#endif /* __3DS__ */
