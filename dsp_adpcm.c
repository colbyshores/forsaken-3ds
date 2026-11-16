#include "dsp_adpcm.h"
#include <stddef.h>

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

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
    int32_t hist1 = st->hist1;
    int32_t hist2 = st->hist2;
    for (uint32_t f = 0; f < num_frames; f++) {
        const uint8_t *frame = frames + (size_t)f * 8;
        uint8_t header = frame[0];
        uint8_t predictor = (header >> 4) & 0x07;
        uint8_t scale = header & 0x0F;
        int32_t c1 = (int16_t)coefs[predictor * 2 + 0];
        int32_t c2 = (int16_t)coefs[predictor * 2 + 1];
        for (int n = 0; n < 14; n++) {
            int byte_idx = 1 + (n >> 1);
            int32_t nibble = (n & 1) ? (frame[byte_idx] & 0x0F)
                                     : ((frame[byte_idx] >> 4) & 0x0F);
            if (nibble & 0x08) nibble |= (int32_t)0xFFFFFFF0;
            int32_t sample = nibble << scale;
            int32_t prediction = (hist1 * c1 + hist2 * c2) >> 11;
            int32_t decoded = prediction + sample;
            if (decoded > 32767) decoded = 32767;
            else if (decoded < -32768) decoded = -32768;
            hist2 = hist1;
            hist1 = decoded;
            if (out) out[f * 14 + n] = (int16_t)decoded;
        }
    }
    st->hist1 = hist1;
    st->hist2 = hist2;
}
