/* music_openal.cpp -- music.h for the PC: .dsp tracks decoded into OpenAL
 *
 * Plays the same DSP-ADPCM trackNN.dsp files the 3DS streams to its DSP
 * (looked up in romfs/music, data/music, then music), decoded on the CPU by
 * the shared dsp_adpcm module. music_play decodes the whole track to 16-bit
 * mono PCM at 32 kHz (about 20 MB for five minutes) and loops it as a single
 * OpenAL buffer and source, so music_update has nothing to do. It plays on
 * whatever OpenAL context sound_openal.cpp made current; with no context
 * (sound not initialised) music_play does nothing.
 */

#ifndef __3DS__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <AL/al.h>
#include <AL/alc.h>

#include "game/main.h"
#include "util/util.h"
#include "audio/music.h"
#include "data/file.h"
#include "audio/dsp_adpcm.h"
#include "audio/music_table.h"

constexpr int MUSIC_SAMPLE_RATE = 32000;

static bool  s_initialized = false;
static bool  s_playing = false;
static int   s_current_track = -1;
static float s_volume = 1.0f;
static ALuint s_source = 0;
static ALuint s_buffer = 0;


static bool load_track_pcm(int track, int16_t **out_pcm, long *out_samples)
{
	char path[128];
	FILE *f = NULL;
	const char *dirs[] = { "romfs/music", "data/music", "music" };
	for (unsigned i = 0; i < sizeof(dirs) / sizeof(dirs[0]) && !f; i++) {
		snprintf(path, sizeof(path), "%s/track%02d.dsp", dirs[i], track);
		f = file_open(path, "rb");
	}
	if (!f) { DebugPrintf("music: cannot open track %02d .dsp\n", track); return false; }

	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (sz <= 96) { fclose(f); return false; }
	uint8_t *data = (uint8_t *)malloc((size_t)sz);
	if (!data) { fclose(f); return false; }
	if (fread(data, 1, (size_t)sz, f) != (size_t)sz) { free(data); fclose(f); return false; }
	fclose(f);

	dsp_header_t hdr;
	dsp_state_t st;
	dsp_parse_header(data, &hdr);
	dsp_state_init(&st, &hdr);

	long frames = (sz - 96) / 8;
	int16_t *pcm = (int16_t *)malloc((size_t)frames * 14 * sizeof(int16_t));
	if (!pcm) { free(data); return false; }
	dsp_decode(data + 96, (uint32_t)frames, hdr.coefs, &st, pcm);
	free(data);

	*out_pcm = pcm;
	*out_samples = frames * 14;
	DebugPrintf("music: decoded track %02d (%ld samples, %.1fs)\n",
				track, *out_samples, *out_samples / (double)MUSIC_SAMPLE_RATE);
	return true;
}

bool music_init(void)
{
	s_initialized = true;
	return true;
}

void music_shutdown(void)
{
	music_stop();
	s_initialized = false;
}

void music_stop(void)
{
	if (s_source) {
		alSourceStop(s_source);
		alDeleteSources(1, &s_source);
		s_source = 0;
	}
	if (s_buffer) {
		alDeleteBuffers(1, &s_buffer);
		s_buffer = 0;
	}
	s_playing = false;
	s_current_track = -1;
}

void music_play(int track)
{
	if (!s_initialized) return;
	if (track < MUSIC_FIRST_TRACK || track > MUSIC_LAST_TRACK) return;
	if (s_current_track == track && s_playing) return;
	if (!alcGetCurrentContext()) return;   /* SFX system not up: stay silent */

	int16_t *pcm = NULL;
	long samples = 0;
	if (!load_track_pcm(track, &pcm, &samples)) return;

	music_stop();

	alGenBuffers(1, &s_buffer);
	alBufferData(s_buffer, AL_FORMAT_MONO16, pcm,
				 (ALsizei)(samples * sizeof(int16_t)), MUSIC_SAMPLE_RATE);
	free(pcm);

	alGenSources(1, &s_source);
	alSourcei(s_source, AL_BUFFER, (ALint)s_buffer);
	alSourcei(s_source, AL_LOOPING, AL_TRUE);
	alSourcef(s_source, AL_GAIN, s_volume);
	alSourcePlay(s_source);

	s_current_track = track;
	s_playing = true;
}

void music_play_for_level(int level_num)
{
	if (level_num < 0) return;
	music_play(music_track_for_level(level_num));
}

void music_play_title(void) { music_play(MUSIC_TITLE_TRACK); }
void music_update(void) { /* whole-track buffer: nothing to refill */ }

#endif /* !__3DS__ */
