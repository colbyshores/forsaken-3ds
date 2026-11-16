/* test_sphere.cpp - host harness for sphere.cpp (C-with-classes
 * conversion). Pins the ray/sphere contract: head-on hit distance,
 * perpendicular miss, origin-inside classification, and the
 * sphere normal at the pole. */
#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>

#include "sphere.h"

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
    VECTOR inside = { 0.0f, 0.0f, 0.0f };

    float d = RaytoSphere(&center, 2.0f, &org, &dir);
    CHECK(closef(d, 8.0f), "head-on hit at 8, got %f", d);

    float miss = RaytoSphere(&center, 2.0f, &org, &side);
    CHECK(closef(miss, 0.0f), "perpendicular ray misses, got %f", miss);

    /* Historical behaviour, pinned not endorsed: from inside,
     * intersect() returns the behind-origin root (-radius). */
    float in = intersect(&center, 2.0f, &inside, &dir);
    CHECK(closef(in, -2.0f), "origin inside returns -2 root, got %f", in);

    VECTOR point = { 0.0f, 2.0f, 0.0f };
    NORMAL n;
    SphereNormal(&center, &point, 2.0f, &n);
    CHECK(closef(n.y, 1.0f) && closef(n.x, 0.0f) && closef(n.z, 0.0f),
          "pole normal is +Y, got (%f,%f,%f)", n.x, n.y, n.z);

    if (failures) { printf("test_sphere: %d FAILURES\n", failures); return 1; }
    printf("test_sphere: all ok\n");
    return 0;
}
