/*
 * music_openal.c - CD audio music playback for the PC build.
 *
 * Plays the same Nintendo DSP-ADPCM (.dsp) track files the 3DS
 * streams to its hardware decoder (romfs/music/trackNN.dsp), decoded
 * in software by the shared dsp_adpcm module and presented through
 * OpenAL. One asset set, identical music on both platforms; only
 * the decode/output layer differs (ndsp hardware vs CPU + OpenAL).
 *
 * The whole track is decoded to PCM at play time (a 5-minute track
 * is ~20 MB at 32 kHz mono s16) and looped as a single OpenAL
 * buffer. Playback uses whichever OpenAL context the SFX system
 * (sound_openal.c) has made current; if sound is not initialised,
 * music calls are silent no-ops.
 */

#ifndef __3DS__
#ifdef SOUND_SUPPORT

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <AL/al.h>
#include <AL/alc.h>

#include "main.h"
#include "util.h"
#include "music.h"
#include "file.h"
#include "dsp_adpcm.h"

#define MUSIC_SAMPLE_RATE 32000

static bool  s_initialized = false;
static bool  s_playing = false;
static int   s_current_track = -1;
static float s_volume = 1.0f;
static ALuint s_source = 0;
static ALuint s_buffer = 0;

/* Level->track mapping: keep in sync with music_3ds.c. The tables
 * live in both backends today because the 3DS file is ndsp-typed;
 * the mapping itself is data, duplicated deliberately rather than
 * coupling the 3DS TU to this one. */
#ifdef EDITION_REMASTER
static const int s_level_track[] = {
     5, 9, 6, 3, 2, 13, 8, 10, 7, 4, 12, 9, 3, 15, 7, 8,
     10, 5, 14, 4, 11, 2, 3, 6, 16, 12, 13, 11, 18, 15, 7, 8
};
#else
static const int s_level_track[] = {
    5, 9, 6, 3, 2, 8, 4, 9, 3, 7, 8, 10, 5, 3, 6
};
#endif
#define NUM_LEVEL_TRACKS (int)(sizeof(s_level_track) / sizeof(s_level_track[0]))

static bool load_track_pcm(int track, int16_t **out_pcm, long *out_samples)
{
    char path[128];
    FILE *f = NULL;
    const char *dirs[] = { "romfs/music", "Data/music", "music" };
    for (unsigned i = 0; i < sizeof(dirs) / sizeof(dirs[0]) && !f; i++) {
        snprintf(path, sizeof(path), "%s/track%02d.dsp", dirs[i], track);
        f = file_open(path, "rb");
    }
    if (!f) { DebugPrintf("music: cannot open track %02d .dsp\n", track); return false; }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 96) { fclose(f); return false; }
    uint8_t *data = malloc((size_t)sz);
    if (fread(data, 1, (size_t)sz, f) != (size_t)sz) { free(data); fclose(f); return false; }
    fclose(f);

    dsp_header_t hdr;
    dsp_state_t st;
    dsp_parse_header(data, &hdr);
    dsp_state_init(&st, &hdr);

    long frames = (sz - 96) / 8;
    int16_t *pcm = malloc((size_t)frames * 14 * sizeof(int16_t));
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

bool music_is_playing(void)
{
    if (!s_playing || !s_source) return false;
    ALint state = 0;
    alGetSourcei(s_source, AL_SOURCE_STATE, &state);
    return state == AL_PLAYING;
}

void music_set_volume(float vol)
{
    if (vol < 0.0f) vol = 0.0f;
    if (vol > 1.0f) vol = 1.0f;
    s_volume = vol;
    if (s_source) alSourcef(s_source, AL_GAIN, vol);
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
void music_update(void) { /* whole-track buffer: nothing to refill */ }

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

#endif
#endif /* !__3DS__ */
