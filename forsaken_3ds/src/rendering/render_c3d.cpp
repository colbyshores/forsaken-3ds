/* render_c3d.cpp -- render.h for the 3DS, on citro3d (PICA200)
 *
 * The 3DS backend of the FS* / draw_* seam (render_vulkan.cpp is the PC one),
 * plus the c3d_* calls render.h declares under __3DS__. c3d_context_init runs
 * C3D_Init once, with a 16x (4 MB) command buffer. c3d_renderer_init, called
 * from render_init and idempotent, loads the vertex shader and creates the
 * render targets (left and right top-screen eyes, and the bottom screen,
 * where the HUD is drawn), the 4 MB linear vertex scratch, the ProcTex
 * wall-grain tables and the fragment-light setup for object shine.
 * FSBeginScene opens the frame on the left target and sets the GPU state
 * draws depend on; c3d_present ends it, and C3D_FrameEnd's display transfer
 * puts it on screen. c3d_transfer_eye moves the second stereo pass to the
 * right target.
 *
 * Engine vertex, index and normal buffers are linearAlloc memory holding
 * engine-format vertices. draw_render_object converts each texture group's
 * LVERTEX / TLVERTEX (packed colour) to gpu_vertex_t (36 bytes, float
 * colour) in the scratch and draws from there. Two caches skip that
 * conversion: the stereo memo lets the right eye resubmit what the left eye
 * converted (keyed by batch, so no draw is ever skipped), and in mono frames
 * level walls stay converted in retained linear buffers until a texture
 * animation step or c3d_release_level_cache.
 *
 * upload_matrices puts world_matrix in the shader's modelView uniform and
 * view x projection (with the PICA depth remap and the 90-degree screen
 * rotation) in projection, so the shader has world-space positions for its
 * per-vertex lighting: ambient plus up to GPU_MAX_LIGHTS point lights from
 * c3d_upload_xlights. Textures load as .t3x through Tex3DS from the HD pack
 * the memory tier selects, else from PNG converted to RGBA4444 and Morton
 * tiled. TexEnv modes: DIFFUSE (texture x vertex colour), AUX (walls, plus
 * ProcTex grain: "Wall detail"), OBJECT (models, plus fragment-light Phong
 * specular: "Object shine"). For full-screen cameras visi.cpp asks for
 * scissor mode: FSSetViewPort keeps the full viewport and scissors to the
 * group's rect, so adjacent groups share one projection.
 *
 * Hard-won rules: initialise GPU state (AttrInfo, TexEnv, depth, cull,
 * uniforms) explicitly, since unset registers draw nothing without
 * crashing; and every draw needs its own C3D_GetBufInfo (bind_vertex_buffer).
 */

#ifdef __3DS__

#include <3ds.h>
#include <citro3d.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include "game/main.h"
#include "math/new3d.h"
#include "math/quat.h"
#include "game/compobjects.h"
#include "game/bgobjects.h"
#include "game/object.h"
#include "net/networking.h"
#include "rendering/render.h"
#include "rendering/stereomemo.h"
#include "rendering/texture.h"
#include "data/file.h"
#include "platform_3ds/main_3ds.h"
#include "rendering/lights.h"
#include "util/util.h"
#include "util/platform.h"
#include "game/oct2.h"

/* Defined further down; used above their definitions. */
void c3d_renderer_cleanup(void);
void GetRealLightAmbientWorldSpace(VECTOR *Pos, float *R, float *G, float *B, float *A);
#include "util/platform.h"
#include "game/camera.h"

/* ---- shader binary (compiled from shaders/render_c3d.v.pica) ---- */
#include "rendering/render_c3d_shbin.h"

/* ============================================================================
 * The rest of this translation unit is split, by concern, into the .inc slices
 * in rendering/c3d/ (the t2k pattern), so a search hit names the part of the
 * renderer it is in. They are not headers: no include guards, no includes of
 * their own, and they depend on the order below; the Makefile compiles *.cpp
 * only, so none becomes an object of its own. The preprocessor emits the same
 * token stream as the single file did, so the object code is unchanged and all
 * cross-function inlining stays. Adding, removing or reordering an include
 * below changes the program.
 * ========================================================================== */
#include "rendering/c3d/01_state.inc"
#include "rendering/c3d/02_level_cache.inc"
#include "rendering/c3d/03_context.inc"
#include "rendering/c3d/04_hud_screen.inc"
#include "rendering/c3d/05_shader.inc"
#include "rendering/c3d/06_matrices_lights.inc"
#include "rendering/c3d/07_scene.inc"
#include "rendering/c3d/08_viewport.inc"
#include "rendering/c3d/09_buffers.inc"
#include "rendering/c3d/10_texenv.inc"
#include "rendering/c3d/11_textures.inc"
#include "rendering/c3d/12_cpu_lighting.inc"
#include "rendering/c3d/13_draw.inc"

#endif /* __3DS__ */
