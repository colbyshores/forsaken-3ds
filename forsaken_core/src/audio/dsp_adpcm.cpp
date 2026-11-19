/* dsp_adpcm.cpp -- DSP-ADPCM header parse and decode; see dsp_adpcm.h. */
#include "audio/dsp_adpcm.h"
#include <stddef.h>

namespace {

uint16_t rd16(const uint8_t *p)
{
	return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

/* DSP-ADPCM predictor decoder. Owns the coefficient table and the
 * two-sample predictor history; one instance decodes one stream
 * from its header-initialised state. No allocation, no I/O. */
struct DspDecoder {
	uint16_t coefs[16];
	int32_t  hist1;   /* newest decoded sample */
	int32_t  hist2;   /* previous decoded sample */

	void init(const dsp_header_t &hdr)
	{
		for (int i = 0; i < 16; i++) coefs[i] = hdr.coefs[i];
		hist1 = hdr.initial_hist1;
		hist2 = hdr.initial_hist2;
	}

	/* Decode num_frames 8-byte frames; out (may be nullptr to
	 * advance state only) receives num_frames*14 s16 samples. */
	void decode(const uint8_t *frames, uint32_t num_frames, int16_t *out)
	{
		for (uint32_t f = 0; f < num_frames; f++) {
			const uint8_t *frame = frames + (size_t)f * 8;
			const uint8_t predictor = (frame[0] >> 4) & 0x07;
			const uint8_t scale = frame[0] & 0x0F;
			const int32_t c1 = (int16_t)coefs[predictor * 2 + 0];
			const int32_t c2 = (int16_t)coefs[predictor * 2 + 1];
			for (int n = 0; n < 14; n++) {
				const int byte_idx = 1 + (n >> 1);
				int32_t nibble = (n & 1) ? (frame[byte_idx] & 0x0F)
					: ((frame[byte_idx] >> 4) & 0x0F);
				if (nibble & 0x08) nibble |= (int32_t)0xFFFFFFF0;
				const int32_t sample = nibble << scale;
				const int32_t prediction = (hist1 * c1 + hist2 * c2) >> 11;
				int32_t decoded = prediction + sample;
				if (decoded > 32767) decoded = 32767;
				else if (decoded < -32768) decoded = -32768;
				hist2 = hist1;
				hist1 = decoded;
				if (out) out[f * 14 + n] = (int16_t)decoded;
			}
		}
	}
};

} // namespace

bool dsp_parse_header(const uint8_t hdr[96], dsp_header_t *out)
{
	if (!hdr || !out) return false;
	for (int i = 0; i < 16; i++)
		out->coefs[i] = rd16(hdr + 28 + i * 2);
	out->initial_index = rd16(hdr + 62);
	out->initial_hist1 = (int16_t)rd16(hdr + 64);
	out->initial_hist2 = (int16_t)rd16(hdr + 66);
	return true;
}

void dsp_state_init(dsp_state_t *st, const dsp_header_t *hdr)
{
	st->hist1 = hdr->initial_hist1;
	st->hist2 = hdr->initial_hist2;
}

void dsp_decode(const uint8_t *frames, uint32_t num_frames,
				const uint16_t *coefs, dsp_state_t *st, int16_t *out)
{
	DspDecoder dec;
	for (int i = 0; i < 16; i++) dec.coefs[i] = coefs[i];
	dec.hist1 = st->hist1;
	dec.hist2 = st->hist2;
	dec.decode(frames, num_frames, out);
	st->hist1 = dec.hist1;
	st->hist2 = dec.hist2;
}
