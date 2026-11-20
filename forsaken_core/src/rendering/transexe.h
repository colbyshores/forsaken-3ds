#pragma once

/* transexe.h -- deferred translucent draws, replayed per visibility group
 *
 * Model and level-mesh draw paths call AddTransExe for meshes with
 * translucent faces instead of drawing them; ExecuteTransExe(group) draws that
 * group's entries after its opaque geometry, and ExecuteTransExeUnclipped draws
 * entries for models without MODFLAG_Clip. The entries are TranslucentQueue's
 * (below) and are per camera pass: RenderCurrentCamera clears them, and entries
 * past MAXTRANSEXE are dropped without warning.
 *
 * Entries copy the RENDEROBJECT by value; its textureGroups pointer still
 * refers to the level hunk, which outlives the pass. Copies are marked so the
 * stereo memo and level cache never reuse them by slot address.
 */

#include "rendering/render.h"
#include "math/new3d.h"

constexpr int MAXTRANSEXE = 128;

struct TRANSEXE{
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
};

/*
 * fn prototypes
 */

void AddTransExe( RENDERMATRIX *Matrix , RENDEROBJECT *renderObject, int UseIdentity, u_int16_t Model, u_int16_t group, int16_t NumVerts );
void ExecuteTransExe( u_int16_t group );
void ExecuteTransExeUnclipped( u_int16_t group );

/* One camera pass's deferred translucent draws (one instance, Translucents,
 * defined in transexe.cpp). The C functions above forward to it; the camera
 * and title renderers call clear() before each pass. */
class TranslucentQueue {
	public:
	// a new camera pass: drop the previous pass's entries
	void clear() { NumOfTransExe = 0; }
	// defined in transexe.cpp and inlined into their forwarders, which run
	// per translucent mesh and per visibility group, as they did before;
	// other files call AddTransExe / ExecuteTransExe / ExecuteTransExeUnclipped
	[[gnu::always_inline]] inline void add( RENDERMATRIX *Matrix , RENDEROBJECT *renderObject , int UseIdentity, u_int16_t Model, u_int16_t group, int16_t NumVerts );
	[[gnu::always_inline]] inline void execute( u_int16_t group );
	[[gnu::always_inline]] inline void execute_unclipped( u_int16_t group );

	private:
	TRANSEXE TransExe[MAXTRANSEXE] = {};
	// Static, so it stays its own symbol as the old global was: ExecuteTransExe
	// re-reads it every iteration, and 19 KB into the object GCC derives the
	// array base from it with an extra sub (232 -> 233); at offset 0 it
	// pushes the array to offset 4 and UseIdentity's indexed load gains an add.
	static int16_t	NumOfTransExe;
};

extern TranslucentQueue Translucents;
