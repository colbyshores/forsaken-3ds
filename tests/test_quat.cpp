/* test_quat.cpp - host harness for quat.cpp (C-with-classes
 * conversion). Pins the quaternion conventions the engine relies
 * on: QUAT is (w,x,y,z), MakeQuat of zero rotation is the identity,
 * multiplication by identity is a no-op, QuatToMatrix of identity
 * is the identity matrix, Slerp hits its endpoints, and a 90-degree
 * Z rotation maps +X to +Y under QuatToMatrix. */
#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>

#include "quat.h"
#include "new3d.h"

/* engine globals quat.cpp references (defined by the game TUs) */
VECTOR Forward;
VECTOR SlideUp;

static int failures = 0;
#define CHECK(cond, ...) do { \
    if (!(cond)) { printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); failures++; } \
} while (0)

static bool closef(float a, float b) { return fabsf(a - b) < 1e-4f; }
static bool quat_eq(const QUAT &q, float w, float x, float y, float z)
{
    return closef(q.w, w) && closef(q.x, x) && closef(q.y, y) && closef(q.z, z);
}

int main(void)
{
    trig_table_init();   /* MakeQuat/Slerp read the fast trig LUT */
    QUAT id;
    MakeQuat(0.0f, 0.0f, 0.0f, &id);
    CHECK(quat_eq(id, 1, 0, 0, 0), "zero rotation is identity, got (%f,%f,%f,%f)",
          id.w, id.x, id.y, id.z);

    QUAT z90;
    MakeQuat(0.0f, 0.0f, 90.0f, &z90);
    QUAT r;
    QuatMultiply(&z90, &id, &r);
    CHECK(quat_eq(r, z90.w, z90.x, z90.y, z90.z), "multiply by identity");

    MATRIX m;
    QuatToMatrix(&id, &m);
    CHECK(closef(m._11, 1) && closef(m._22, 1) && closef(m._33, 1) && closef(m._44, 1) &&
          closef(m._12, 0) && closef(m._13, 0) && closef(m._21, 0) && closef(m._31, 0),
          "identity quat -> identity matrix");

    /* Engine convention (pinned): MakeQuat(+90 deg Z) produces a
     * matrix with _12=-1, _21=+1 (row-vector D3D-style application,
     * i.e. the transpose of the column-vector GL reading). */
    QuatToMatrix(&z90, &m);
    CHECK(closef(m._11, 0) && closef(m._12, -1) && closef(m._21, 1) && closef(m._22, 0),
          "z90 matrix maps axes, got _11=%f _12=%f _21=%f _22=%f", m._11, m._12, m._21, m._22);

    QUAT out;
    Quaternion_Slerp(0.0f, &id, &z90, &out, 0);
    CHECK(quat_eq(out, id.w, id.x, id.y, id.z), "slerp alpha 0 is a");
    Quaternion_Slerp(1.0f, &id, &z90, &out, 0);
    CHECK(quat_eq(out, z90.w, z90.x, z90.y, z90.z) ||
          quat_eq(out, -z90.w, -z90.x, -z90.y, -z90.z), "slerp alpha 1 is b (up to sign)");

    QUAT n = { 2.0f, 0, 0, 0 };
    QuatNormalise(&n);
    CHECK(quat_eq(n, 1, 0, 0, 0), "normalise restores unit length");

    if (failures) { printf("test_quat: %d FAILURES\n", failures); return 1; }
    printf("test_quat: all ok\n");
    return 0;
}
