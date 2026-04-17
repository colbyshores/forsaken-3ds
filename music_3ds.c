/*
 * music_3ds.c - CD audio music playback for 3DS via ndsp
 *
 * Streams WAV files from romfs:/music/ using a small double-buffer
 * in linear memory (~64KB total) instead of loading entire tracks
 * (~20MB each) which would starve the GPU's linear memory pool.
 *
 * Format expected: 16-bit signed LE PCM, mono, 32000 Hz.
 *
 * Track-to-level mapping matches the original 1998 Forsaken CD
 * soundtrack by The Swarm.
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
#define MUSIC_CHANNEL       23      /* last ndsp channel, reserved for music */
#define STREAM_BUF_SAMPLES  16384   /* samples per buffer half (~0.5s at 32kHz) */
#define STREAM_BUF_BYTES    (STREAM_BUF_SAMPLES * 2)  /* 16-bit mono = 2 bytes/sample */
#define NUM_WAVEBUFS        2       /* double buffer */

/* ---- state ---- */

static void       *s_stream_buf = NULL;   /* linear-allocated double buffer */
static ndspWaveBuf s_wavebufs[NUM_WAVEBUFS];
static FILE       *s_file = NULL;         /* currently open WAV file */
static u_int32_t   s_data_offset = 0;     /* file offset where PCM data starts */
static u_int32_t   s_data_size = 0;       /* total PCM data size in bytes */
static u_int32_t   s_data_read = 0;       /* bytes read so far in current loop */
static int         s_current_track = -1;
static bool        s_initialized = false;
static bool        s_playing = false;
static float       s_volume = 1.0f;
static int         s_fill_buf = 0;        /* which buffer to fill next */

/* ---- level-to-track mapping ---- */

static const int s_level_track[] = {
	5,  /* vol2       - Volcano                */
	3,  /* Ship       - Gargantuan             */
	6,  /* Bio-sphere - Reactor                */
	4,  /* military   - Sanctuary of Tloloc    */
	8,  /* sewer      - Condemned              */
	7,  /* arena      - Pure Bitch Power       */
	9,  /* Pship      - Flameout               */
	3,  /* alpha      - Gargantuan             */
	5,  /* thermal    - Volcano                */
	4,  /* oldtemple  - Sanctuary of Tloloc    */
	6,  /* high       - Reactor                */
	8,  /* Fedbankv   - Condemned              */
	9,  /* Capship    - Flameout               */
	7,  /* Refinery   - Pure Bitch Power       */
	10, /* cruise     - The Dead System         */
	2,  /* endscene   - Forsaken (main theme)  */
};

#define NUM_LEVEL_TRACKS (int)(sizeof(s_level_track) / sizeof(s_level_track[0]))

/* ---- streaming callback ---- */

static bool fill_buffer(int buf_index)
{
	ndspWaveBuf *wb = &s_wavebufs[buf_index];
	void *dst = (u_int8_t *)s_stream_buf + (buf_index * STREAM_BUF_BYTES);
	u_int32_t remaining, to_read, nread;

	if (!s_file || !s_playing)
		return false;

	remaining = s_data_size - s_data_read;
	if (remaining == 0)
	{
		/* loop: seek back to start of PCM data */
		fseek(s_file, s_data_offset, SEEK_SET);
		s_data_read = 0;
		remaining = s_data_size;
	}

	to_read = remaining < STREAM_BUF_BYTES ? remaining : STREAM_BUF_BYTES;
	nread = fread(dst, 1, to_read, s_file);
	if (nread == 0)
		return false;

	s_data_read += nread;

	DSP_FlushDataCache(dst, nread);

	memset(wb, 0, sizeof(ndspWaveBuf));
	wb->data_vaddr = dst;
	wb->nsamples = nread / 2;  /* 16-bit mono */
	wb->looping = false;       /* we handle looping manually */
	wb->status = NDSP_WBUF_FREE;

	ndspChnWaveBufAdd(MUSIC_CHANNEL, wb);
	return true;
}

/* ---- internal: open a WAV file and find the data chunk ---- */

static bool open_track(int track)
{
	char path[128];
	char chunk_id[4];
	u_int32_t chunk_size, file_size;

	snprintf(path, sizeof(path), "romfs:/music/track%02d.wav", track);

	FILE *f = fopen(path, "rb");
	if (!f)
	{
		DebugPrintf("music: cannot open %s\n", path);
		return false;
	}

	/* RIFF header */
	fread(chunk_id, 1, 4, f);
	fread(&file_size, 4, 1, f);
	fread(chunk_id, 1, 4, f); /* "WAVE" */

	/* scan for data chunk */
	while (fread(chunk_id, 1, 4, f) == 4)
	{
		fread(&chunk_size, 4, 1, f);

		if (memcmp(chunk_id, "data", 4) == 0)
		{
			s_data_offset = ftell(f);
			s_data_size = chunk_size;
			s_data_read = 0;
			s_file = f;

			DebugPrintf("music: opened track %02d (%u bytes PCM)\n",
				track, chunk_size);
			return true;
		}
		else
		{
			fseek(f, chunk_size, SEEK_CUR);
			if (chunk_size & 1)
				fseek(f, 1, SEEK_CUR);
		}
	}

	DebugPrintf("music: no data chunk in %s\n", path);
	fclose(f);
	return false;
}

static void close_track(void)
{
	if (s_file)
	{
		fclose(s_file);
		s_file = NULL;
	}
	s_current_track = -1;
}

/* ---- public API ---- */

bool music_init(void)
{
	if (s_initialized)
		return true;

	/* allocate double buffer in linear memory — only 64KB total */
	s_stream_buf = linearAlloc(STREAM_BUF_BYTES * NUM_WAVEBUFS);
	if (!s_stream_buf)
	{
		DebugPrintf("music: linearAlloc failed for stream buffer\n");
		return false;
	}
	memset(s_stream_buf, 0, STREAM_BUF_BYTES * NUM_WAVEBUFS);

	/* configure the music channel */
	ndspChnReset(MUSIC_CHANNEL);
	ndspChnSetInterp(MUSIC_CHANNEL, NDSP_INTERP_LINEAR);
	ndspChnSetRate(MUSIC_CHANNEL, (float)MUSIC_SAMPLE_RATE);
	ndspChnSetFormat(MUSIC_CHANNEL, NDSP_FORMAT_MONO_PCM16);

	float mix[12] = {0};
	mix[0] = s_volume;
	mix[1] = s_volume;
	ndspChnSetMix(MUSIC_CHANNEL, mix);

	s_initialized = true;
	DebugPrintf("music: initialized (stream buffer %u bytes)\n",
		STREAM_BUF_BYTES * NUM_WAVEBUFS);
	return true;
}

void music_shutdown(void)
{
	if (!s_initialized)
		return;

	music_stop();

	if (s_stream_buf)
	{
		linearFree(s_stream_buf);
		s_stream_buf = NULL;
	}

	s_initialized = false;
}

void music_play(int track)
{
	if (!s_initialized)
		return;

	if (track < MUSIC_FIRST_TRACK || track > MUSIC_LAST_TRACK)
		return;

	/* already playing this track */
	if (s_current_track == track && s_playing)
		return;

	/* stop current playback */
	music_stop();

	/* open new track */
	if (!open_track(track))
		return;

	s_current_track = track;
	s_playing = true;
	s_fill_buf = 0;

	/* prime both buffers */
	fill_buffer(0);
	fill_buffer(1);
}

void music_stop(void)
{
	if (!s_initialized)
		return;

	s_playing = false;
	ndspChnWaveBufClear(MUSIC_CHANNEL);
	ndspChnReset(MUSIC_CHANNEL);

	/* reconfigure after reset */
	ndspChnSetInterp(MUSIC_CHANNEL, NDSP_INTERP_LINEAR);
	ndspChnSetRate(MUSIC_CHANNEL, (float)MUSIC_SAMPLE_RATE);
	ndspChnSetFormat(MUSIC_CHANNEL, NDSP_FORMAT_MONO_PCM16);

	float mix[12] = {0};
	mix[0] = s_volume;
	mix[1] = s_volume;
	ndspChnSetMix(MUSIC_CHANNEL, mix);

	close_track();
}

bool music_is_playing(void)
{
	if (!s_initialized)
		return false;
	return s_playing && ndspChnIsPlaying(MUSIC_CHANNEL);
}

void music_set_volume(float vol)
{
	float mix[12] = {0};

	if (vol < 0.0f) vol = 0.0f;
	if (vol > 1.0f) vol = 1.0f;
	s_volume = vol;

	if (!s_initialized)
		return;

	mix[0] = vol;
	mix[1] = vol;
	ndspChnSetMix(MUSIC_CHANNEL, mix);
}

void music_play_for_level(int level_num)
{
	int track;

	if (level_num < 0)
		return;

	if (level_num < NUM_LEVEL_TRACKS)
		track = s_level_track[level_num];
	else
		track = MUSIC_FIRST_TRACK + (level_num % MUSIC_NUM_TRACKS);

	music_play(track);
}

void music_play_title(void)
{
	music_play(MUSIC_TITLE_TRACK);
}

/*
 * music_update() - call once per frame from the main loop.
 * Refills stream buffers as the DSP consumes them.
 */
void music_update(void)
{
	int i;

	if (!s_initialized || !s_playing || !s_file)
		return;

	for (i = 0; i < NUM_WAVEBUFS; i++)
	{
		if (s_wavebufs[i].status == NDSP_WBUF_DONE ||
		    s_wavebufs[i].status == NDSP_WBUF_FREE)
		{
			/* only refill buffers that the DSP has finished with,
			   or that haven't been queued yet (FREE after init) */
			if (s_wavebufs[i].nsamples > 0 ||
			    s_wavebufs[i].status == NDSP_WBUF_FREE)
			{
				/* skip if this buffer was never queued (nsamples==0 and FREE) */
				if (s_wavebufs[i].nsamples == 0 &&
				    s_wavebufs[i].status == NDSP_WBUF_FREE)
					continue;

				fill_buffer(i);
			}
		}
	}

	/* if nothing is playing and we're supposed to be, DSP drained both
	   buffers before we could refill — restart */
	if (!ndspChnIsPlaying(MUSIC_CHANNEL) && s_playing)
	{
		s_wavebufs[0].nsamples = 0;
		s_wavebufs[1].nsamples = 0;
		fill_buffer(0);
		fill_buffer(1);
	}
}

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
