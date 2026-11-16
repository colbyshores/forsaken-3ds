#ifndef DSP_ADPCM_INCLUDED
#define DSP_ADPCM_INCLUDED

/*
 * dsp_adpcm - shared Nintendo DSP-ADPCM (.dsp) decoding.
 *
 * One asset set serves every platform: the 3DS streams .dsp files to
 * the ndsp hardware decoder (music_3ds.c), the PC decodes the same
 * files in software (music_openal.c). This module holds the parts
 * that must agree bit-for-bit: the big-endian header layout, the
 * coefficient table, and the predictor decode math.
 *
 * A .dsp file is a 96-byte header followed by 8-byte frames; each
 * frame carries a predictor/scale byte and 14 4-bit samples.
 */

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t coefs[16];       /* predictor coefficients (host order) */
    int16_t  initial_hist1;   /* newest history sample at stream start */
    int16_t  initial_hist2;   /* previous history sample */
    uint16_t initial_index;
} dsp_header_t;

typedef struct {
    int32_t hist1;            /* newest decoded sample */
    int32_t hist2;            /* previous decoded sample */
} dsp_state_t;

/* Parse the 96-byte .dsp header (big-endian on disk). */
bool dsp_parse_header(const uint8_t hdr[96], dsp_header_t *out);

/* Initialise decode state from a parsed header. */
void dsp_state_init(dsp_state_t *st, const dsp_header_t *hdr);

/* Decode num_frames 8-byte frames. out receives num_frames*14 mono
 * s16 samples; pass NULL to advance the predictor state only (the
 * 3DS uses this to track per-buffer hardware decode context). */
void dsp_decode(const uint8_t *frames, uint32_t num_frames,
                const uint16_t *coefs, dsp_state_t *st, int16_t *out);

#ifdef __cplusplus
}
#endif

#endif
