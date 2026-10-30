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
#include "main.h"

extern CAMERA CurrentCamera;
extern render_info_t render_info;
extern u_int16_t FirstModelUsed;
extern MODEL Models[MAXNUMOFMODELS];
extern void Build_View(void);
extern void platform_render_present(render_info_t *info);

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
static int s_pending_present;
static int s_skip_normal_present;
static int s_replay_active;
static float s_present_alpha;
static unsigned long s_present_frames;
static unsigned long s_simulation_frames;
static unsigned long s_level_ticks;
static u64 s_present_window_ms;
static unsigned long s_present_window_frames;
static int s_present_fps;

float interp_alpha(void)
{
	return INTERP_ALPHA;
}

void interp_frame_begin(void)
{
	s_cur.Pos = CurrentCamera.Pos;
	s_cur.Mat = CurrentCamera.Mat;
	s_applied = 0;
	s_pending_present = 0;
	s_skip_normal_present = 0;
	s_simulation_frames++;
	s_level_ticks++;
}

int interp_pending(void)
{
	return s_pending_present && !render_info.stereo_enabled;
}

void interp_begin_extra_present(void)
{
	s_replay_active = 1;
	s_skip_normal_present = 1;
}

void interp_end_extra_present(void)
{
	s_replay_active = 0;
}

int interp_replay_active(void)
{
	return s_replay_active;
}

void interp_present_window_tick(void)
{
#ifdef FRAME_INTERP_EXTRA_PRESENT
	u64 now = osGetTime();
	s_present_window_frames++;
	if (s_present_window_ms == 0) s_present_window_ms = now;
	if (now - s_present_window_ms >= 1000) {
		s_present_fps = (int)s_present_window_frames;
		s_present_window_frames = 0;
		s_present_window_ms = now;
	}
#endif
}

int interp_present_fps(void)
{
	return s_present_fps;
}

int interp_consume_outer_present_skip(void)
{
	int skip = s_skip_normal_present;
	s_skip_normal_present = 0;
	return skip;
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
	s_present_alpha = alpha;

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
				Models[i].Mat = s_curModel[i].Mat;
				Models[i].Pos.x = lerpf(s_prevModel[i].Pos.x, s_curModel[i].Pos.x, alpha);
				Models[i].Pos.y = lerpf(s_prevModel[i].Pos.y, s_curModel[i].Pos.y, alpha);
				Models[i].Pos.z = lerpf(s_prevModel[i].Pos.z, s_curModel[i].Pos.z, alpha);
				Models[i].Mat._41 = lerpf(s_prevModel[i].Mat._41, s_curModel[i].Mat._41, alpha);
				Models[i].Mat._42 = lerpf(s_prevModel[i].Mat._42, s_curModel[i].Mat._42, alpha);
				Models[i].Mat._43 = lerpf(s_prevModel[i].Mat._43, s_curModel[i].Mat._43, alpha);
				MatrixTranspose(&Models[i].Mat, &Models[i].InvMat);
			}
		}
	}
	s_applied = 1;
	s_pending_present = 1;
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

void interp_present_current(void)
{
	platform_render_present(&render_info);
	s_present_frames++;
}

void interp_present(void)
{
	interp_restore();
	if (!s_pending_present)
		return;
	s_pending_present = 0;
	s_skip_normal_present = 0;
	s_replay_active = 0;
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
	s_level_ticks = 0;
}

#endif
