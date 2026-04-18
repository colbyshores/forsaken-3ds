/*
 * sound_3ds.c - Nintendo 3DS ndsp audio replacing sound_openal.c
 *
 * Uses the DSP service (ndsp) from libctru.
 * All audio buffers are allocated in linear memory so
 * the DSP hardware can read them directly.
 */

#ifdef __3DS__
#ifdef SOUND_SUPPORT

#include <3ds.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "main.h"
#include "util.h"
#include "sound.h"
#include "file.h"

/* ---- configuration ---- */

#define MAX_CHANNELS 24
#define MAX_PATH_LEN 500

bool Sound3D = false;

int sound_minimum_volume = -10000 / 3;

/* ---- internal structs ---- */

struct sound_buffer_t {
	void  *data;          /* linear-allocated PCM data */
	u_int32_t size;       /* data size in bytes */
	u_int32_t sample_rate;
	u_int16_t channels;   /* 1=mono, 2=stereo */
	u_int16_t bits;       /* 8 or 16 */
	char path[MAX_PATH_LEN];
};

struct sound_source_t {
	int channel;            /* ndsp channel index, -1 if none */
	sound_buffer_t *buffer;
	ndspWaveBuf wavebuf;
	bool playing;
	bool looping;
	float volume;
	float pan;
	char path[MAX_PATH_LEN];
};

/* SFX channel pool. The DSP firmware mixes channels in software and starts
 * skipping audio when too many channels are active simultaneously (which
 * killed both SFX and music under heavy combat). Capping total concurrent
 * SFX voices keeps the DSP under its mixing budget. Music owns channel 23
 * directly via music_3ds.c; SFX stay in 0-(MAX_SFX_VOICES-1).
 *
 * When all SFX slots are full, alloc evicts the OLDEST-allocated channel
 * (round-robin replacement) so a new gunshot doesn't get silently dropped. */
#define MAX_SFX_VOICES 8
static bool       channel_used[MAX_CHANNELS];
static u_int64_t  channel_alloc_time[MAX_CHANNELS];
static u_int64_t  s_alloc_seq = 0;

static int alloc_channel(void)
{
	int i, oldest_idx = 0;
	u_int64_t oldest = (u_int64_t)-1;

	for (i = 0; i < MAX_SFX_VOICES; i++)
	{
		if (!channel_used[i])
		{
			channel_used[i] = true;
			channel_alloc_time[i] = ++s_alloc_seq;
			return i;
		}
		if (channel_alloc_time[i] < oldest)
		{
			oldest = channel_alloc_time[i];
			oldest_idx = i;
		}
	}

	/* All slots full — evict the oldest. */
	ndspChnReset(oldest_idx);
	channel_alloc_time[oldest_idx] = ++s_alloc_seq;
	return oldest_idx;
}

static void free_channel(int ch)
{
	if (ch >= 0 && ch < MAX_SFX_VOICES)
		channel_used[ch] = false;
}

/* ---- init / destroy ---- */

static bool ndsp_initialized = false;

bool sound_init(void)
{
	if (ndsp_initialized)
		return true;

	Result rc = ndspInit();
	if (R_FAILED(rc))
	{
		DebugPrintf("sound_init: ndspInit failed (0x%08lx)\n", rc);
		return false;
	}

	ndspSetOutputMode(NDSP_OUTPUT_STEREO);
	memset(channel_used, 0, sizeof(channel_used));

	ndsp_initialized = true;
	DebugPrintf("sound_init: ndsp initialized\n");
	return true;
}

void sound_destroy(void)
{
	if (!ndsp_initialized)
		return;

	ndspExit();
	ndsp_initialized = false;
	DebugPrintf("sound_destroy: ndsp shut down\n");
}

/* ---- 3D listener (stubs for now, ndsp doesn't have built-in 3D) ---- */

bool sound_listener_position(float x, float y, float z)
{
	(void)x; (void)y; (void)z;
	return true;
}

bool sound_listener_velocity(float x, float y, float z)
{
	(void)x; (void)y; (void)z;
	return true;
}

bool sound_listener_orientation(
	float fx, float fy, float fz,
	float ux, float uy, float uz)
{
	(void)fx; (void)fy; (void)fz;
	(void)ux; (void)uy; (void)uz;
	return true;
}

/* ---- simple WAV loader (replaces SDL_LoadWAV) ---- */

/* minimal WAV header parsing */
typedef struct {
	char     riff[4];
	u_int32_t file_size;
	char     wave[4];
} wav_header_t;

typedef struct {
	char     id[4];
	u_int32_t size;
} wav_chunk_t;

typedef struct {
	u_int16_t format;
	u_int16_t channels;
	u_int32_t sample_rate;
	u_int32_t byte_rate;
	u_int16_t block_align;
	u_int16_t bits_per_sample;
} wav_fmt_t;

sound_buffer_t *sound_load(char *path)
{
	FILE *f;
	wav_header_t header;
	wav_chunk_t chunk;
	wav_fmt_t fmt;
	sound_buffer_t *buffer;
	char *file_path = convert_path(path);
	bool found_fmt = false, found_data = false;

	f = fopen(file_path, "rb");
	if (!f)
	{
		DebugPrintf("sound_load: cannot open %s\n", file_path);
		return NULL;
	}

	/* read RIFF header */
	if (fread(&header, sizeof(header), 1, f) != 1 ||
		memcmp(header.riff, "RIFF", 4) != 0 ||
		memcmp(header.wave, "WAVE", 4) != 0)
	{
		DebugPrintf("sound_load: not a valid WAV file: %s\n", file_path);
		fclose(f);
		return NULL;
	}

	buffer = malloc(sizeof(sound_buffer_t));
	if (!buffer) { fclose(f); return NULL; }
	memset(buffer, 0, sizeof(sound_buffer_t));
	strncpy(buffer->path, file_path, MAX_PATH_LEN - 1);

	/* scan chunks */
	while (fread(&chunk, sizeof(chunk), 1, f) == 1)
	{
		if (memcmp(chunk.id, "fmt ", 4) == 0)
		{
			if (fread(&fmt, sizeof(fmt), 1, f) != 1)
				break;
			/* skip any extra fmt bytes */
			if (chunk.size > sizeof(fmt))
				fseek(f, chunk.size - sizeof(fmt), SEEK_CUR);
			buffer->sample_rate = fmt.sample_rate;
			buffer->channels    = fmt.channels;
			buffer->bits        = fmt.bits_per_sample;
			found_fmt = true;
		}
		else if (memcmp(chunk.id, "data", 4) == 0)
		{
			buffer->size = chunk.size;
			/* allocate in linear memory for DSP access */
			buffer->data = linearAlloc(chunk.size);
			if (!buffer->data)
			{
				DebugPrintf("sound_load: linearAlloc failed for %u bytes\n", chunk.size);
				free(buffer);
				fclose(f);
				return NULL;
			}
			if (fread(buffer->data, 1, chunk.size, f) != chunk.size)
			{
				linearFree(buffer->data);
				free(buffer);
				fclose(f);
				return NULL;
			}
			/* flush CPU cache so DSP sees correct data */
			DSP_FlushDataCache(buffer->data, chunk.size);
			found_data = true;
		}
		else
		{
			/* skip unknown chunk */
			fseek(f, chunk.size, SEEK_CUR);
		}

		/* align to 2-byte boundary */
		if (chunk.size & 1)
			fseek(f, 1, SEEK_CUR);
	}

	fclose(f);

	if (!found_fmt || !found_data)
	{
		DebugPrintf("sound_load: incomplete WAV: %s\n", file_path);
		if (buffer->data) linearFree(buffer->data);
		free(buffer);
		return NULL;
	}

	DebugPrintf("sound_load: loaded %s (%u Hz, %d ch, %d bit, %u bytes)\n",
		file_path, buffer->sample_rate, buffer->channels,
		buffer->bits, buffer->size);

	return buffer;
}

/* ---- source management ---- */

sound_source_t *sound_source(sound_buffer_t *buffer)
{
	sound_source_t *src;
	int ch;

	if (!buffer)
	{
		DebugPrintf("sound_source: null buffer\n");
		return NULL;
	}

	ch = alloc_channel();
	if (ch < 0)
	{
		DebugPrintf("sound_source: no free channels\n");
		return NULL;
	}

	src = malloc(sizeof(sound_source_t));
	if (!src) { free_channel(ch); return NULL; }
	memset(src, 0, sizeof(sound_source_t));

	src->channel = ch;
	src->buffer  = buffer;
	src->playing = false;
	src->looping = false;
	src->volume  = 1.0f;
	src->pan     = 0.0f;
	strncpy(src->path, buffer->path, MAX_PATH_LEN - 1);

	/* configure ndsp channel */
	ndspChnReset(ch);
	ndspChnSetInterp(ch, NDSP_INTERP_LINEAR);
	ndspChnSetRate(ch, (float)buffer->sample_rate);

	if (buffer->bits == 8)
		ndspChnSetFormat(ch, buffer->channels == 2 ? NDSP_FORMAT_STEREO_PCM8 : NDSP_FORMAT_MONO_PCM8);
	else
		ndspChnSetFormat(ch, buffer->channels == 2 ? NDSP_FORMAT_STEREO_PCM16 : NDSP_FORMAT_MONO_PCM16);

	/* default volume mix: equal L/R */
	float mix[12] = {0};
	mix[0] = 1.0f; /* left */
	mix[1] = 1.0f; /* right */
	ndspChnSetMix(ch, mix);

	return src;
}

static void setup_wavebuf(sound_source_t *src)
{
	memset(&src->wavebuf, 0, sizeof(ndspWaveBuf));
	src->wavebuf.data_vaddr = src->buffer->data;
	src->wavebuf.nsamples = src->buffer->size /
		(src->buffer->channels * (src->buffer->bits / 8));
	src->wavebuf.looping = src->looping;
	src->wavebuf.status = NDSP_WBUF_FREE;

	DSP_FlushDataCache(src->buffer->data, src->buffer->size);
}

/* ---- playback ---- */

void sound_play(sound_source_t *src)
{
	if (!src || src->channel < 0) return;
	src->looping = false;
	src->playing = true;
	setup_wavebuf(src);
	ndspChnWaveBufAdd(src->channel, &src->wavebuf);
}

void sound_play_looping(sound_source_t *src)
{
	if (!src || src->channel < 0) return;
	src->looping = true;
	src->playing = true;
	setup_wavebuf(src);
	ndspChnWaveBufAdd(src->channel, &src->wavebuf);
}

void sound_stop(sound_source_t *src)
{
	if (!src || src->channel < 0) return;
	ndspChnWaveBufClear(src->channel);
	src->playing = false;
}

bool sound_is_playing(sound_source_t *src)
{
	if (!src || src->channel < 0) return false;
	return ndspChnIsPlaying(src->channel);
}

/* ---- parameters ---- */

void sound_set_pitch(sound_source_t *src, float pitch)
{
	if (!src || src->channel < 0) return;
	float rate = (float)src->buffer->sample_rate * (pitch > 0.0f ? pitch : 1.0f);
	ndspChnSetRate(src->channel, rate);
}

void sound_volume(sound_source_t *src, long millibels)
{
	float mix[12] = {0};
	float gain;

	if (!src || src->channel < 0) return;

	if (millibels > 0) millibels = 0;
	gain = (float)powf(10.0, millibels / 2000.0);
	src->volume = gain;

	/* apply pan: -1 (left) to +1 (right) */
	float left  = gain * (1.0f - src->pan) * 0.5f;
	float right = gain * (1.0f + src->pan) * 0.5f;
	if (left  < 0.0f) left  = 0.0f;
	if (right < 0.0f) right = 0.0f;

	mix[0] = left;
	mix[1] = right;
	ndspChnSetMix(src->channel, mix);
}

void sound_pan(sound_source_t *src, long _pan)
{
	float mix[12] = {0};
	float pan;

	if (!src || src->channel < 0) return;

	pan = (float)_pan / 10000.0f;
	if (pan < -1.0f) pan = -1.0f;
	if (pan >  1.0f) pan =  1.0f;
	src->pan = pan;

	float left  = src->volume * (1.0f - pan) * 0.5f;
	float right = src->volume * (1.0f + pan) * 0.5f;

	mix[0] = left;
	mix[1] = right;
	ndspChnSetMix(src->channel, mix);
}

void sound_position(sound_source_t *src, float x, float y, float z,
                    float min_distance, float max_distance)
{
	/* Basic positional audio approximation:
	   Just do simple left/right panning based on x position
	   relative to min/max distance */
	if (!src || src->channel < 0) return;

	float dist = sqrtf(x*x + y*y + z*z);
	if (dist < min_distance) dist = min_distance;

	/* attenuation */
	float atten = 1.0f;
	if (dist > min_distance && max_distance > min_distance)
	{
		atten = 1.0f - (dist - min_distance) / (max_distance - min_distance);
		if (atten < 0.0f) atten = 0.0f;
	}

	/* pan from x position */
	float pan = 0.0f;
	if (dist > 0.001f)
		pan = x / dist;

	float mix[12] = {0};
	mix[0] = atten * (1.0f - pan) * 0.5f;
	mix[1] = atten * (1.0f + pan) * 0.5f;
	ndspChnSetMix(src->channel, mix);
}

/* ---- release ---- */

void sound_release_source(sound_source_t *src)
{
	if (!src) return;
	if (src->channel >= 0)
	{
		ndspChnWaveBufClear(src->channel);
		free_channel(src->channel);
	}
	free(src);
}

void sound_release_buffer(sound_buffer_t *buffer)
{
	if (!buffer) return;
	if (buffer->data)
		linearFree(buffer->data);
	free(buffer);
}

#endif /* SOUND_SUPPORT */
#endif /* __3DS__ */
