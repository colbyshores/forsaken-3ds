/* test_new3d.cpp - host harness for new3d.cpp (C-with-classes
 * conversion). Pins the math-core contracts: trig LUT endpoints,
 * cross/dot products on basis vectors, matrix multiply by
 * identity, and vector normalisation. */
#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>

#include "new3d.h"

static int failures = 0;
#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); failures++; } \
} while (0)

static bool closef(float a, float b) { return fabsf(a - b) < 1e-3f; }

int main(void)
{
    trig_table_init();
    CHECK(closef(fast_sinf(0.0f), 0.0f), "sin(0)");
    CHECK(closef(fast_cosf(0.0f), 1.0f), "cos(0)");
    CHECK(closef(fast_sinf((float)M_PI / 2.0f), 1.0f), "sin(pi/2) via LUT");

    VECTOR ex = { 1, 0, 0 }, ey = { 0, 1, 0 }, ez;
    CrossProduct(&ex, &ey, &ez);
    CHECK(closef(ez.x, 0) && closef(ez.y, 0) && closef(ez.z, 1), "X x Y = Z");
    CHECK(closef(DotProduct(&ex, &ey), 0.0f), "X . Y = 0");
    CHECK(closef(DotProduct(&ex, &ex), 1.0f), "X . X = 1");

    VECTOR v = { 3.0f, 4.0f, 0.0f };
    NormaliseVector(&v);
    CHECK(closef(v.x, 0.6f) && closef(v.y, 0.8f) && closef(v.z, 0.0f),
          "normalise (3,4,0), got (%f,%f,%f)", v.x, v.y, v.z);

    MATRIX ident;
    memset(&ident, 0, sizeof ident);
    ident._11 = ident._22 = ident._33 = ident._44 = 1.0f;
    MATRIX scale;
    memset(&scale, 0, sizeof scale);
    scale._11 = 2.0f; scale._22 = 3.0f; scale._33 = 4.0f; scale._44 = 1.0f;
    MATRIX out;
    MatrixMultiply(&scale, &ident, &out);
    CHECK(closef(out._11, 2) && closef(out._22, 3) && closef(out._33, 4),
          "M * I = M");

    if (failures) { printf("test_new3d: %d FAILURES\n", failures); return 1; }
    printf("test_new3d: all ok\n");
    return 0;
}
