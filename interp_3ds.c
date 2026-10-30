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
 * Rotation uses quaternion slerp and is re-normalized by the existing
 * quaternion routines; element-wise basis lerp would shrink/expand the view
 * matrix. The camera-only prototype is deliberate: it isolates the visual
 * cadence effect and CPU cost before adding entity transforms.
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
#include "models.h"

extern CAMERA CurrentCamera;
extern u_int16_t FirstModelUsed;
extern MODEL Models[MAXNUMOFMODELS];
extern void Build_View(void);

#ifndef INTERP_ALPHA
#define INTERP_ALPHA 0.5F
#endif

typedef struct
{
	VECTOR	Pos;
	MATRIX	Mat;
} CAMSNAP;

typedef struct
{
	VECTOR Pos;
	MATRIX Mat;
	int valid;
} MODELSNAP;

static CAMSNAP	s_prev;
static CAMSNAP	s_cur;
static MODELSNAP s_prevModel[MAXNUMOFMODELS];
static MODELSNAP s_curModel[MAXNUMOFMODELS];
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

void interp_capture_models(void)
{
	int i;
	for (i = 0; i < MAXNUMOFMODELS; i++) s_curModel[i].valid = 0;
	i = FirstModelUsed;
	while (i != (u_int16_t)-1) {
		if (Models[i].Visible) {
			s_curModel[i].Pos = Models[i].Pos;
			s_curModel[i].Mat = Models[i].Mat;
			s_curModel[i].valid = 1;
		}
		i = Models[i].Prev;
	}
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

static void interp_rotation(MATRIX *out, const MATRIX *a, const MATRIX *b, float alpha)
{
	QUAT qa, qb, q;
	MatrixToQuat(&qa, (MATRIX *)a);
	MatrixToQuat(&qb, (MATRIX *)b);
	QuatNormalise(&qa);
	QuatNormalise(&qb);
	Quaternion_Slerp(alpha, &qa, &qb, &q, 0);
	QuatNormalise(&q);
	QuatToMatrix(&q, out);
}
void interp_apply(float alpha)
{
	CAMSNAP m;

	m.Pos.x = lerpf(s_prev.Pos.x, s_cur.Pos.x, alpha);
	m.Pos.y = lerpf(s_prev.Pos.y, s_cur.Pos.y, alpha);
	m.Pos.z = lerpf(s_prev.Pos.z, s_cur.Pos.z, alpha);

	interp_rotation(&m.Mat, &s_prev.Mat, &s_cur.Mat, alpha);
	m.Mat._41 = lerpf(s_prev.Mat._41, s_cur.Mat._41, alpha);
	m.Mat._42 = lerpf(s_prev.Mat._42, s_cur.Mat._42, alpha);
	m.Mat._43 = lerpf(s_prev.Mat._43, s_cur.Mat._43, alpha);
	interp_set_camera(&m);

	{
		int i;
		for (i = 0; i < MAXNUMOFMODELS; i++) {
			if (s_prevModel[i].valid && s_curModel[i].valid) {
				Models[i].Pos.x = lerpf(s_prevModel[i].Pos.x, s_curModel[i].Pos.x, alpha);
				Models[i].Pos.y = lerpf(s_prevModel[i].Pos.y, s_curModel[i].Pos.y, alpha);
				Models[i].Pos.z = lerpf(s_prevModel[i].Pos.z, s_curModel[i].Pos.z, alpha);
				interp_rotation(&Models[i].Mat, &s_prevModel[i].Mat, &s_curModel[i].Mat, alpha);
				Models[i].Mat._11 *= lerpf(s_prevModel[i].Mat._11, s_curModel[i].Mat._11, alpha);
				Models[i].Mat._22 *= lerpf(s_prevModel[i].Mat._22, s_curModel[i].Mat._22, alpha);
				Models[i].Mat._33 *= lerpf(s_prevModel[i].Mat._33, s_curModel[i].Mat._33, alpha);
				MatrixTranspose(&Models[i].Mat, &Models[i].InvMat);
			}
		}
	}
	s_applied = 1;
}

void interp_restore(void)
{
	int i;
	if (!s_applied)
		return;
	interp_set_camera(&s_cur);
	for (i = 0; i < MAXNUMOFMODELS; i++) {
		if (s_curModel[i].valid) {
			Models[i].Pos = s_curModel[i].Pos;
			Models[i].Mat = s_curModel[i].Mat;
			MatrixTranspose(&Models[i].Mat, &Models[i].InvMat);
		}
	}
	s_applied = 0;
}

void interp_frame_end(void)
{
	int i;
	s_prev = s_cur;
	for (i = 0; i < MAXNUMOFMODELS; i++) s_prevModel[i] = s_curModel[i];
	s_have_prev = 1;
}

void interp_reset(void)
{
	s_have_prev = 0;
	s_applied = 0;
}

#endif
