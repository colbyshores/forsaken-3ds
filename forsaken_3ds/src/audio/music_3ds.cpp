/*
 * music_3ds.cpp - CD audio music playback for 3DS via ndsp.
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
 *   - SFX overload was breaking music: see sound_3ds.cpp MAX_SFX_VOICES cap.
 *
 * C-with-classes (AGENTS.md S9): the stream is one class owning the
 * file, the DSP buffers, the predictor state, the worker thread and its
 * lock; the public music_* API in audio/music.h is a flat wrapper over
 * the single instance.
 */

#ifdef __3DS__
#ifdef SOUND_SUPPORT

#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game/main.h"
#include "util/util.h"
#include "audio/music.h"
#include "platform_3ds/main_3ds.h"
#include "audio/dsp_adpcm.h"
#include "audio/music_table.h"

namespace {

/* ---- configuration ---- */

constexpr int MUSIC_SAMPLE_RATE = 32000;
constexpr int MUSIC_CHANNEL     = 23;
constexpr int STREAM_BUF_FRAMES = 1170;                     /* ~0.5 s @ 32 kHz */
constexpr int STREAM_BUF_BYTES  = STREAM_BUF_FRAMES * 8;    /* 9 360 B per buffer */
constexpr int NUM_WAVEBUFS      = 4;                        /* deep queue (~2 s headroom) */

/* ---- DSP-ADPCM CPU decode (shared dsp_adpcm module) ----
 * Used in state-only mode to track the end-of-buffer predictor
 * context the hardware decoder will have reached. */
void adpcm_advance(const u_int8_t *frames, u_int32_t num_frames,
                   const u_int16_t *coefs, ndspAdpcmData *ctx)
{
	dsp_state_t st;
	st.hist1 = (int16_t)ctx->history0;
	st.hist2 = (int16_t)ctx->history1;
	dsp_decode(frames, num_frames, coefs, &st, NULL);
	ctx->history0 = (int16_t)st.hist1;
	ctx->history1 = (int16_t)st.hist2;
}

/* Set the music channel's mix to the stream volume on both outputs. */
void set_channel_volume(float volume)
{
	float mix[12] = {0};
	mix[0] = volume; mix[1] = volume;
	ndspChnSetMix(MUSIC_CHANNEL, mix);
}

/* Reset the music channel and configure it for this stream's ADPCM. */
void configure_adpcm_channel(u_int16_t *coefs, float volume)
{
	ndspChnReset(MUSIC_CHANNEL);
	ndspChnSetInterp(MUSIC_CHANNEL, NDSP_INTERP_LINEAR);
	ndspChnSetRate(MUSIC_CHANNEL, (float)MUSIC_SAMPLE_RATE);
	ndspChnSetFormat(MUSIC_CHANNEL, NDSP_FORMAT_MONO_ADPCM);
	ndspChnSetAdpcmCoefs(MUSIC_CHANNEL, coefs);
	set_channel_volume(volume);
}

class MusicStream {
public:
	bool init();
	void shutdown();
	void play(int track);
	void stop();

private:
	bool fill_buffer(int buf_index);
	bool open_track(int track);
	void close_track();
	void thread_loop();
	static void thread_entry(void *self);

	void        *stream_buf = NULL;            /* linearAlloc'd scratch (DSP DMA) */
	ndspWaveBuf  wavebufs[NUM_WAVEBUFS] = {};

	FILE        *file = NULL;
	char         file_path[128] = {};          /* saved so we can reopen on loop */
	u_int32_t    data_offset = 0;
	u_int32_t    data_size = 0;
	u_int32_t    data_read = 0;

	int          current_track = -1;
	bool         initialized = false;
	bool         playing = false;
	float        volume = 1.0f;

	u_int16_t     adpcm_coefs[16] = {};
	ndspAdpcmData adpcm_per_buf[NUM_WAVEBUFS] = {};
	ndspAdpcmData adpcm_running = {};
	ndspAdpcmData adpcm_initial = {};

	Thread        thread = NULL;
	LightLock     lock = 0;
	volatile bool thread_exit = false;
};

/* ---- buffer fill (streamed from romfs via fread) ---- */

bool MusicStream::fill_buffer(int buf_index)
{
	ndspWaveBuf *wb = &wavebufs[buf_index];
	void *dst = (u_int8_t *)stream_buf + (buf_index * STREAM_BUF_BYTES);
	u_int32_t remaining, to_read, nread, num_frames;

	if (!file || !playing) return false;

	remaining = data_size - data_read;
	if (remaining == 0) {
		/* End of track — close + reopen the file, then seek back to
		 * the data start. fseek+clearerr alone is unreliable on
		 * libctru's romfs FILE* (EOF flag latches and subsequent
		 * freads keep returning 0). Predictor history reset to the
		 * file-header initial state since we're decoding from
		 * sample 1 again. */
		fclose(file);
		file = fopen(file_path, "rb");
		if (!file) return false;
		fseek(file, data_offset, SEEK_SET);
		data_read = 0;
		remaining = data_size;
		adpcm_running = adpcm_initial;
	}

	to_read = remaining < (u_int32_t)STREAM_BUF_BYTES ? remaining : (u_int32_t)STREAM_BUF_BYTES;
	to_read = (to_read / 8) * 8;
	if (to_read == 0) to_read = 8;

	nread = fread(dst, 1, to_read, file);
	if (nread == 0) return false;

	data_read += nread;
	num_frames = nread / 8;

	DSP_FlushDataCache(dst, nread);

	memset(wb, 0, sizeof(ndspWaveBuf));
	wb->data_adpcm = (u8*)dst;
	wb->nsamples = num_frames * 14;
	wb->looping = false;
	wb->status = NDSP_WBUF_FREE;

	adpcm_per_buf[buf_index] = adpcm_running;
	wb->adpcm_data = &adpcm_per_buf[buf_index];
	adpcm_advance((u_int8_t*)dst, num_frames, adpcm_coefs, &adpcm_running);

	ndspChnWaveBufAdd(MUSIC_CHANNEL, wb);
	return true;
}

/* ---- track open: parse DSP header ---- */

bool MusicStream::open_track(int track)
{
	FILE *f;
	u_int8_t hdr[96];
	int i;

	snprintf(file_path, sizeof(file_path),
		"romfs:/music/track%02d.dsp", track);
	f = fopen(file_path, "rb");
	if (!f) { DebugPrintf("music: cannot open %s\n", file_path); return false; }

	if (fread(hdr, 1, 96, f) != 96) { fclose(f); return false; }

	{
		dsp_header_t dh;
		dsp_parse_header(hdr, &dh);
		for (i = 0; i < 16; i++)
			adpcm_coefs[i] = dh.coefs[i];
		adpcm_initial.index    = dh.initial_index;
		adpcm_initial.history0 = dh.initial_hist1;
		adpcm_initial.history1 = dh.initial_hist2;
	}
	adpcm_running = adpcm_initial;

	/* Trust the file size, not the header's nibble count (offset 4).
	 * Different DSP encoders disagree about whether it covers data
	 * nibbles only (14/frame) or includes the per-frame header nibbles
	 * (16/frame). gc-dspadpcm-encode writes the latter, so
	 * num_nibbles/14 over-counts frames and produces a data_size larger
	 * than the file → fread returns 0 at the real EOF, the loop logic
	 * never fires (remaining > 0), and the track never restarts.
	 * Round the file size down to a frame boundary instead. */
	data_offset = 96;
	fseek(f, 0, SEEK_END);
	{
		long file_size = ftell(f);
		u_int32_t audio_bytes = (file_size > 96) ? (file_size - 96) : 0;
		data_size = (audio_bytes / 8) * 8;   /* frame-align */
	}
	fseek(f, data_offset, SEEK_SET);
	data_read = 0;
	file = f;

	DebugPrintf("music: opened track %02d (%u bytes ADPCM)\n",
		track, data_size);
	return true;
}

void MusicStream::close_track()
{
	if (file) { fclose(file); file = NULL; }
	current_track = -1;
}

/* ---- background streaming thread ---- */

void MusicStream::thread_entry(void *self)
{
	/* R1 (AGENTS.md §3): FPSCR is per-thread state and this worker touches
	 * floats in the channel-restart path (ndspChnSetRate / ndspChnSetMix).
	 * Establish Flush-to-Zero (FZ) + Default-NaN (DN) at entry so denormal
	 * results cannot stall the VFP11 on this thread, independent of what
	 * libctru initialized. See main_3ds.h for the full rationale. */
	vfp_thread_fz_dn();
	static_cast<MusicStream *>(self)->thread_loop();
}

void MusicStream::thread_loop()
{
	while (!thread_exit) {
		LightLock_Lock(&lock);
		if (initialized && playing && file) {
			int i;
			for (i = 0; i < NUM_WAVEBUFS; i++)
				if (wavebufs[i].status == NDSP_WBUF_DONE)
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
				configure_adpcm_channel(adpcm_coefs, volume);
				for (j = 0; j < NUM_WAVEBUFS; j++)
					memset(&wavebufs[j], 0, sizeof(ndspWaveBuf));
				for (j = 0; j < NUM_WAVEBUFS; j++)
					fill_buffer(j);
			}
		}
		LightLock_Unlock(&lock);
		svcSleepThread(25 * 1000 * 1000ULL);
	}
}

/* ---- lifecycle and playback ---- */

bool MusicStream::init()
{
	if (initialized) return true;

	stream_buf = linearAlloc(STREAM_BUF_BYTES * NUM_WAVEBUFS);
	if (!stream_buf) {
		DebugPrintf("music: linearAlloc failed for stream buffer\n");
		return false;
	}
	memset(stream_buf, 0, STREAM_BUF_BYTES * NUM_WAVEBUFS);

	ndspChnReset(MUSIC_CHANNEL);
	ndspChnSetInterp(MUSIC_CHANNEL, NDSP_INTERP_LINEAR);
	ndspChnSetRate(MUSIC_CHANNEL, (float)MUSIC_SAMPLE_RATE);
	ndspChnSetFormat(MUSIC_CHANNEL, NDSP_FORMAT_MONO_PCM16);
	set_channel_volume(volume);

	initialized = true;

	LightLock_Init(&lock);
	thread_exit = false;
	thread = threadCreate(thread_entry, this,
	                      16 * 1024, 0x28, -2, false);
	if (!thread)
		DebugPrintf("music: WARNING threadCreate failed\n");

	DebugPrintf("music: initialized (4x %u-byte buffers, thread=%p)\n",
		(unsigned)STREAM_BUF_BYTES, (void*)thread);
	return true;
}

void MusicStream::shutdown()
{
	if (!initialized) return;

	if (thread) {
		thread_exit = true;
		threadJoin(thread, U64_MAX);
		threadFree(thread);
		thread = NULL;
	}

	stop();

	if (stream_buf) { linearFree(stream_buf); stream_buf = NULL; }

	initialized = false;
}

void MusicStream::play(int track)
{
	int i;

	if (!initialized) return;
	if (track < MUSIC_FIRST_TRACK || track > MUSIC_LAST_TRACK) return;

	LightLock_Lock(&lock);
	if (current_track == track && playing) {
		LightLock_Unlock(&lock);
		return;
	}
	LightLock_Unlock(&lock);

	stop();

	LightLock_Lock(&lock);
	if (!open_track(track)) {
		LightLock_Unlock(&lock);
		return;
	}

	configure_adpcm_channel(adpcm_coefs, volume);

	current_track = track;
	playing = true;
	for (i = 0; i < NUM_WAVEBUFS; i++)
		fill_buffer(i);
	LightLock_Unlock(&lock);
}

void MusicStream::stop()
{
	if (!initialized) return;

	LightLock_Lock(&lock);
	playing = false;
	ndspChnWaveBufClear(MUSIC_CHANNEL);
	close_track();
	LightLock_Unlock(&lock);
}

MusicStream g_music;

} // namespace

/* ---- public API ---- */

bool music_init(void) { return g_music.init(); }
void music_shutdown(void) { g_music.shutdown(); }
void music_play(int track) { g_music.play(track); }
void music_stop(void) { g_music.stop(); }

void music_play_for_level(int level_num)
{
	if (level_num < 0) return;
	g_music.play(music_track_for_level(level_num));
}

void music_play_title(void) { g_music.play(MUSIC_TITLE_TRACK); }

void music_update(void) { /* no-op — background thread handles refills */ }

#else /* !SOUND_SUPPORT */

bool music_init(void) { return false; }
void music_shutdown(void) {}
void music_play(int track) { (void)track; }
void music_stop(void) {}
void music_play_for_level(int level_num) { (void)level_num; }
void music_play_title(void) {}
void music_update(void) {}

#endif /* SOUND_SUPPORT */
#endif /* __3DS__ */
