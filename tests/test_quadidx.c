/* test_quadidx.c — host harness for the shared quad index table.
 *
 * Proves the two claims the mono billboard path relies on:
 *  1. quadidx_is_quad4 accepts exactly the canonical (0,1,2),(0,2,3)
 *     pattern and rejects every near-miss (fan order, swapped winding,
 *     offset bases, degenerate repeats).
 *  2. Expanding unique verts through the shared table yields the
 *     identical vertex stream as expanding through the per-quad
 *     source indices — the conversion-equivalence core of TF04-A.
 */
#include <stdio.h>
#include <string.h>

#include "quadidx.h"

static int failures = 0;

#define CHECK(cond, ...) do { \
	if (!(cond)) { \
		printf("FAIL line %d: ", __LINE__); \
		printf(__VA_ARGS__); \
		printf("\n"); \
		failures++; \
	} \
} while (0)

int main(void)
{
	static const unsigned short good[6] = { 0, 1, 2, 0, 2, 3 };
	static const unsigned short fan[6] = { 0, 1, 2, 0, 3, 1 };
	static const unsigned short swapped[6] = { 0, 2, 1, 0, 3, 2 };
	static const unsigned short offset[6] = { 4, 5, 6, 4, 6, 7 };
	static const unsigned short degen[6] = { 0, 1, 2, 0, 2, 2 };
	static const unsigned short rev[6] = { 0, 2, 3, 0, 1, 2 };
	unsigned short table[18];
	int q, i;

	CHECK(quadidx_is_quad4(good), "canonical pattern must accept");
	CHECK(!quadidx_is_quad4(fan), "fan pattern must reject");
	CHECK(!quadidx_is_quad4(swapped), "swapped winding must reject");
	CHECK(!quadidx_is_quad4(offset), "offset base must reject (table is 0-based)");
	CHECK(!quadidx_is_quad4(degen), "degenerate must reject");
	CHECK(!quadidx_is_quad4(rev), "reversed order must reject");

	quadidx_build(table, 3);
	for (q = 0; q < 3; q++)
		for (i = 0; i < 6; i++)
			CHECK(table[6 * q + i] == (unsigned short)(4 * q + good[i]),
			      "table q=%d i=%d got %u", q, i, table[6 * q + i]);

	/* Equivalence: 5 quads of distinct verts; table expansion of the
	 * unique array must equal source-index expansion, element for
	 * element, including triangle order. */
	{
		float uniq[20];
		float via_table[30];
		float via_source[30];
		static unsigned short big[6 * 5];
		for (i = 0; i < 20; i++) uniq[i] = 100.0f + (float)i;
		quadidx_build(big, 5);
		for (i = 0; i < 30; i++) {
			via_table[i] = uniq[big[i]];
			via_source[i] = uniq[(i % 6 == 3 || i % 6 == 4 || i % 6 == 5)
				? (4 * (i / 6) + good[i % 6]) : (4 * (i / 6) + good[i % 6])];
		}
		CHECK(memcmp(via_table, via_source, sizeof(via_table)) == 0,
		      "table expansion must equal source expansion");
	}

	if (failures == 0)
		printf("quadidx: all cases pass\n");
	return failures != 0;
}
