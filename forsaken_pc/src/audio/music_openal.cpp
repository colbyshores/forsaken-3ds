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

namespace {

constexpr int MUSIC_SAMPLE_RATE = 32000;

bool load_track_pcm(int track, int16_t **out_pcm, long *out_samples)
{
	char path[128];
	FILE *f = nullptr;
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

/* The one track playing (one instance, g_music): its OpenAL buffer and
 * source, which track it is, and whether music_init has run. */
class MusicPlayer {
	public:
	bool init();
	void shutdown();
	void play(int track);
	void stop();

	private:
	bool   initialized = false;
	bool   playing = false;
	int    current_track = -1;
	float  volume = 1.0f;
	ALuint source = 0;
	ALuint buffer = 0;
};

bool MusicPlayer::init()
{
	initialized = true;
	return true;
}

void MusicPlayer::shutdown()
{
	stop();
	initialized = false;
}

void MusicPlayer::stop()
{
	if (source) {
		alSourceStop(source);
		alDeleteSources(1, &source);
		source = 0;
	}
	if (buffer) {
		alDeleteBuffers(1, &buffer);
		buffer = 0;
	}
	playing = false;
	current_track = -1;
}

void MusicPlayer::play(int track)
{
	if (!initialized) return;
	if (track < MUSIC_FIRST_TRACK || track > MUSIC_LAST_TRACK) return;
	if (current_track == track && playing) return;
	if (!alcGetCurrentContext()) return;   /* SFX system not up: stay silent */

	int16_t *pcm = nullptr;
	long samples = 0;
	if (!load_track_pcm(track, &pcm, &samples)) return;

	stop();

	alGenBuffers(1, &buffer);
	alBufferData(buffer, AL_FORMAT_MONO16, pcm,
				 (ALsizei)(samples * sizeof(int16_t)), MUSIC_SAMPLE_RATE);
	free(pcm);

	alGenSources(1, &source);
	alSourcei(source, AL_BUFFER, (ALint)buffer);
	alSourcei(source, AL_LOOPING, AL_TRUE);
	/* at the listener, like every sfx source: a world-positioned mono
	 * source would be panned and attenuated as the ship moves */
	alSourcei(source, AL_SOURCE_RELATIVE, AL_TRUE);
	alSourcef(source, AL_GAIN, volume);
	alSourcePlay(source);

	current_track = track;
	playing = true;
}

MusicPlayer g_music;

} // namespace

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
void music_update(void) { /* whole-track buffer: nothing to refill */ }

#endif /* !__3DS__ */
