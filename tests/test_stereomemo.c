/* test_stereomemo.c — host harness for the single-pass stereo memo.
 *
 * Links the REAL stereomemo.c (built with -DSTEREO_REPLAY, same as the
 * game build) and proves the table contract the renderer relies on:
 * insert-then-hit round-trips exact ranges, misses are safe (never a
 * stale range), a new frame invalidates everything, pressure degrades
 * to misses never false hits, and a shared slot refilled for a
 * different group/pass/resume (same key, different epoch) misses
 * instead of aliasing. Case 3 fails against a naive table without
 * generations; case 7 fails against a naive key-only table (false hits
 * across refills — the 2026-11-13 hardware mismatch signature).
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "stereomemo.h"

/* Epoch shorthand: pass 1 = polys-clipped, group = world group. */
#define EP(g, t) 1u, (uint32_t)(g), (uint32_t)(t)

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
	long off = -1;
	int cnt = -1;
	bool hit;

	/* 1. Record then hit: exact round-trip (same epoch). */
	smemo_begin_frame();
	smemo_record((const void *)0x1000, smemo_sub(3, 0, 0), 100, 36,
	             EP(7, 0));
	hit = smemo_lookup((const void *)0x1000, smemo_sub(3, 0, 0),
	                    &off, &cnt, EP(7, 0));
	CHECK(hit, "expected hit");
	CHECK(off == 100 && cnt == 36, "range mismatch off=%ld cnt=%d", off, cnt);

	/* 2. Unknown key misses. */
	hit = smemo_lookup((const void *)0x2000, smemo_sub(3, 0, 0),
	                    &off, &cnt, EP(7, 0));
	CHECK(!hit, "unknown ro must miss");
	hit = smemo_lookup((const void *)0x1000, smemo_sub(4, 0, 0),
	                    &off, &cnt, EP(7, 0));
	CHECK(!hit, "unknown texgroup must miss");
	hit = smemo_lookup((const void *)0x1000, smemo_sub(3, 1, 0),
	                    &off, &cnt, EP(7, 0));
	CHECK(!hit, "ortho flag is part of the key");

	/* 3. New frame invalidates everything. */
	smemo_begin_frame();
	hit = smemo_lookup((const void *)0x1000, smemo_sub(3, 0, 0),
	                    &off, &cnt, EP(7, 0));
	CHECK(!hit, "old frame must miss after begin_frame");

	/* 4. Re-record after invalidation hits again. */
	smemo_record((const void *)0x1000, smemo_sub(3, 0, 0), 200, 48,
	             EP(7, 0));
	hit = smemo_lookup((const void *)0x1000, smemo_sub(3, 0, 0),
	                    &off, &cnt, EP(7, 0));
	CHECK(hit && off == 200 && cnt == 48, "re-record must hit off=%ld cnt=%d", off, cnt);

	/* 5. Overwrite same key + same epoch: latest wins. */
	smemo_record((const void *)0x1000, smemo_sub(3, 0, 0), 300, 12,
	             EP(7, 0));
	hit = smemo_lookup((const void *)0x1000, smemo_sub(3, 0, 0),
	                    &off, &cnt, EP(7, 0));
	CHECK(hit && off == 300 && cnt == 12, "latest record must win off=%ld cnt=%d", off, cnt);

	/* 6. Pressure: 5000 distinct keys (over 4096 slots). Every hit must
	 * return exactly the last-recorded range for its key; misses are
	 * the only other allowed outcome. */
	{
		static int last_off[5000];
		static int last_cnt[5000];
		int k;
		smemo_begin_frame();
		for (k = 0; k < 5000; k++) {
			last_off[k] = 1000 + k;
			last_cnt[k] = 6 + (k % 60);
			smemo_record((const void *)(uintptr_t)(0x10000 + k * 64),
			             smemo_sub((unsigned)(k % 64), 0, 0),
			             last_off[k], last_cnt[k],
			             1u, (uint32_t)(k % 128), (uint32_t)(k % 8));
		}
		for (k = 0; k < 5000; k++) {
			hit = smemo_lookup((const void *)(uintptr_t)(0x10000 + k * 64),
			                   smemo_sub((unsigned)(k % 64), 0, 0),
			                   &off, &cnt,
			                   1u, (uint32_t)(k % 128), (uint32_t)(k % 8));
			if (hit)
				CHECK(off == last_off[k] && cnt == last_cnt[k],
				      "false hit k=%d off=%ld cnt=%d", k, off, cnt);
		}
	}

	/* 7. Alias regression: same (ro, sub), different epoch (another
	 * group refilled the shared slot) must MISS, never return the
	 * stale range. Re-recording the new epoch hits again; the old
	 * epoch then misses. */
	smemo_begin_frame();
	smemo_record((const void *)0x3000, smemo_sub(0, 0, 0), 400, 54,
	             EP(9, 0));
	hit = smemo_lookup((const void *)0x3000, smemo_sub(0, 0, 0),
	                    &off, &cnt, EP(4, 0));
	CHECK(!hit, "aliased epoch must miss, not return stale range");
	smemo_record((const void *)0x3000, smemo_sub(0, 0, 0), 500, 54,
	             EP(4, 0));
	hit = smemo_lookup((const void *)0x3000, smemo_sub(0, 0, 0),
	                    &off, &cnt, EP(4, 0));
	CHECK(hit && off == 500 && cnt == 54, "re-recorded epoch must hit off=%ld cnt=%d", off, cnt);
	hit = smemo_lookup((const void *)0x3000, smemo_sub(0, 0, 0),
	                    &off, &cnt, EP(9, 0));
	CHECK(!hit, "old epoch must miss after overwrite");

	/* 8. Resume split: same key + group, different resume index misses. */
	smemo_begin_frame();
	smemo_record((const void *)0x4000, smemo_sub(0, 0, 0), 600, 54,
	             EP(5, 0));
	hit = smemo_lookup((const void *)0x4000, smemo_sub(0, 0, 0),
	                    &off, &cnt, EP(5, 3));
	CHECK(!hit, "different resume index must miss");
	hit = smemo_lookup((const void *)0x4000, smemo_sub(0, 0, 0),
	                    &off, &cnt, EP(5, 0));
	CHECK(hit && off == 600, "same resume index must hit off=%ld", off);

	/* 9. Pass change: same key + group, different pass misses. */
	smemo_begin_frame();
	smemo_record((const void *)0x5000, smemo_sub(2, 0, 0), 700, 20,
	             1u, 11u, 0u);
	hit = smemo_lookup((const void *)0x5000, smemo_sub(2, 0, 0),
	                    &off, &cnt, 5u, 11u, 0u);
	CHECK(!hit, "different pass must miss");
	hit = smemo_lookup((const void *)0x5000, smemo_sub(2, 0, 0),
	                    &off, &cnt, 1u, 11u, 0u);
	CHECK(hit && off == 700 && cnt == 20, "same pass must hit off=%ld cnt=%d", off, cnt);

	/* 10. Mid-frame invalidation: records before the bump miss
	 * afterwards (content changed); re-recording hits again. */
	smemo_begin_frame();
	smemo_record((const void *)0x6000, smemo_sub(1, 0, 0), 800, 20,
	             3u, 9u, 0u);
	smemo_invalidate();
	hit = smemo_lookup((const void *)0x6000, smemo_sub(1, 0, 0),
	                    &off, &cnt, 3u, 9u, 0u);
	CHECK(!hit, "pre-invalidation record must miss after bump");
	smemo_record((const void *)0x6000, smemo_sub(1, 0, 0), 810, 20,
	             3u, 9u, 0u);
	hit = smemo_lookup((const void *)0x6000, smemo_sub(1, 0, 0),
	                    &off, &cnt, 3u, 9u, 0u);
	CHECK(hit && off == 810, "post-invalidation record must hit off=%ld", off);

	if (failures == 0)
		printf("stereomemo: all cases pass\n");
	return failures != 0;
}
