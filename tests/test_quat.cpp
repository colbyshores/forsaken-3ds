/* test_quat.cpp - host harness for quat.cpp (C-with-classes
 * conversion). Pins the quaternion conventions the engine relies
 * on: QUAT is (w,x,y,z), MakeEuler of zero rotation is the identity,
 * multiplication by identity is a no-op, ToMatrix of identity
 * is the identity matrix, QUATLERP::Step hits its endpoints, and a 90-degree
 * Z rotation maps +X to +Y under ToMatrix. */
#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>

#include "math/quat.h"
#include "math/new3d.h"

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
	trig_table_init();   /* MakeEuler reads the fast trig LUT */
	QUAT id;
	id.MakeEuler(0.0f, 0.0f, 0.0f);
	CHECK(quat_eq(id, 1, 0, 0, 0), "zero rotation is identity, got (%f,%f,%f,%f)",
		  id.w, id.x, id.y, id.z);

	QUAT z90;
	z90.MakeEuler(0.0f, 0.0f, 90.0f);
	QUAT r;
	z90.Multiply(&id, &r);
	CHECK(quat_eq(r, z90.w, z90.x, z90.y, z90.z), "multiply by identity");

	MATRIX m;
	id.ToMatrix(&m);
	CHECK(closef(m._11, 1) && closef(m._22, 1) && closef(m._33, 1) && closef(m._44, 1) &&
		  closef(m._12, 0) && closef(m._13, 0) && closef(m._21, 0) && closef(m._31, 0),
		  "identity quat -> identity matrix");

	/* Engine convention (pinned): MakeEuler(+90 deg Z) produces a
	 * matrix with _12=-1, _21=+1 (row-vector D3D-style application,
	 * i.e. the transpose of the column-vector GL reading). */
	z90.ToMatrix(&m);
	CHECK(closef(m._11, 0) && closef(m._12, -1) && closef(m._21, 1) && closef(m._22, 0),
		  "z90 matrix maps axes, got _11=%f _12=%f _21=%f _22=%f", m._11, m._12, m._21, m._22);

	/* QUATLERP::Step is the interpolation the game uses (normalised lerp) */
	QUAT out;
	QUATLERP ql;
	ql.start = id; ql.end = z90; ql.crnt = &out; ql.dir = 1.0f;
	ql.time = 0.0f; ql.Step();
	CHECK(quat_eq(out, id.w, id.x, id.y, id.z), "lerp time 0 is start");
	ql.time = 1.0f; ql.Step();
	CHECK(quat_eq(out, z90.w, z90.x, z90.y, z90.z), "lerp time 1 is end");
	ql.time = 0.5f; ql.Step();
	CHECK(closef(out.w*out.w + out.x*out.x + out.y*out.y + out.z*out.z, 1.0f), "lerp midpoint is unit length");

	QUAT n = { 2.0f, 0, 0, 0 };
	n.Normalise();
	CHECK(quat_eq(n, 1, 0, 0, 0), "normalise restores unit length");

	if (failures) { printf("test_quat: %d FAILURES\n", failures); return 1; }
	printf("test_quat: all ok\n");
	return 0;
}
