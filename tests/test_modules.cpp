/* test_modules.cpp - host harness for the first C-with-classes
 * conversions (AGENTS.md S9): dsp_adpcm.cpp and hunk_alloc.cpp,
 * linked as the real objects and exercised through their flat C
 * APIs exactly as the game backends call them.
 *
 * dsp: a synthetic .dsp frame with a known predictor/scale must
 * decode to the hand-computed sample sequence, and state-only
 * advance must land on the same history as a storing decode.
 * hunk: two-tag bump semantics - startup grows up, level grows
 * down, free_all resets only its tag, overflow fails cleanly. */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "audio/dsp_adpcm.h"
#include "util/hunk_alloc.h"

static int failures = 0;
#define CHECK(cond, ...) do { \
	if (!(cond)) { printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); failures++; } \
} while (0)

static void test_dsp(void)
{
	/* Header: coefs all zero except predictor 0 = (2048, 0), so the
	 * prediction term vanishes and samples are pure nibble<<scale.
	 * Frame: predictor 0, scale 1; nibbles +1,-1,+2,-2,... */
	uint8_t hdr[96];
	memset(hdr, 0, sizeof hdr);
	hdr[28] = 0x08; hdr[29] = 0x00;   /* coef0 = 2048 (BE) */
	dsp_header_t dh;
	CHECK(dsp_parse_header(hdr, &dh), "header parse");
	CHECK(dh.coefs[0] == 2048 && dh.coefs[1] == 0, "coef byteswap");

	dsp_state_t st;
	dsp_state_init(&st, &dh);
	const uint8_t frame[8] = { 0x01, 0x12, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00 };
	int16_t out[14];
	dsp_decode(frame, 1, dh.coefs, &st, out);
	/* nibbles 1,2,3,-1 (scale 1: 2,4,6,-2); the predictor adds
	 * the previous sample each time: 2, 2+4, 6+6, 12-2, 10+0 */
	const int16_t want[5] = { 2, 6, 12, 10, 10 };
	for (int i = 0; i < 5; i++)
		CHECK(out[i] == want[i], "sample %d: got %d want %d", i, out[i], want[i]);

	/* state-only advance must reach the same history */
	dsp_state_t st2;
	dsp_state_init(&st2, &dh);
	dsp_decode(frame, 1, dh.coefs, &st2, nullptr);
	CHECK(st.hist1 == st2.hist1 && st.hist2 == st2.hist2, "state-only advance");
}

static void test_hunk(void)
{
	CHECK(Hunk_Init(1024), "hunk init");
	void *a = Hunk_Alloc(TAG_STARTUP, 100);
	void *b = Hunk_Alloc(TAG_LEVEL, 200);
	CHECK(a != nullptr && b != nullptr, "allocs succeed");
	CHECK(((uintptr_t)a & 7) == 0 && ((uintptr_t)b & 7) == 0, "8-byte alignment");
	const unsigned char *z = (const unsigned char *)a;
	bool zeroed = true;
	for (int i = 0; i < 100; i++) if (z[i]) zeroed = false;
	CHECK(zeroed, "allocations are zeroed");
	CHECK(Hunk_Alloc(TAG_LEVEL, 100000) == nullptr, "overflow fails cleanly");
	/* 104 startup + 200 level used: 900 more level bytes do not fit */
	memset(a, 0x5a, 100);
	CHECK(Hunk_Alloc(TAG_LEVEL, 900) == nullptr, "level arena full before free_all");
	Hunk_FreeAll(TAG_LEVEL);
	CHECK(Hunk_Alloc(TAG_LEVEL, 900) != nullptr, "free_all releases the level arena");
	bool kept = true;
	for (int i = 0; i < 100; i++) if (z[i] != 0x5a) kept = false;
	CHECK(kept, "free_all leaves the startup arena alone");
}

int main(void)
{
	test_dsp();
	test_hunk();
	if (failures) { printf("test_modules: %d FAILURES\n", failures); return 1; }
	printf("test_modules: all ok\n");
	return 0;
}
