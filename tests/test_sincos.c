/* test_sincos.c — host harness for fast_sincosf (R15, new3d.h).
 *
 * Links the REAL new3d.h inline code (via tests/trigshim/main.h, which
 * satisfies the header's main.h include) against a table built by the
 * exact trig_table_init formula. Proves:
 *   1. sin output is bit-identical to fast_sinf (same op order).
 *   2. cos output stays within the stated absolute bound of fast_cosf
 *      across the game domain (|rad| <= 10 covers model/billboard
 *      angles after D2R; wider bands checked looser for accumulators).
 *
 * Absolute error is the honest metric here, not ULPs: near a cosine
 * zero-crossing any nonzero delta is huge in ULPs while negligible
 * in absolute terms (and far below the LUT's own 4.7e-6 floor).
 */
#include <math.h>
#include <stdio.h>

#include "new3d.h"

float fast_sin_table[TRIG_TABLE_SIZE + 1];

static int failures = 0;

#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL: "); printf(__VA_ARGS__); \
        putchar('\n'); failures++; } \
} while (0)

static void sweep(float lo, float hi, int n, float bound, const char *tag)
{
    float worst = 0.0f;
    for (int k = 0; k < n; k++) {
        float rad = lo + (hi - lo) * ((float)k / (float)(n - 1));
        float s, c;
        fast_sincosf(rad, &s, &c);
        CHECK(s == fast_sinf(rad), "%s sin not bit-identical at %g", tag, rad);
        {
            float d = fabsf(c - fast_cosf(rad));
            if (d > worst)
                worst = d;
            CHECK(d <= bound, "%s cos err %g exceeds %g at rad=%g",
                  tag, d, bound, rad);
        }
    }
    printf("%s: worst cos abs err %.3g (bound %.3g)\n", tag, worst, bound);
    if (failures)
        return;
}

int main(void)
{
    int i;
    for (i = 0; i <= TRIG_TABLE_SIZE; i++)
        fast_sin_table[i] = sinf((float)i
            * (2.0f * 3.14159265358979323846f / TRIG_TABLE_SIZE));
    /* Game domain: model/billboard angles after degree->radian. */
    sweep(-10.0f, 10.0f, 200001, 1e-6f, "game-domain");
    /* Boundary neighborhoods: wrap points, quarter turns, near-zero. */
    sweep(-3.14159265f - 0.01f, -3.14159265f + 0.01f, 2001, 1e-6f, "wrap-pi");
    sweep(1.57079633f - 0.01f, 1.57079633f + 0.01f, 2001, 1e-6f, "quarter");
    sweep(-0.01f, 0.01f, 2001, 1e-6f, "near-zero");
    /* Wider accumulator range, looser bound (input rounding dominates). */
    sweep(-1000.0f, 1000.0f, 200001, 1e-4f, "wide");
    if (failures == 0)
        printf("test_sincos: all cases pass\n");
    return failures != 0;
}
