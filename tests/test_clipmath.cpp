/* test_clipmath.cpp — host harness for the near-plane portal clipper.
 *
 * Tests the real world/clipmath.h logic (no engine deps): for synthetic
 * clip-space polygons, the SH bounds must contain the densely-sampled
 * true visible region (soundness: never cull wrongly) and stay close
 * to it (tightness: no slab explosions, no runaway bounds).
 *
 * Reference is independent direct projection of dense interior samples
 * (no shared clipping code; only the documented viewport mapping).
 */
#include <math.h>
#include <stdio.h>

#include "world/clipmath.h"

static int failures = 0;

#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL: "); printf(__VA_ARGS__); \
        putchar('\n'); failures++; } \
} while (0)

/* Viewport mapping, mirroring Transform2Viewport (checked separately
 * by hardware A/B, which exercises the real one). */
static float mapx(float nx, float vpx, float vpw, float vpsx)
{
    if (nx < -1.0f)
        return vpx;
    if (nx > 1.0f)
        return vpx + vpw;
    return vpx + vpw * 0.5f + vpsx * nx;
}

static float mapy(float ny, float vpy, float vph, float vpsy)
{
    if (ny < -1.0f)
        return vpy + vph;
    if (ny > 1.0f)
        return vpy;
    return vpy + vph * 0.5f - vpsy * ny;
}

/* Brute-force reference: dense barycentric sampling of each triangle fan,
 * projecting samples with w>1e-3 and z>=0 directly. */
static void reference_bounds(const float *clip, int n,
                             float vpx, float vpy, float vpw, float vph,
                             float vpsx, float vpsy,
                             float *mnx, float *mny, float *mxx, float *mxy,
                             int *any)
{
    const int D = 40;
    int a, b, c, i, j;
    *mnx = *mny = 1e30f;
    *mxx = *mxy = -1e30f;
    *any = 0;
    for (a = 1; a < n - 1; a++) {
        for (i = 0; i <= D; i++) {
            for (j = 0; j <= D - i; j++) {
                float w0 = 1.0f - (float)(i + j) / D;
                float w1 = (float)i / D, w2 = (float)j / D;
                int v[3] = {0, a, a + 1};
                float bw[3] = {w0, w1, w2};
                float x = 0, y = 0, z = 0, w = 0;
                for (c = 0; c < 3; c++) {
                    x += bw[c] * clip[v[c] * 4 + 0];
                    y += bw[c] * clip[v[c] * 4 + 1];
                    z += bw[c] * clip[v[c] * 4 + 2];
                    w += bw[c] * clip[v[c] * 4 + 3];
                }
                if (w > 1e-4f && z >= 0.0f) {
                    float px = mapx(x / w, vpx, vpw, vpsx);
                    float py = mapy(y / w, vpy, vph, vpsy);
                    if (px < *mnx)
                        *mnx = px;
                    if (py < *mny)
                        *mny = py;
                    if (px > *mxx)
                        *mxx = px;
                    if (py > *mxy)
                        *mxy = py;
                    *any = 1;
                }
            }
        }
    }
}

static void check_case(const char *tag, const float *clip, int n,
                       float vpx, float vpy, float vpw, float vph,
                       float vpsx, float vpsy)
{
    float mnx, mny, mxx, mxy, rmnx, rmny, rmxx, rmxy;
    int any, ok;
    ok = fillclip_tight_bounds(clip, n, vpx, vpy, vpw, vph, vpsx, vpsy,
                               &mnx, &mny, &mxx, &mxy);
    reference_bounds(clip, n, vpx, vpy, vpw, vph, vpsx, vpsy,
                     &rmnx, &rmny, &rmxx, &rmxy, &any);
    if (!any) {
        /* Empty true region: either answer (fallback or empty bounds)
         * is acceptable; must not crash and must not claim coverage. */
        if (ok)
            CHECK(mnx > mxx || mny > mxy, "%s: empty region but nonempty bounds", tag);
        printf("%s: empty region, algo=%d ok\n", tag, ok);
        return;
    }
    CHECK(ok, "%s: nonempty region but algo fell back", tag);
    if (!ok)
        return;
    /* Soundness: algo bounds must contain the reference (1px slack for
     * sampling density). */
    CHECK(mnx <= rmnx + 1.0f && mny <= rmny + 1.0f, "%s: min not contained", tag);
    CHECK(mxx >= rmxx - 1.0f && mxy >= rmxy - 1.0f, "%s: max not contained", tag);
    /* Tightness: algo area within 2x of reference area (slab explosions
     * would exceed this by orders of magnitude). */
    {
        float aa = (mxx - mnx) * (mxy - mny);
        float ra = (rmxx - rmnx) * (rmxy - rmny);
        if (ra > 100.0f)
            CHECK(aa <= ra * 2.0f + 100.0f, "%s: too loose (%.0f vs %.0f)",
                  tag, aa, ra);
        printf("%s: ref area=%.0f algo area=%.0f\n", tag, ra, aa);
    }
}

static float rndf(unsigned *seed, float lo, float hi)
{
    *seed = *seed * 1103515245u + 12345u;
    return lo + (hi - lo) * (float)(*seed >> 8) / (float)(1u << 24);
}

int main(void)
{
    /* Ordinary straddle: quad crossing the near plane, one vert behind. */
    static const float straddle[4 * 4] = {
        -0.5f, -0.5f, 1.0f, 2.0f,
        0.5f, -0.5f, 1.0f, 2.0f,
        0.5f, 0.5f, -1.0f, -0.5f,
        -0.5f, 0.5f, 1.0f, 2.0f,
    };
    /* Deep straddle: half the quad behind. */
    static const float half[4 * 4] = {
        -2.0f, -2.0f, 2.0f, 3.0f,
        2.0f, -2.0f, 2.0f, 3.0f,
        2.0f, 2.0f, -2.0f, -1.0f,
        -2.0f, 2.0f, -2.0f, -1.0f,
    };
    /* Fully in front (ordinary portal). */
    static const float front[4 * 4] = {
        -0.5f, -0.5f, 1.0f, 2.0f,
        0.5f, -0.5f, 1.0f, 2.0f,
        0.5f, 0.5f, 1.0f, 2.0f,
        -0.5f, 0.5f, 1.0f, 2.0f,
    };
    /* Fully behind (invisible): must fall back. */
    static const float behind[4 * 4] = {
        -0.5f, -0.5f, -1.0f, -2.0f,
        0.5f, -0.5f, -1.0f, -2.0f,
        0.5f, 0.5f, -1.0f, -2.0f,
        -0.5f, 0.5f, -1.0f, -2.0f,
    };
    /* Grazing: sliver with one vert marginally behind. */
    static const float graze[4 * 4] = {
        -0.05f, -0.05f, 0.5f, 1.0f,
        0.05f, -0.05f, 0.5f, 1.0f,
        0.05f, 0.05f, -0.02f, -0.01f,
        -0.05f, 0.05f, 0.5f, 1.0f,
    };
    /* Degenerate + over-capacity inputs must fall back, never crash. */
    static const float tri[3 * 4] = {
        -0.5f, -0.5f, 1.0f, 2.0f,
        0.5f, -0.5f, 1.0f, -0.5f,
        0.0f, 0.5f, 1.0f, 2.0f,
    };
    check_case("straddle", straddle, 4, 0, 0, 400, 240, 200, 120);
    check_case("half", half, 4, 0, 0, 400, 240, 200, 120);
    check_case("front", front, 4, 0, 0, 400, 240, 200, 120);
    check_case("behind", behind, 4, 0, 0, 400, 240, 200, 120);
    check_case("graze", graze, 4, 0, 0, 400, 240, 200, 120);
    check_case("tri-straddle", tri, 3, 0, 0, 400, 240, 200, 120);
    {
        /* Over-capacity: must fall back. */
        float big[40 * 4];
        int i, ok;
        float a, b, c, d;
        for (i = 0; i < 40; i++) {
            big[i * 4 + 0] = 0.1f;
            big[i * 4 + 1] = 0.1f;
            big[i * 4 + 2] = 1.0f;
            big[i * 4 + 3] = 2.0f;
        }
        ok = fillclip_tight_bounds(big, 40, 0, 0, 400, 240, 200, 120,
                                   &a, &b, &c, &d);
        CHECK(!ok, "over-capacity should fall back");
        ok = fillclip_tight_bounds(big, 2, 0, 0, 400, 240, 200, 120,
                                   &a, &b, &c, &d);
        CHECK(!ok, "degenerate should fall back");
    }
    /* Fuzz: random CONVEX quads (real portals are convex doorway
     * polys; bowties/concave would be ill-formed input for both the
     * reference fan and the clipper). Soundness + tightness. */
    {
        unsigned seed = 12345;
        int t, bad = 0;
        for (t = 0; t < 300; t++) {
            float q[16], angs[4], cx, cy, csc;
            int i, k, fails_before = failures;
            for (i = 0; i < 4; i++) {
                seed = seed * 1103515245u + 12345u;
                angs[i] = (float)(seed >> 8) / (float)(1u << 24);
            }
            /* sort by angle; unit circle => strictly convex (varying
             * radii make star-shaped-but-concave quads, which are
             * ill-formed input for both the fan reference and SH). */
            for (i = 0; i < 4; i++)
                for (k = i + 1; k < 4; k++)
                    if (angs[k] < angs[i]) {
                        float tmp = angs[i];
                        angs[i] = angs[k];
                        angs[k] = tmp;
                    }
            cx = rndf(&seed, -2.0f, 2.0f);
            cy = rndf(&seed, -2.0f, 2.0f);
            csc = rndf(&seed, 0.5f, 4.0f);
            /* Planar graph: z,w linear in x,y (portal faces are planar). */
            float az = rndf(&seed, -1.0f, 1.0f);
            float bz = rndf(&seed, -1.0f, 1.0f);
            float c0z = rndf(&seed, -3.0f, 3.0f);
            float aw = rndf(&seed, -1.0f, 1.0f);
            float bw = rndf(&seed, -1.0f, 1.0f);
            float c0w = rndf(&seed, -3.0f, 3.0f);
            for (i = 0; i < 4; i++) {
                float a = angs[i] * 6.2831853f;
                float x = cx + csc * cosf(a);
                float y = cy + csc * sinf(a);
                q[i * 4 + 0] = x;
                q[i * 4 + 1] = y;
                q[i * 4 + 2] = az * x + bz * y + c0z;
                q[i * 4 + 3] = aw * x + bw * y + c0w;
            }
            /* Keep w away from exact 0 (measure-zero knife edge). */
            for (i = 0; i < 4; i++)
                if (q[i * 4 + 3] > -0.05f && q[i * 4 + 3] < 0.05f)
                    q[i * 4 + 3] = 0.05f;
            check_case("fuzz", q, 4, 0, 0, 400, 240, 200, 120);
            if (failures != fails_before) {
                int d;
                printf(" failing quad:");
                for (d = 0; d < 4; d++)
                    printf(" [%.3f %.3f %.3f %.3f]", q[d*4], q[d*4+1], q[d*4+2], q[d*4+3]);
                printf("\n");
                bad++;
                if (bad > 2) break;
            }
        }
        printf("fuzz: %d/300 cases with findings\n", bad);
    }
    if (failures == 0)
        printf("test_clipmath: all cases pass\n");
    return failures != 0;
}
