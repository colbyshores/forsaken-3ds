#pragma once

/* transexe.h -- deferred translucent draws, replayed per visibility group
 *
 * Model and level-mesh draw paths call AddTransExe for execbufs with
 * translucent faces instead of drawing them; ExecuteTransExe(group) draws that
 * group's entries after its opaque geometry, and ExecuteTransExeUnclipped draws
 * entries for models without MODFLAG_Clip. TransExe[] lives in transexe.cpp and
 * is per camera pass: RenderCurrentCamera resets NumOfTransExe, and entries past
 * MAXTRANSEXE are dropped without warning.
 *
 * Entries copy the RENDEROBJECT by value; its textureGroups pointer still
 * refers to the level hunk, which outlives the pass. Copies are marked so the
 * stereo memo and level cache never reuse them by slot address.
 */

#include "rendering/render.h"
#include "math/new3d.h"

#define MAXTRANSEXE 128

typedef struct _TRANSEXE{
	int	UseIdentity;
	RENDERMATRIX  Matrix;
	RENDEROBJECT renderObject;
	int16_t		NumVerts;
	u_int16_t		Model;
	u_int16_t		group;
	int render_lighting_use_only_light_color_and_blend;
	int render_lighting_use_only_light_color;
	int render_lighting_point_lights_only;
	int render_lighting_enabled;
	int render_light_ambience;
	int render_light_ambience_alpha;
	int render_color_blend_red;
	int render_color_blend_green;
	int render_color_blend_blue;
}TRANSEXE;

extern int16_t	NumOfTransExe;

/*
 * fn prototypes
 */

void AddTransExe( RENDERMATRIX *Matrix , RENDEROBJECT *renderObject, int UseIdentity, u_int16_t Model, u_int16_t group, int16_t NumVerts );
void ExecuteTransExe( u_int16_t group );
void ExecuteTransExeUnclipped( u_int16_t group );
