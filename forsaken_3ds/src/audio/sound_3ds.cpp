/* sound_3ds.cpp -- sound.h for the 3DS, on libctru's DSP service (ndsp)
 *
 * sound_load parses the RIFF WAV itself (PCM, 8 or 16 bit, mono or stereo)
 * into linear memory, which the DSP reads directly; 8-bit data is flipped to
 * the signed form ndsp expects. sound_source claims an ndsp channel from a
 * pool of MAX_SFX_VOICES (channels 0-7) for the life of the source. When all
 * are taken, the channel claimed longest ago is reset and handed over: the
 * evicted source goes silent and is left with no channel (-1). Channel 23
 * belongs to music_3ds.cpp. Volume (millibels) and pan become the channel's
 * left/right mix. The listener calls are no-ops; sfx.cpp pans in software.
 *
 * sound_init first copies romfs:/dspfirm.cdc to sdmc:/3ds/dspfirm.cdc if the
 * SD copy is missing, because ndspInit loads the DSP firmware from there.
 * Only development .3dsx builds carry the romfs copy (see sound_3ds.h for the
 * boot-time warning when neither exists); without the firmware sound_init
 * fails and the game runs silent.
 */

#ifdef __3DS__

#include <3ds.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/stat.h>     /* mkdir() for dspfirm.cdc bootstrap */

#include "game/main.h"
#include "audio/sound_3ds.h"
#include "util/util.h"
#include "audio/sound.h"
#include "data/file.h"

/* ---- configuration ---- */

constexpr int MAX_CHANNELS = 24;
constexpr int MAX_PATH_LEN = 500;

/* The volume floor is AudioOpts.sound_minimum_volume (sfx.cpp). */



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

namespace {

/* The ndsp service and the SFX channel pool (one instance, g_sound).
 *
 * The DSP firmware mixes channels in software and starts skipping audio
 * when too many channels are active simultaneously (which killed both SFX
 * and music under heavy combat). Capping total concurrent SFX voices keeps
 * the DSP under its mixing budget. Music owns channel 23 directly via
 * music_3ds.cpp; SFX stay in 0-(MAX_SFX_VOICES-1).
 *
 * When all SFX slots are full, alloc_channel evicts the OLDEST-allocated
 * channel (round-robin replacement) so a new gunshot doesn't get silently
 * dropped. */
constexpr int MAX_SFX_VOICES = 8;

class SoundSystem {
	public:
	bool init();
	void destroy();

	int  alloc_channel();
	void free_channel(int ch);
	/* Record the source holding a channel alloc_channel returned. */
	void set_owner(int ch, sound_source_t *src) { channel_owner[ch] = src; }

	private:
	bool       ndsp_initialized = false;
	bool       channel_used[MAX_CHANNELS] = {};
	u_int64_t  channel_alloc_time[MAX_CHANNELS] = {};
	u_int64_t  alloc_seq = 0;
	/* The source holding each SFX channel, so an evicted source can be told
	   it no longer has one (otherwise its volume/pan/release calls would act
	   on the sound that took the channel over). */
	sound_source_t *channel_owner[MAX_SFX_VOICES] = {};
};

int SoundSystem::alloc_channel()
{
	int i, oldest_idx = 0;
	u_int64_t oldest = (u_int64_t)-1;

	for (i = 0; i < MAX_SFX_VOICES; i++)
	{
		if (!channel_used[i])
		{
			channel_used[i] = true;
			channel_alloc_time[i] = ++alloc_seq;
			return i;
		}
		if (channel_alloc_time[i] < oldest)
		{
			oldest = channel_alloc_time[i];
			oldest_idx = i;
		}
	}

	/* All slots full — evict the oldest. Its source keeps no channel. */
	ndspChnReset(oldest_idx);
	if (channel_owner[oldest_idx])
		channel_owner[oldest_idx]->channel = -1;
	channel_owner[oldest_idx] = nullptr;
	channel_alloc_time[oldest_idx] = ++alloc_seq;
	return oldest_idx;
}

void SoundSystem::free_channel(int ch)
{
	if (ch >= 0 && ch < MAX_SFX_VOICES)
	{
		channel_used[ch] = false;
		channel_owner[ch] = nullptr;
	}
}

/* ---- init / destroy ---- */

/* ndspInit requires the DSP firmware blob at sdmc:/3ds/dspfirm.cdc.
 * Development .3dsx builds bundle a copy in romfs (the CIA and the release
 * .3dsx do not); copy it to the SD if the SD copy is missing or
 * zero-length, so emulator and dev runs need no separate dump step. */
void ensure_dspfirm_on_sd(void)
{
	const char *sd_path  = "sdmc:/3ds/dspfirm.cdc";
	const char *src_path = "romfs:/dspfirm.cdc";
	FILE *test = fopen(sd_path, "rb");
	if (test) {
		fseek(test, 0, SEEK_END);
		long sz = ftell(test);
		fclose(test);
		if (sz > 0) return;        /* already there */
	}
	FILE *src = fopen(src_path, "rb");
	if (!src) return;              /* no bundled copy — user is on their own */

	mkdir("sdmc:/3ds", 0777);      /* harmless if it already exists */

	FILE *dst = fopen(sd_path, "wb");
	if (!dst) { fclose(src); return; }
	char buf[4096];
	size_t n;
	while ((n = fread(buf, 1, sizeof(buf), src)) > 0)
		fwrite(buf, 1, n, dst);
	fclose(dst);
	fclose(src);
	DebugPrintf("sound_init: extracted dspfirm.cdc to %s\n", sd_path);
}

bool SoundSystem::init()
{
	if (ndsp_initialized)
		return true;

	ensure_dspfirm_on_sd();

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

void SoundSystem::destroy()
{
	if (!ndsp_initialized)
		return;

	ndspExit();
	ndsp_initialized = false;
	DebugPrintf("sound_destroy: ndsp shut down\n");
}

SoundSystem g_sound;

} // namespace

/* Returns true if we expect ndspInit() to succeed later — either the
 * firmware blob is on the virtual SD already, or we have a copy in romfs
 * that ensure_dspfirm_on_sd() will install on first launch. Called at
 * startup (before c3d_context_init) so we can show a user-friendly warning if
 * the firmware is missing on a CIA install. */
bool sound_check_dsp_firmware_available(void)
{
	FILE *f = fopen("sdmc:/3ds/dspfirm.cdc", "rb");
	if (f)
	{
		fseek(f, 0, SEEK_END);
		long sz = ftell(f);
		fclose(f);
		if (sz > 0) return true;
	}
	f = fopen("romfs:/dspfirm.cdc", "rb");
	if (f)
	{
		fseek(f, 0, SEEK_END);
		long sz = ftell(f);
		fclose(f);
		if (sz > 0) return true;       /* dev .3dsx build, will be auto-copied */
	}
	return false;
}

/* Top-screen console warning shown when neither firmware copy exists (a
 * CIA or release .3dsx on a console that never ran dsp1). Runs before
 * c3d_context_init so we can cleanly hand over the top screen to citro3d
 * once the user dismisses it.
 *
 * Requires gfxInit() to have run already. */
void sound_show_missing_firmware_warning(void)
{
	consoleInit(GFX_TOP, nullptr);

	printf("\x1b[2J");                 /* clear */
	printf("\n  FORSAKEN 3DS: AUDIO DISABLED\n");
	printf("  ----------------------------\n\n");
	printf("  The 3DS DSP firmware is missing.\n\n");
	printf("  Installing custom firmware on a 3DS\n");
	printf("  does not automatically dump it -- there\n");
	printf("  is a separate one-time tool for that.\n\n");
	printf("  To enable audio:\n\n");
	printf("    1. Download dsp1.3dsx from\n");
	printf("         github.com/zoogie/DSP1\n");
	printf("    2. Run it from the Homebrew Launcher\n");
	printf("    3. Relaunch Forsaken\n\n");
	printf("  Press A to continue without audio.\n");

	/* Block until A is pressed. */
	while (aptMainLoop())
	{
		hidScanInput();
		u32 down = hidKeysDown();
		if (down & KEY_A) break;
		if (down & (KEY_START | KEY_SELECT)) break;
		gspWaitForVBlank();
	}

	/* Clear the console so when citro3d takes over the top screen
	 * there's no leftover text bleeding through the first frame. */
	printf("\x1b[2J");
	gfxFlushBuffers();
	gfxSwapBuffers();
	gspWaitForVBlank();

	/* consoleInit disables double buffering on the target screen so text
	 * writes don't flicker. citro3d assumes double-buffered output and
	 * will race the display if we leave it single-buffered, producing
	 * striped corruption on every frame. Restore the default format and
	 * re-enable double buffering before handing the screen back. */
	gfxSetScreenFormat(GFX_TOP, GSP_BGR8_OES);
	gfxSetDoubleBuffering(GFX_TOP, true);
	gfxFlushBuffers();
	gfxSwapBuffers();
	gspWaitForVBlank();
	gfxSwapBuffers();
	gspWaitForVBlank();
}

bool sound_init(void) { return g_sound.init(); }
void sound_destroy(void) { g_sound.destroy(); }

/* ---- 3D listener (no-ops: ndsp has no 3D positioning) ---- */

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
struct wav_header_t {
	char     riff[4];
	u_int32_t file_size;
	char     wave[4];
};

struct wav_chunk_t {
	char     id[4];
	u_int32_t size;
};

struct wav_fmt_t {
	u_int16_t format;
	u_int16_t channels;
	u_int32_t sample_rate;
	u_int32_t byte_rate;
	u_int16_t block_align;
	u_int16_t bits_per_sample;
};

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
		return nullptr;
	}

	/* read RIFF header */
	if (fread(&header, sizeof(header), 1, f) != 1 ||
		memcmp(header.riff, "RIFF", 4) != 0 ||
		memcmp(header.wave, "WAVE", 4) != 0)
	{
		DebugPrintf("sound_load: not a valid WAV file: %s\n", file_path);
		fclose(f);
		return nullptr;
	}

	buffer = (sound_buffer_t *)malloc(sizeof(sound_buffer_t));
	if (!buffer) { fclose(f); return nullptr; }
	memset(buffer, 0, sizeof(sound_buffer_t));
	copy_string(buffer->path, sizeof(buffer->path), file_path);

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
				return nullptr;
			}
			if (fread(buffer->data, 1, chunk.size, f) != chunk.size)
			{
				linearFree(buffer->data);
				free(buffer);
				fclose(f);
				return nullptr;
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
		return nullptr;
	}

	/* WAV 8-bit PCM is UNSIGNED (silence = 128), but ndsp's
	 * NDSP_FORMAT_*_PCM8 decodes SIGNED 8-bit (silence = 0). Feeding the
	 * unsigned bytes straight to the DSP puts a half-scale DC offset on
	 * every sample and splits the waveform around the silence level, which
	 * reads as loud garble. This bites ~70% of Forsaken's sound set — every
	 * biker taunt and the bike-computer / pickup announcer voice (e.g.
	 * BRE = Brenda) are 8-bit, which is exactly what sounded broken.
	 * Convert unsigned -> signed in place by flipping the sign bit
	 * (0..255 -> -128..127, silence 128 -> 0); correct for interleaved
	 * stereo too since each byte is one sample. 16-bit WAV is already
	 * signed and matches PCM16, so it is left untouched. */
	if (buffer->bits == 8 && buffer->data)
	{
		u_int8_t *p = (u_int8_t *)buffer->data;
		u_int32_t k;
		for (k = 0; k < buffer->size; k++)
			p[k] ^= 0x80u;
		DSP_FlushDataCache(buffer->data, buffer->size);
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
		return nullptr;
	}

	ch = g_sound.alloc_channel();
	if (ch < 0)
	{
		DebugPrintf("sound_source: no free channels\n");
		return nullptr;
	}

	src = (sound_source_t *)malloc(sizeof(sound_source_t));
	if (!src) { g_sound.free_channel(ch); return nullptr; }
	memset(src, 0, sizeof(sound_source_t));

	src->channel = ch;
	g_sound.set_owner(ch, src);
	src->buffer  = buffer;
	src->playing = false;
	src->looping = false;
	src->volume  = 1.0f;
	src->pan     = 0.0f;
	copy_string( src->path, MAX_PATH_LEN, buffer->path );

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

/* ---- release ---- */

void sound_release_source(sound_source_t *src)
{
	if (!src) return;
	if (src->channel >= 0)
	{
		ndspChnWaveBufClear(src->channel);
		g_sound.free_channel(src->channel);
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

#endif /* __3DS__ */
