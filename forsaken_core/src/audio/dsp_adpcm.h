#pragma once

/* dsp_adpcm.h -- shared Nintendo DSP-ADPCM (.dsp) header parsing and decode
 *
 * One set of music files serves both platforms: music_3ds.cpp streams the
 * .dsp tracks to the ndsp hardware decoder, music_openal.cpp decodes them
 * in software. This module holds the parts that must agree bit-for-bit:
 * the big-endian header layout, the per-file coefficient table, and the
 * predictor decode math. Stateless apart from the caller's dsp_state_t; no
 * allocation, no I/O.
 *
 * A .dsp file is a 96-byte header followed by 8-byte frames; each frame
 * carries a predictor/scale byte and 14 4-bit samples. The 3DS hands each
 * ndsp buffer its own start context, so it runs dsp_decode with out == NULL
 * to carry the predictor history from one buffer to the next.
 */

#include <stdint.h>
#include <stdbool.h>


struct dsp_header_t {
	uint16_t coefs[16];       /* predictor coefficients (host order) */
	int16_t  initial_hist1;   /* newest history sample at stream start */
	int16_t  initial_hist2;   /* previous history sample */
	uint16_t initial_index;
};

struct dsp_state_t {
	int32_t hist1;            /* newest decoded sample */
	int32_t hist2;            /* previous decoded sample */
};

/* Parse the 96-byte .dsp header (big-endian on disk). Fails only on NULL
 * arguments; the header contents are not validated. */
bool dsp_parse_header(const uint8_t hdr[96], dsp_header_t *out);

/* Initialise decode state from a parsed header. */
void dsp_state_init(dsp_state_t *st, const dsp_header_t *hdr);

/* Decode num_frames 8-byte frames. out receives num_frames*14 mono
 * s16 samples; pass NULL to advance the predictor state only (the
 * 3DS uses this to track per-buffer hardware decode context). */
void dsp_decode(const uint8_t *frames, uint32_t num_frames,
				const uint16_t *coefs, dsp_state_t *st, int16_t *out);
