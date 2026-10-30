/* interp_3ds.c — temporal camera interpolation prototype. See interp.h.
 *
 * Compiles to nothing unless FRAME_INTERP is defined.
 *
 * Flow per rendered frame (see the call site in oct2.c MainGameRender):
 *   interp_frame_begin()   snapshots the camera the real frame will use
 *   ... real render ...
 *   interp_apply(alpha)    camera := lerp(previous, current, alpha)
 *   ... in-between render ...
 *   interp_restore()       camera := current
 *   interp_frame_end()     previous := current
 *
 * The camera is rebuilt exactly the way the engine does it elsewhere
 * (oct2.c: MakeViewMatrix + MatrixTranspose + Build_View): Pos and Mat are
 * the state, InvMat is Mat transposed, and Build_View() derives the view
 * matrix the render path consumes (visi.c FSSetView(&cam->View)).
 *
 * Rotation interpolation is element-wise on the 3x3 basis rather than a
 * quaternion slerp. At one-frame deltas the difference is sub-pixel; if the
 * prototype survives its measurement the slerp is the correctness follow-up.
 */
#ifdef FRAME_INTERP

#include <stdio.h>
#include <string.h>
#include "new3d.h"
#include "quat.h"
#include "compobjects.h"
#include "bgobjects.h"
#include "object.h"
#include "networking.h"
#include "mload.h"
#include "camera.h"

extern CAMERA CurrentCamera;
extern void Build_View(void);

#ifndef INTERP_ALPHA
#define INTERP_ALPHA 0.5F
#endif

typedef struct
{
	VECTOR	Pos;
	MATRIX	Mat;
} CAMSNAP;

static CAMSNAP	s_prev;
static CAMSNAP	s_cur;
static int	s_have_prev;
static int	s_applied;

float interp_alpha(void)
{
	return INTERP_ALPHA;
}

void interp_frame_begin(void)
{
	s_cur.Pos = CurrentCamera.Pos;
	s_cur.Mat = CurrentCamera.Mat;
	s_applied = 0;
}

int interp_ready(void)
{
	return s_have_prev;
}

static void interp_set_camera(const CAMSNAP *c)
{
	CurrentCamera.Pos = c->Pos;
	CurrentCamera.Mat = c->Mat;
	MatrixTranspose(&CurrentCamera.Mat, &CurrentCamera.InvMat);
	Build_View();
}

static float lerpf(float a, float b, float t)
{
	return a + (b - a) * t;
}

void interp_apply(float alpha)
{
	CAMSNAP m;

	m.Pos.x = lerpf(s_prev.Pos.x, s_cur.Pos.x, alpha);
	m.Pos.y = lerpf(s_prev.Pos.y, s_cur.Pos.y, alpha);
	m.Pos.z = lerpf(s_prev.Pos.z, s_cur.Pos.z, alpha);

#define LERP_M(f) m.Mat.f = lerpf(s_prev.Mat.f, s_cur.Mat.f, alpha)
	LERP_M(_11); LERP_M(_12); LERP_M(_13); LERP_M(_14);
	LERP_M(_21); LERP_M(_22); LERP_M(_23); LERP_M(_24);
	LERP_M(_31); LERP_M(_32); LERP_M(_33); LERP_M(_34);
	LERP_M(_41); LERP_M(_42); LERP_M(_43); LERP_M(_44);
#undef LERP_M

	interp_set_camera(&m);
	s_applied = 1;
}

void interp_restore(void)
{
	if (!s_applied)
		return;
	interp_set_camera(&s_cur);
	s_applied = 0;
}

void interp_frame_end(void)
{
	s_prev = s_cur;
	s_have_prev = 1;
}

#endif
