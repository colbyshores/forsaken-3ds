/*
 * music_3ds.c - CD audio music playback for 3DS via ndsp.
 *
 * Streams Nintendo DSP-ADPCM (.dsp) tracks from romfs:/music/.
 *
 * Design notes:
 *   - DSP-ADPCM is decoded by 3DS hardware, zero CPU during playback.
 *   - Streamed in 0.5 s ADPCM chunks from the romfs file (no full-track
 *     RAM buffer — the SFX system needs the linear-memory pool too, and
 *     HD textures already eat much of it).
 *   - A background thread refills wave buffers on a 25 ms tick so heavy
 *     gameplay frames cannot starve the music channel. All shared state
 *     is guarded by a LightLock.
 *   - Each wave buffer carries its OWN ndspAdpcmData start context;
 *     after writing each buffer we CPU-decode its frames just enough to
 *     compute the END predictor history, so the next buffer starts where
 *     this one ended. Without this, buffers share one context and drift
 *     glitches accumulate after a few minutes ("broken record" symptom).
 *   - SFX overload was breaking music: see sound_3ds.c MAX_SFX_VOICES cap.
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
#define STREAM_BUF_FRAMES   1170                          /* ~0.5 s @ 32 kHz */
#define STREAM_BUF_BYTES    (STREAM_BUF_FRAMES * 8)        /* 9 360 B per buffer */
#define NUM_WAVEBUFS        4                              /* deep queue (~2 s headroom) */

/* ---- state ---- */

static void       *s_stream_buf = NULL;          /* linearAlloc'd scratch (DSP DMA) */
static ndspWaveBuf s_wavebufs[NUM_WAVEBUFS];

static FILE       *s_file = NULL;
static char        s_file_path[128];        /* saved so we can reopen on loop */
static u_int32_t   s_data_offset = 0;
static u_int32_t   s_data_size = 0;
static u_int32_t   s_data_read = 0;

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

/* Level → CD audio track. Indices line up with Data/Levels/mission.dat
 * (the authoritative 1998 SP campaign order). Track values were
 * extracted from ForsakenHW.exe strings on the original 1998 CD —
 * the exe embeds an internal array of "<levelname> <trackN>" entries
 * where N is the CDDA track number (2-10 on the music CD). This is
 * the AUTHORITATIVE 1998 mapping straight from the shipped binary.
 * 9 distinct tracks, themes repeat across 15 levels as in the original. */
static const int s_level_track[] = {
	5,   /*  1. vol2       (Volcano)               */
	9,   /*  2. asubchb    (Abandoned Subway)      */
	6,   /*  3. nps-sp01   (Nuclear Power Station) */
	3,   /*  4. thermal    (Thermal Power Station) */
	2,   /*  5. fedbankv   (Federal Bank Vault)    */
	8,   /*  6. pship      (Prison Ship)           */
	4,   /*  7. spc-sp01   (Asteroid Base)         */
	9,   /*  8. bio-sphere (Bio-Sphere)            */
	3,   /*  9. nukerf     (Subterranean Complex)  */
	7,   /* 10. capship    (Capsized Ship)         */
	8,   /* 11. space      (Orbital Space Station) */
	10,  /* 12. high       (Shuttle Bay)           */
	5,   /* 13. military   (Military Base)         */
	3,   /* 14. azt-sp01   (Tloloc Temple)         */
	6,   /* 15. azchb      (Ancient Temple)        */
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

/* ---- buffer fill (streamed from SD via fread) ---- */

static bool fill_buffer(int buf_index)
{
	ndspWaveBuf *wb = &s_wavebufs[buf_index];
	void *dst = (u_int8_t *)s_stream_buf + (buf_index * STREAM_BUF_BYTES);
	u_int32_t remaining, to_read, nread, num_frames;

	if (!s_file || !s_playing) return false;

	remaining = s_data_size - s_data_read;
	if (remaining == 0) {
		/* End of track — close + reopen the file, then seek back to
		 * the data start. fseek+clearerr alone is unreliable on
		 * libctru's romfs FILE* (EOF flag latches and subsequent
		 * freads keep returning 0). Predictor history reset to the
		 * file-header initial state since we're decoding from
		 * sample 1 again. */
		fclose(s_file);
		s_file = fopen(s_file_path, "rb");
		if (!s_file) return false;
		fseek(s_file, s_data_offset, SEEK_SET);
		s_data_read = 0;
		remaining = s_data_size;
		s_adpcm_running = s_adpcm_initial;
	}

	to_read = remaining < STREAM_BUF_BYTES ? remaining : STREAM_BUF_BYTES;
	to_read = (to_read / 8) * 8;
	if (to_read == 0) to_read = 8;

	nread = fread(dst, 1, to_read, s_file);
	if (nread == 0) return false;

	s_data_read += nread;
	num_frames = nread / 8;

	DSP_FlushDataCache(dst, nread);

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

/* ---- track open: parse DSP header ---- */

static bool open_track(int track)
{
	FILE *f;
	u_int8_t hdr[96];
	int i;

	snprintf(s_file_path, sizeof(s_file_path),
		"romfs:/music/track%02d.dsp", track);
	f = fopen(s_file_path, "rb");
	if (!f) { DebugPrintf("music: cannot open %s\n", s_file_path); return false; }

	if (fread(hdr, 1, 96, f) != 96) { fclose(f); return false; }

	/* Header field at offset 4 is num_adpcm_nibbles, but encoders disagree
	 * on whether it counts data nibbles only or includes per-frame header
	 * nibbles. We trust the file size (below) instead — much simpler. */

	for (i = 0; i < 16; i++)
		s_adpcm_coefs[i] = bs16(*(u_int16_t*)&hdr[28 + i*2]);

	s_adpcm_initial.index    = bs16(*(u_int16_t*)&hdr[62]);
	s_adpcm_initial.history0 = (s16)bs16(*(u_int16_t*)&hdr[64]);
	s_adpcm_initial.history1 = (s16)bs16(*(u_int16_t*)&hdr[66]);
	s_adpcm_running = s_adpcm_initial;

	/* Trust the file size, not the header's nibble count. Different DSP
	 * encoders disagree about whether `nibble_count` covers data nibbles
	 * only (14/frame) or includes the per-frame header nibbles (16/frame).
	 * gc-dspadpcm-encode writes the latter, so num_nibbles/14 over-counts
	 * frames and produces a data_size larger than the file → fread returns
	 * 0 at the real EOF, our loop logic never fires (remaining > 0), and
	 * the track never restarts.
	 *
	 * Just round file size down to a frame boundary and call it a day. */
	s_data_offset = 96;
	fseek(f, 0, SEEK_END);
	{
		long file_size = ftell(f);
		u_int32_t audio_bytes = (file_size > 96) ? (file_size - 96) : 0;
		s_data_size = (audio_bytes / 8) * 8;   /* frame-align */
	}
	fseek(f, s_data_offset, SEEK_SET);
	s_data_read = 0;
	s_file = f;

	DebugPrintf("music: opened track %02d (%u bytes ADPCM)\n",
		track, s_data_size);
	return true;
}

static void close_track(void)
{
	if (s_file) { fclose(s_file); s_file = NULL; }
	s_current_track = -1;
}

/* ---- background streaming thread ---- */

static void music_thread_fn(void *arg)
{
	(void)arg;
	while (!s_thread_exit) {
		LightLock_Lock(&s_lock);
		if (s_initialized && s_playing && s_file) {
			int i;
			for (i = 0; i < NUM_WAVEBUFS; i++)
				if (s_wavebufs[i].status == NDSP_WBUF_DONE)
					fill_buffer(i);

			/* Track-loop kickstart: at the end of a track all 4 buffers
			 * finish in quick succession and the DSP channel goes fully
			 * idle. Just refilling those slots isn't enough — once the
			 * channel has drained completely, ndsp won't auto-resume
			 * playback when you ndspChnWaveBufAdd new buffers. Have to
			 * tear down + reconfigure to wake it. */
			if (!ndspChnIsPlaying(MUSIC_CHANNEL)) {
				int j;
				ndspChnWaveBufClear(MUSIC_CHANNEL);
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

	DebugPrintf("music: initialized (4x %u-byte buffers, thread=%p)\n",
		STREAM_BUF_BYTES, (void*)s_thread);
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

	music_stop();

	LightLock_Lock(&s_lock);
	if (!open_track(track)) {
		LightLock_Unlock(&s_lock);
		return;
	}

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

void music_update(void) { /* no-op — background thread handles refills */ }

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
