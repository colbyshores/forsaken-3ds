/* test_sphere.cpp - host harness for sphere.cpp. Pins the ray/sphere
 * contract of the two tests the game uses: RaytoSphere2's near and far
 * intersection points and its inside-start miss, and RaytoSphereShort's
 * range limit and its inside-start hit. */
#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>

#include "math/sphere.h"

static int failures = 0;
#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); failures++; } \
} while (0)

static bool closef(float a, float b) { return fabsf(a - b) < 1e-3f; }

int main(void)
{
    VECTOR center = { 0.0f, 0.0f, 0.0f };
    VECTOR org    = { 0.0f, 0.0f, -10.0f };
    VECTOR dir    = { 0.0f, 0.0f, 1.0f };
    VECTOR side   = { 0.0f, 1.0f, 0.0f };
    VECTOR inside = { 0.0f, 0.0f, 0.5f };
    VECTOR near_pt, far_pt;

    CHECK(RaytoSphere2(&center, 2.0f, &org, &dir, &near_pt, &far_pt), "head-on ray hits");
    CHECK(closef(near_pt.z, -2.0f) && closef(far_pt.z, 2.0f),
          "entry at z=-2, exit at z=2, got %f / %f", near_pt.z, far_pt.z);
    CHECK(!RaytoSphere2(&center, 2.0f, &org, &side, &near_pt, &far_pt), "perpendicular ray misses");
    CHECK(!RaytoSphere2(&center, 2.0f, &inside, &dir, &near_pt, &far_pt), "start inside reports no hit");

    CHECK(RaytoSphereShort(&center, 2.0f, &org, &dir, 8.5f), "hit within range (entry at 8)");
    CHECK(!RaytoSphereShort(&center, 2.0f, &org, &dir, 7.5f), "short ray stops before the sphere");
    CHECK(!RaytoSphereShort(&center, 2.0f, &org, &side, 100.0f), "perpendicular ray misses");
    CHECK(RaytoSphereShort(&center, 2.0f, &inside, &dir, 0.1f), "start inside reports a hit");

    if (failures) { printf("test_sphere: %d FAILURES\n", failures); return 1; }
    printf("test_sphere: all ok\n");
    return 0;
}
