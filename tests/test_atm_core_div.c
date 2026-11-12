/* test_atm_core_div.c — host harness for the R11 cold-division fixes.
 *
 * Two once-per-run/dump-time sites in atm_core.c used integer division by a
 * runtime value (a __aeabi idiv call on ARMv6). They now divide in f64,
 * which is exact for the magnitudes involved (all values << 2^53) and
 * lowers to hardware vdiv. This test pins that exactness: every vector
 * compares the shipped f64 expression against exact u64/u32 arithmetic.
 *
 * Compiles the REAL ../atm_core.c (included for access to the static
 * round-up helper). Reference divisions below run on the host only.
 */
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../atm_core.c"

static int failures = 0;

#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL %s:%d: ", __FILE__, __LINE__); \
        printf(__VA_ARGS__); putchar('\n'); failures++; } \
} while (0)

/* Exact-u64 reference for the round-up (host division is fine here). */
static uint64_t round_up_ref(uint64_t bytes, uint32_t c) {
    if (c <= 1) return bytes;
    uint64_t cc = c;
    return (bytes + cc - 1) / cc * cc;
}

static void test_round_up(void) {
    static const struct { uint64_t bytes; uint32_t c; } v[] = {
        { 0, 512 }, { 1, 512 }, { 511, 512 }, { 512, 512 }, { 513, 512 },
        { 4096, 4096 }, { 4097, 4096 },
        { 12345, 1 }, { 12345, 0 },          /* cluster guard path */
        { 1000, 300 }, { 1000, 7 }, { 999999, 1000 },   /* non-pow2 */
        { 1000000000000ULL, 3000 },
        { (1ULL << 40) + 123, 32768 },
        { (1ULL << 53) - 1, 4096 },          /* top of exact-f64 range */
        { 0xFFFFFFFFULL, 512 }, { 0x100000000ULL, 4096 },
    };
    for (unsigned i = 0; i < sizeof(v) / sizeof(v[0]); i++) {
        uint64_t got = atm_round_up_clusters(v[i].bytes, v[i].c);
        uint64_t want = round_up_ref(v[i].bytes, v[i].c);
        CHECK(got == want, "round_up(%" PRIu64 ",%u)=%" PRIu64 " want %" PRIu64,
              v[i].bytes, v[i].c, got, want);
    }
}

static unsigned long long summary_avg(uint64_t count, uint64_t sum) {
    AtmFrameStats f;
    char out[256];
    unsigned long long avg = 0;
    atm_frame_init(&f);
    f.count = count;
    f.sum_us = sum;
    if (count) f.min_us = f.max_us = 1000;
    if (atm_frame_format_summary(&f, out, sizeof(out)) == 0)
        return 0;
    CHECK(sscanf(out, "@n3dl frames=%*u avg_us=%llu", &avg) == 1,
          "unparsable summary: %s", out);
    return avg;
}

static void test_summary_avg(void) {
    static const struct { uint64_t count, sum; } v[] = {
        { 3, 10 },                            /* truncation, not rounding */
        { 1, (1ULL << 50) },
        { 7, 1000000000000ULL },
        { 240, 240ULL * 16681 },              /* one bench window */
        { 1000000, 1000000ULL * 33333 },      /* long run, ~2^35 sum */
        { (1ULL << 32) + 1, (1ULL << 53) - 2 },
    };
    for (unsigned i = 0; i < sizeof(v) / sizeof(v[0]); i++) {
        unsigned long long got = summary_avg(v[i].count, v[i].sum);
        unsigned long long want = (unsigned long long)(v[i].sum / v[i].count);
        CHECK(got == want, "avg(%" PRIu64 "/%" PRIu64 ")=%llu want %llu",
              v[i].sum, v[i].count, got, want);
    }
    /* Zero-frame path must not divide at all. */
    {
        AtmFrameStats f;
        char out[256];
        atm_frame_init(&f);
        size_t n = atm_frame_format_summary(&f, out, sizeof(out));
        CHECK(n > 0 && strstr(out, "frames=0") != NULL,
              "zero-frame summary wrong: %s", out);
    }
    /* Realistic path through the per-frame API. */
    {
        AtmFrameStats f;
        char out[256];
        uint64_t sum = 0;
        unsigned long long avg = 0;
        atm_frame_init(&f);
        for (int i = 0; i < 240; i++) {
            uint32_t us = (uint32_t)(15000 + (i * 37) % 5000);
            atm_frame_add(&f, us, 33333);
            sum += us;
        }
        CHECK(atm_frame_format_summary(&f, out, sizeof(out)) > 0,
              "summary failed");
        CHECK(sscanf(out, "@n3dl frames=%*u avg_us=%llu", &avg) == 1,
              "unparsable summary: %s", out);
        CHECK(avg == sum / 240, "api-path avg=%llu want %llu",
              avg, (unsigned long long)(sum / 240));
    }
}

int main(void) {
    test_round_up();
    test_summary_avg();
    if (failures == 0) printf("test_atm_core_div: all cases pass\n");
    return failures != 0;
}
