# ForsakenX 3DS Port - Technical Notes

## Overview
Port of ForsakenX (C-based space combat game, ~158K lines) from SDL2/OpenGL/OpenAL
to Nintendo 3DS using devkitARM, picaGL (OpenGL 1.x shim over citro3d/PICA200),
libctru HID, and ndsp audio.

## Architecture

| PC Component | 3DS Replacement |
|---|---|
| SDL2 + OpenGL | picaGL (GL1 immediate mode) |
| SDL input | libctru HID (`hidScanInput`) |
| OpenAL audio | ndsp DSP audio |
| SDL windowing | libctru gfx (`gfxInitDefault`) |
| Winsock/enet | Stubbed (no networking) |
| Lua socket module | Stubbed (`pcall(require, ...)`) |
| File I/O | romfs (embedded in .3dsx) |

## Fixes Applied (chronological)

### 1. Build System (`Makefile.3ds`)
**Problem:** devkitARM cross-compiler doesn't have SDL2, uses different type names.
**Fix:** Created `Makefile.3ds` excluding all SDL/desktop sources, linking picaGL + citro3d + libctru.
Excluded: `main_sdl.c`, `input_sdl.c`, `sound_openal.c`, `render_gl2.c`, `render_gl3.c`,
`net_enet.c`, `net_tracker.c`, bot AI files, etc.

### 2. Type Definitions (`main.h`)
**Problem:** devkitARM provides `uint*_t` but not `u_int*_t` (BSD types).
**Fix:** Added `typedef uint8_t u_int8_t;` etc. under `#if defined(WIN32) || defined(__3DS__)`.

### 3. Header Include Paths (`Makefile.3ds`)
**Problem:** Game uses `#include <header.h>` (angle brackets) for project-local headers.
**Fix:** Added `-isystem $(SRCDIR)` to CFLAGS so the project root is searched for angle-bracket includes.

### 4. GLOB_ABORTED (`file.c`, `stubs_3ds.c`)
**Problem:** devkitARM's glob.h defines `GLOB_ABEND` not `GLOB_ABORTED`.
**Fix:** `#define GLOB_ABORTED GLOB_ABEND` in file.c. Implemented full `glob()`/`globfree()` in stubs_3ds.c.

### 5. BSS Size Reduction (`models.h`, `mxaload.h`, `render.h`)
**Problem:** Static arrays made BSS 368MB - far exceeding 3DS RAM (64-124MB).
**Fix:** Reduced `MAXMODELHEADERS` 1024→128, `MAXMXAMODELHEADERS` 1024→128,
`MAX_TEXTURE_GROUPS` 600→128 under `#ifdef __3DS__`. BSS reduced to ~16MB.

### 6. Platform Layer (`main_3ds.c`, `main_3ds.h`, `platform.h`)
**Problem:** Game calls SDL functions for timing, window management, video init.
**Fix:** Created platform abstraction: `platform_init()`, `platform_init_video()`,
`platform_render_present()`, `platform_get_ticks()`, `platform_delay()`, `platform_shutdown()`.
Uses `svcGetSystemTick()` for timing, `pglSwapBuffers()` for frame present.

### 7. GSP Service Init (`main_3ds.c`)
**Problem:** picaGL's `pglInit()` uses GPU services that require GSP to be initialized first.
Crash in `gxCmdQueueClear` (NULL pointer at address 0x6).
**Fix:** Call `gfxInitDefault()` and `gfxSet3D(false)` before `pglInit()`.

### 8. Lua socket.http Crash (`scripts/games.lua`)
**Problem:** `games.lua` does `require("socket.http")` which doesn't exist on 3DS (no luasocket).
This caused `lua_init()` → `run_init_func()` to fail, triggering cleanup which called
`pglExit()` before `pglInit()` → NULL dereference crash.
**Fix:** Changed to `pcall(require, "socket.http")` with nil guard on the `get()` function.

### 9. pglExit NULL Guard (`main_3ds.c`)
**Problem:** If `AppInit()` fails before video init, cleanup calls `pglExit()` on NULL `pglState`.
**Fix:** Track `_video_initialized` flag; only call `pglExit()`/`gfxExit()` when set.

### 10. Config Save on Read-Only FS (`scripts/config.lua`)
**Problem:** `config_save()` calls `io.open(path, 'wb')` on romfs (read-only), returns nil,
then `f:close()` crashes with "attempt to index nil value".
**Fix:** Added `if not f then return end` guard at the top of `config_save()`.

### 11. Default Config Files (`Configs/main.txt`, `Configs/debug.txt`)
**Problem:** `config_load("main")` calls `assert(loadfile("configs/main.txt"))` - crashes if
the file doesn't exist. romfs is read-only so `touch_file()` can't create it.
**Fix:** Created default config files with minimal Lua content in `Configs/` directory.

### 12. Game Data (`Data/offsets/`, `Data/models/`, `Data/textures/`)
**Problem:** The ForsakenX engine repo doesn't include game assets. Title screen needs
`.off` files, `.mx` models, and `.png` textures from the separate `ForsakenX/forsaken-data` repo.
**Fix:** Cloned `forsaken-data` and copied offsets/, models/, textures/, bgobjects/, txt/
into the Data/ directory. These get embedded in the .3dsx via romfs.

### 13. RomFS Embedding (`Makefile.3ds`, `romfs/`)
**Problem:** Game data must be bundled into the single .3dsx binary.
`3dsxtool --romfs=` doesn't follow symlinks.
**Fix:** Created `romfs/` directory with lowercase symlinks (configs→Configs, data→Data, etc.).
Makefile creates `.romfs_staging/` via `cp -rL` (follows symlinks) as a build step.
The staging directory is only rebuilt when the stamp file is stale.

### 14. Mipmap Filter Fix (`render_gl_shared.c`)
**Problem:** Game sets `GL_TEXTURE_MIN_FILTER` to `GL_LINEAR_MIPMAP_NEAREST` but
picaGL doesn't support mipmaps. Without mipmaps, this filter causes textures to render BLACK.
`gluBuild2DMipmaps` stub doesn't actually generate mipmaps.
**Fix:** On 3DS, use `GL_LINEAR` for both min and mag filters. Skip `gluBuild2DMipmaps` call.

### 15. Texture Downscaling (`render_gl_shared.c`)
**Problem:** Game textures are 2048x2048 but PICA200 max is 1024x1024.
**Fix:** Added 2:1 box filter downscaling loop under `#ifdef __3DS__` in `create_texture()`.
Repeatedly halves dimensions until both are ≤1024.

### 16. glGetIntegerv GL_VIEWPORT (`picaGL/source/get.c`) ★ KEY FIX
**Problem:** picaGL's `glGetIntegerv()` only handled `GL_MAX_TEXTURE_SIZE` (hardcoded to 128).
It returned GARBAGE for `GL_VIEWPORT`. The game's `FSGetViewPort()` reads `GL_VIEWPORT`
to get the current viewport dimensions, which are then used by `SetFOV()` to compute the
projection matrix. With garbage viewport values, both X and Y scale factors were identical
(~2.0 instead of 1.0 and 1.667), producing a square projection that squished the 5:3
aspect ratio into a thin horizontal band.
**Fix:** Added `GL_VIEWPORT` case returning `viewportX/Y/Width/Height` from `pglState`.
Also fixed `GL_MAX_TEXTURE_SIZE` from 128 to 1024 (PICA200 actual limit).

## Files Created for 3DS Port

| File | Purpose |
|---|---|
| `Makefile.3ds` | devkitPro build system |
| `main_3ds.c` | Platform init, picaGL setup, tick counter |
| `main_3ds.h` | Platform function declarations |
| `input_3ds.c` | HID input with 3DS button mapping |
| `sound_3ds.c` | ndsp audio with WAV parser |
| `net_3ds.c` | Network stubs |
| `stubs_3ds.c` | glob, ftime, SDL stubs |
| `platform.h` | SDL_GetTicks/SDL_Delay macros |
| `romfs/` | Symlinks to game data for romfs embedding |
| `picaGL/source/glu.c` | gluOrtho2D, gluBuild2DMipmaps, gluErrorString |
| `picaGL/include/GL/glu.h` | GLU function declarations |
| `picaGL/include/GL/glext.h` | Missing GL extension defines |

### 17-21. D3D→PICA200 Pipeline — THE CORRECT FIX ★★★ CORE BREAKTHROUGH ★★★

**Background on the wrong path.** An earlier iteration tried to fix the D3D→GL
mismatch with three layered "reflection" hacks:
1. Negate View.z / Look.z in `DisplayTitle` (reposition camera)
2. Negate modelview row 3 (`m[8..11]`) in `reset_modelview` (introduce reflection)
3. Modify picaGL's `fix_projection` rotation matrix + `GX_TRANSFER_FLIP_VERT`
   to un-mirror the resulting horizontal flip

This "worked" in the sense that geometry rendered right-side-out, but it had
ugly side effects:
- The modelview row-3 negate is mathematically `v.z → -v.z`. Since `mv = world * view`
  in row-vector form, this negates the **local** Z of every model vertex, not
  the world Z. Symmetric geometry (stacked discs rotating around Y) was fine;
  **asymmetric geometry (the Mtop/Mbot mechanism cage, the VDU screens)
  rendered front-to-back flipped**, looking like a 180° rotation around Z.
- The camera Z flip preserved distances only because the modelview reflection
  happened to partially cancel it — fragile.
- Horizontal mirror required GX_TRANSFER_FLIP_VERT as a separate display-level
  correction, which couldn't be reasoned about in the same coordinate space as
  the matrix hacks.

The real diagnosis is that D3D→PICA200 is not one problem — it's **two independent
problems**, and each has a clean one-line fix:

---

#### A. Depth range conversion (`FSSetProjection`) ★ CORE FIX ★

**Problem:** The game builds a D3D LH projection matrix:
```
_33 =  F/(F-N)    _34 = 1
_43 = -F*N/(F-N)  _44 = 0
```
After perspective divide this produces `NDC.z ∈ [0, 1]` (D3D convention).

picaGL's `fix_projection` expects **GL** NDC `z ∈ [-1, 1]` and applies
`z' = 0.5*z - 0.5*w` to remap to PICA's native `[-1, 0]`. If you feed it the
D3D matrix directly you get `[-0.5, 0]` — half the depth precision **and** a
busted near plane.

**Fix:** Rescale two elements of the D3D projection in `FSSetProjection` on 3DS
to produce GL NDC before picaGL's remap runs:

```c
#ifdef __3DS__
    RENDERMATRIX gl_proj;
    memmove(&gl_proj, matrix, sizeof(RENDERMATRIX));
    gl_proj._33 = 2.0f * matrix->_33 - 1.0f;  // (F+N)/(F-N)
    gl_proj._43 = 2.0f * matrix->_43;         // -2FN/(F-N)
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf((GLfloat*)&gl_proj.m);
#else
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf((GLfloat*)&matrix->m);
#endif
```

`_34 = 1` is **left alone** — the game's D3D view matrix produces `v.z > 0`
for in-front points, so keeping `clip.w = +v.z` is correct. The view matrix
does NOT need to be touched.

Two-step depth pipeline on 3DS is now:
- D3D `[0, 1]` → GL `[-1, 1]` via `_33, _43` rewrite in FSSetProjection
- GL `[-1, 1]` → PICA `[-1, 0]` via picaGL's existing `0.5z - 0.5w` remap

Full PICA depth precision, correct near/far, no view matrix changes.

---

#### B. Winding / culling for CW-front meshes (`reset_cull`, `cull_cw`) ★ CORE FIX ★

**The PICA200 hardware only supports CCW-front culling modes.** From libctru
`<3ds/gpu/enums.h>`:
```c
typedef enum {
    GPU_CULL_NONE      = 0,
    GPU_CULL_FRONT_CCW = 1,  // "Front, counter-clockwise"
    GPU_CULL_BACK_CCW  = 2,  // "Back, counter-clockwise"
} GPU_CULLMODE;
```
There is **no `GPU_CULL_*_CW` mode in hardware.** The GPU always assumes CCW
is front and lets you pick which face to cull.

And picaGL's `glFrontFace` is a no-op stub (`picaGL/source/stubs.c:75`). So
the game's `glFrontFace(GL_CW)` in `reset_cull` is **silently ignored**. The
engine's D3D-authored meshes have CW-front triangles, but picaGL always treats
them as if CCW were front, meaning:

- Game calls `glCullFace(GL_BACK)` — intent: "cull back faces"
- picaGL maps it to `GPU_CULL_BACK_CCW` — hardware: "cull the back where front=CCW"
- Hardware culls CW triangles — i.e., **culls the game's front faces**
- Only back faces are visible → scene looks inside-out

**Fix:** Invert the `glCullFace` argument on 3DS only. Swap `GL_BACK`↔`GL_FRONT`
so picaGL's CCW-front assumption lines up with what we want.

```c
void reset_cull(void)
{
    glEnable(GL_CULL_FACE);
    glFrontFace(GL_CW);
#ifdef __3DS__
    /* picaGL's glFrontFace is a no-op and PICA200 hardware only has
     * CCW-front modes. Our meshes are CW-front, so we flip the cull
     * sense here: asking for GL_FRONT makes picaGL emit
     * GPU_CULL_FRONT_CCW, which culls CCW triangles in hardware
     * (i.e., the real back faces of our D3D meshes). */
    glCullFace(GL_FRONT);
#else
    glCullFace(GL_BACK);
#endif
}

void cull_cw(void)
{
#ifdef __3DS__
    glCullFace(GL_BACK);   // same inversion
#else
    glCullFace(GL_FRONT);
#endif
}
```

**Why this is the clean fix:** it involves zero reflections, zero matrix
manipulations, and has no interaction with the rest of the pipeline. It just
tells the hardware to cull the other face because the hardware assumes the
opposite handedness from what the data is authored for. Asymmetric geometry
renders identically to the PC build. No FLIP_VERT needed. No modelview hacks.
No View.z flip.

---

#### Files that are NOT touched on 3DS (and why)

- `title.c` DisplayTitle: **no** View.z/Look.z flip. The raw D3D camera is
  passed to `MakeViewMatrix` unchanged.
- `render_gl_shared.c` `reset_modelview`: **no** modelview Z-row negate. Combined
  `world * view` is loaded as-is.
- `picaGL/source/picaGL.c` `pglSwapBuffers`: **no** `GX_TRANSFER_FLIP_VERT`. The
  scene already renders correctly oriented; a hardware flip would re-mirror it.
- `new3d.c` `MakeViewMatrix`: unchanged. Still uses D3D LH convention
  (`vz = Look - View`, `vx = cross(Up, vz)`). The resulting view matrix, when
  GL's column-major reinterpretation transposes it, produces mathematically
  correct view-space coordinates. The earlier worry about "D3D's vx is opposite
  sign from GL's vx" is a conceptual difference, not a pipeline error — the
  numeric result going through `glLoadMatrixf` is the same either way.

---

### 22. Letterbox 320×240 viewport centered on 400×240 physical screen ★ FRAMING FIX ★

**Problem:** The 3DS top screen is **5:3** (400×240). Forsaken's title camera
positions, room layout, HUD, and `SetFOV` defaults were all authored for **4:3**
(Windows/SDL build defaults to 800×600). Rendering at the full 400×240 gives
a correct-aspect projection but cuts ~27% off the vertical framing — the title
crate overflows top and bottom, the VDUs fall off the edges, the entire
background room feels cramped.

**Fix:** Lie to the engine about the screen size. Use 320×240 as the logical
screen (render_info.ThisMode, window_size, WindowsDisplay), which naturally
computes the correct 4:3 aspect in `SetFOV`. When the game calls `glViewport`,
shift it horizontally by 40 pixels so the 320×240 viewport is centered on the
400-pixel physical width, leaving 40-pixel black bars on each side.

`glClearColor(0,0,0,1)` is already the game default and picaGL's `glClear`
always fills the entire physical framebuffer (hard-coded `_picaViewport(0, 0, 240, 400)`
in `picaGL/source/misc.c`), so the black bars stay black every frame without
any extra work.

```c
/* main_3ds.c */
#define PHYS_SCREEN_WIDTH   400
#define PHYS_SCREEN_HEIGHT  240
#define SCREEN_WIDTH        320     /* logical 4:3 */
#define SCREEN_HEIGHT       240
#define LETTERBOX_OFFSET_X  ((PHYS_SCREEN_WIDTH - SCREEN_WIDTH) / 2)  /* 40 */
```

```c
/* main_3ds.h — shared with render_gl_shared.c */
#define FS3DS_LETTERBOX_OFFSET_X 40
```

```c
/* render_gl_shared.c FSSetViewPort */
int vp_x = (int)view->X;
#ifdef __3DS__
vp_x += FS3DS_LETTERBOX_OFFSET_X;
#endif
glViewport(vp_x, bottom, (GLint)view->Width, (GLint)view->Height);
```

This preserves the Windows-era 4:3 framing faithfully on the 5:3 hardware.

---

### 23. picaGL `fix_projection` rotation — keep the modified rotation ★

picaGL's upstream `fix_projection` premultiplies the projection with a matrix
that rotates the 240×400 portrait framebuffer's output to display as 400×240
landscape. The upstream version uses `row[1].x = 1.0` which is actually a
**reflection** (determinant −1), not a rotation. We keep the local modification
`row[1].x = -1.0` (proper 90° rotation, determinant +1):

```c
/* picaGL/source/utils/math_utils.c matrix4x4_fix_projection */
mp.row[0].x = 0.0;
mp.row[0].y = 1.0;
mp.row[1].x = -1.0;  /* proper rotation, not reflection */
mp.row[1].y = 0.0;
```

Rationale: the culling fix (Section B above) depends on hardware seeing CCW as
front for D3D-authored meshes when combined with the inverted `glCullFace`.
The upstream reflection (det=−1) would flip winding once, which would combine
with our cull inversion to break the culling again. A proper rotation (det=+1)
preserves winding, letting the cull inversion land cleanly.

## Key Concepts for PICA200 Porting (Generalized — use for citro3d port)

The D3D LH → PICA200 conversion is **two orthogonal problems**, and each has
a single-site fix. **Do not conflate them.** The earlier hack stack tried to
solve them simultaneously with matrix reflections and paid for it with
asymmetric-geometry bugs.

### Problem 1: Depth Range
D3D projection produces `NDC.z ∈ [0, 1]` (near=0, far=1).
PICA200 depth hardware wants `z ∈ [-1, 0]` (near=-1, far=0).
OpenGL convention is `NDC.z ∈ [-1, 1]` (near=-1, far=1).

**Fix (both picaGL and citro3d paths):** rescale the projection matrix's
`_33` and `_43` elements at upload time to produce GL-style `[-1, 1]`:
```c
_33' = 2 * _33 - 1     // (F+N)/(F-N)
_43' = 2 * _43         // -2FN/(F-N)
// leave _34 = 1 so clip.w = +v.z matches D3D view-space sign
```

For the picaGL path, picaGL's existing `0.5z - 0.5w` depth remap then takes
the result to PICA's `[-1, 0]`.

For a native citro3d port, either:
- Apply the same `_33, _43` rewrite + a second `0.5z - 0.5w` matrix composed
  into the uploaded projection, or
- Go directly from D3D `[0, 1]` to PICA `[-1, 0]` with `_33' = _33 - 1`,
  `_43' = _43` (which is matrix-wise `row[2] -= row[3]`) and skip GL as an
  intermediate.

**Leave the view matrix alone.** D3D `v.z > 0` for in-front points is fine;
clip.w stays positive.

### Problem 2: Winding / Culling Handedness
D3D meshes are **CW-front** (clockwise winding = front face in D3D convention).
PICA200 hardware **only** supports CCW-front modes (`GPU_CULL_FRONT_CCW`,
`GPU_CULL_BACK_CCW`). There is **no hardware knob** to change this.

**Fix (picaGL path):** invert the argument to `glCullFace` in the game code.
`glCullFace(GL_FRONT)` on 3DS instead of `GL_BACK`, because picaGL's
`glFrontFace` is a no-op and it always assumes CCW-front. This makes picaGL
cull the CCW triangles which, on CW-front data, are the real back faces.

**Fix (citro3d path):** call `C3D_CullFace(GPU_CULL_FRONT_CCW)` when the game
wants back-face culling, and `C3D_CullFace(GPU_CULL_BACK_CCW)` when it wants
front-face culling. Just swap the two enum values in whatever translation
layer maps the game's cull state to citro3d.

**Do NOT reflect the geometry** to flip winding — reflections flip local-Z
on asymmetric models and introduce visual artifacts like the Mtop/Mbot cage
appearing front-to-back flipped relative to the stacked discs.

### What you do NOT need to do
- You do NOT need to negate `View.z`/`Look.z` in camera setup.
- You do NOT need to negate any row or column of the modelview matrix.
- You do NOT need `GX_TRANSFER_FLIP_VERT` for any handedness reason.
- You do NOT need to touch `MakeViewMatrix` / `CalcViewAxes`. The D3D LH
  cross-product convention is fine when the final image only has the two
  fixes above.

### View matrix & column-major note
The game stores matrices row-major with row-vector convention (`v_row * M`).
OpenGL and citro3d interpret the same memory as column-major with column-vector
convention (`M * v_col`). This implicit transpose is **mathematically the
identity** for D3D matrices: `(M_row)ᵀ` applied column-major gives the same
result as `M_row` applied row-major. So the row-major memory layout of the
game's D3D view matrix produces correct view-space coordinates on GL/PICA
without any conversion. The only real conversions needed are Problem 1 and
Problem 2 above.

## The WHY — Debugging Journey and Reasoning

This section is the most important part of the document for anyone attempting
a citro3d port or another PICA200 port. The TL;DR above tells you *what* to
do; this section tells you *why*, so you don't accidentally reinvent the
layered hack stack we had to back out of.

### Why the old hack stack existed at all

The original port noticed that geometry rendered **inside-out** (you could see
the back walls of every closed mesh through the front faces). The intuitive
interpretation was "D3D LH vs GL RH handedness mismatch" — the ingredients
of which are:
- D3D uses left-handed coords with +Z going INTO the screen
- GL uses right-handed coords with −Z going INTO the screen
- A handedness change requires negating one axis somewhere

So the fix was assumed to be "negate Z somewhere in the matrix chain." Doing
that on the modelview (the Z-row negate hack) did fix the inside-out symptom
because **any matrix reflection flips triangle winding as a side effect** —
the scene's apparent CW triangles became CCW after the reflection, and
PICA200's CCW-front hardware then rendered them.

This worked for most geometry. Symmetric shapes (the stacked menu discs, the
room walls) looked fine because local-Z flipping a cylinder you're viewing
side-on is invisible.

But asymmetric geometry revealed the truth. The Mtop/Mbot mechanism cage has
a distinct "front" and "back" in its local coordinate system, and reflecting
its local-Z axis turned the cage front-to-back. That's why the user described
it as "rotated 180° around Z" — the mechanism's facing direction had literally
been mirrored in model space. Similarly, the VDU's green screen text ended up
on what should have been the back face of the monitor model.

The Z-row hack was not a handedness fix. **It was a winding-order hack that
happened to be labeled "handedness" because it used the same mathematical
operation a real handedness conversion would use.** Two very different root
causes with the same matrix signature.

### What actually goes wrong end-to-end

Once we stopped conflating "handedness" with "winding order" and traced them
separately, the picture cleared up. These are the real, independent things
that differ between D3D LH and PICA200:

1. **Depth range of the projection output.** D3D projection outputs
   `NDC.z ∈ [0, 1]`. GL outputs `[−1, 1]`. PICA hardware uses `[−1, 0]`.
   If you feed a D3D projection through picaGL's GL-assuming `0.5z − 0.5w`
   remap, you get `[−0.5, 0]` — half the depth precision and a wrong near
   plane. The fix is to rescale two elements of the projection matrix so
   it produces GL-style `[−1, 1]` before picaGL's remap runs. **This is a
   pure projection-matrix rewrite. It touches nothing else in the pipeline.**

2. **Triangle winding convention vs hardware culling.** D3D authors meshes
   as CW-front. The PICA200 GPU has exactly two hardware cull modes,
   `GPU_CULL_FRONT_CCW` and `GPU_CULL_BACK_CCW`. Both assume CCW is front.
   There is **no CW-front mode in hardware, ever, under any API** — it's a
   physical limitation of the PICA200 face-culling unit. And picaGL's
   `glFrontFace` is a no-op stub, so `glFrontFace(GL_CW)` never reaches
   the hardware (it couldn't do anything with it even if it did).

   The game says `glCullFace(GL_BACK)` intending "cull what D3D considers
   back." picaGL translates this to `GPU_CULL_BACK_CCW` which means "cull
   what hardware considers back, assuming front=CCW." Hardware then culls
   CW triangles — i.e., the game's actual front faces. The visible result
   is back-faces-only. Inside-out.

   The fix is **telling the hardware to cull the opposite face.** In the
   picaGL path that means `glCullFace(GL_FRONT)` in `reset_cull` on 3DS,
   which picaGL maps to `GPU_CULL_FRONT_CCW`, which culls CCW in hardware —
   i.e., the game's real back faces. No matrix touches. No reflections.
   No geometry changes. Just "the hardware assumes the opposite winding, so
   ask it to cull the opposite face."

3. **Authored aspect ratio.** Unrelated to handedness, but worth knowing:
   Forsaken's title camera, HUD, and room layout were all authored for
   **4:3** (Windows 800×600). The 3DS top screen is **5:3** (400×240).
   Rendering full-screen gives a correct 5:3 projection but cuts ~27% off
   the vertical framing — the title crate overflows, the VDUs fall out of
   frame, etc. The fix is to letterbox: render to a centered 320×240
   viewport inside the 400×240 screen, leave the side 40-pixel columns
   black. This makes the engine compute a 4:3 projection matrix naturally
   via its existing `SetFOV` math, and it matches the Windows framing
   exactly.

### Why "handedness" was a red herring

If you think about the GL→PICA path mathematically, nothing actually needs
to change for a D3D LH view matrix to produce correct view-space coordinates
on GL. Here's the reason:

- The game stores matrices **row-major** with row-vector convention
  (`v_row * M`). Every multiplication in the game uses this.
- OpenGL and citro3d expect **column-major** with column-vector convention
  (`M * v_col`).
- Reading the same 16-float memory layout under the opposite convention is
  equivalent to transposing the matrix. But the transpose is **mathematically
  the identity** for these rendering operations: `M_row · v_row` produces
  the same coordinates as `M_rowᵀ · v_col` (column form). The resulting
  clip-space coordinates are identical.

So passing the game's D3D view matrix to picaGL via `glLoadMatrixf` gives
**mathematically correct** world→view transforms. No negation, no flipping,
no reflection needed for the matrix math itself. The only places we actually
have to intervene are the two physical hardware realities above: the depth
range mapping and the culling winding.

### Why we spent time on the wrong fixes

Every one of the layered hacks we removed was solving the right **symptom**
with the wrong **cause**:

- **View.z/Look.z flip:** this was put in because the Z-row negate hack
  moved the camera to the wrong side of the scene (the reflection flipped
  where "in front" was). The camera flip partially cancelled the side
  effect. Once we remove the Z-row negate, the camera flip is not only
  unnecessary, it actively makes things wrong.
- **Modelview Z-row negate:** as explained, this was a winding-order fix
  disguised as a handedness fix, with a nasty local-Z side effect on
  asymmetric geometry.
- **Modified picaGL `fix_projection` rotation (`row[1].x = -1`):** this
  changed the upstream reflection to a proper rotation. With the rest of
  the hack stack gone, either value ends up producing the correct final
  image because the cull inversion handles winding separately. We kept
  the modified rotation because it's conceptually cleaner (det=+1,
  preserves winding).
- **`GX_TRANSFER_FLIP_VERT`:** this was needed to un-mirror the image that
  the Z-row reflection produced. With no reflection in the pipeline, there
  is no mirror to un-do, so we disable it.

Every hack made sense as a local fix for the symptom it addressed. The
problem was that they accumulated — each new hack had to work around the
side effects of the previous one, and by the end there were four layered
compensations entangled across three files and picaGL. Ripping them all
out simultaneously and applying the two real fixes (projection depth
rewrite + cull inversion) produces a pipeline where every step is
individually justifiable and nothing depends on a distant compensation.

### What this means for the native citro3d port

When porting to citro3d directly (bypassing picaGL), apply the same two
fixes at the citro3d-equivalent sites:

1. **Depth range.** Build the projection with either:
   - `C3D_Mtx` populated by `Mtx_PerspTilt` (which already handles the 3DS
     portrait→landscape rotation) — then manually rescale row 2 so
     `C3D_Mtx` produces `[−1, 0]` directly. Or:
   - Take the game's D3D `RENDERMATRIX`, apply `_33' = _33 − 1`,
     `_43' = _43` (the one-shot D3D→PICA depth remap, equivalent to
     `row[2] −= row[3]`), and upload to `C3D_FVUnifMtx4x4` for the
     projection uniform.

2. **Culling.** When the game calls `reset_cull` / `cull_cw` / `cull_none`,
   translate to `C3D_CullFace()`:
   - `reset_cull` (game wants back-face culling of CW-front meshes) →
     `C3D_CullFace(GPU_CULL_FRONT_CCW)`. Swap the enums compared to what
     the name suggests, for the same reason we inverted `glCullFace` above.
   - `cull_cw` (game wants front-face culling) →
     `C3D_CullFace(GPU_CULL_BACK_CCW)`.
   - `cull_none` → `C3D_CullFace(GPU_CULL_NONE)`.

3. **View matrix** goes straight to `C3D_FVUnifMtx4x4` unchanged (you'll
   need to transpose it to `C3D_Mtx`'s `.r[i].c[j]` convention, which is
   what the D3D→GL column-major reinterpretation already does implicitly).
   Don't touch view.z, don't touch the basis, don't scale anything.

4. **Modelview** same as view — no row/column negation. Just upload
   `world * view` and let the shader multiply.

5. **Letterbox** via `C3D_SetViewport(offset_x=40, offset_y=0, width=320,
   height=240)` on the 400×240 top screen render target, and `C3D_RenderTargetClear`
   the whole target to black once per frame so the side bars stay filled.

If you find yourself reaching for a matrix reflection or a `Z` negation
during a citro3d port, stop and ask: "am I actually solving a winding-order
problem by applying a depth/handedness operation?" If yes, the answer is
almost certainly at the `C3D_CullFace` call, not in the matrix. The PICA200
hardware will never, under any API, let you set CW as front — you either
swap the cull enum or you reauthor the meshes.

## Hardware Stereoscopic 3D

### Overview

The 3DS top screen uses an autostereoscopic parallax-barrier display driven by
two separate framebuffers: `GFX_LEFT` and `GFX_RIGHT`. Stereo depth is
controlled by the physical 3D slider (`osGet3DSliderState()`, range 0.0–1.0).

### Implementation

Stereo is implemented as **two sequential render passes** — one per eye — using
the game's existing `RenderCurrentCameraInStereo()` path:

1. **Eye separation**: `platform_get_3d_slider()` reads the slider (or a config
   override `stereo_test_slider` in `Configs/main.txt`). Eye separation is
   `slider × 30` world units (max 30 at full depth, 15 at half).

2. **Camera offset**: The camera is shifted left/right by `eye_sep / 2` along
   the camera's local X axis. The offset vector is computed by scaling the
   **first row** of `FinalMat` (`_11, _12, _13`) — which is the camera-right
   unit vector in D3D row-vector convention. `ApplyMatrix` must NOT be used
   here because it adds the translation components (`_41, _42, _43`), producing
   a world-position-scale offset instead of a direction offset.

3. **Eye routing**: `pglTransferEye(eye)` is called after each pass. Currently
   a no-op stub — on real hardware it should call `glFinish()` +
   `GX_DisplayTransfer` into the `GFX_LEFT` / `GFX_RIGHT` framebuffer.
   `gfxSet3D(true)` must also be called once per frame when stereo is active.

4. **Performance**: Two full render passes ≈ 2× CPU+GPU cost. At ~60 fps mono
   this gives ~26–30 fps stereo (measured in Mandarine). Slider = 0 disables
   stereo and restores full framerate.

### Bugs Fixed During Development

**Bug 1 — `ApplyMatrix` adds world translation to eye offset**
The original code:
```c
cam_offset.x = render_info.stereo_eye_sep / 2.0f;
cam_offset.y = cam_offset.z = 0.0f;
ApplyMatrix( &CurrentCamera.Mat, &cam_offset, &cam_offset );
```
`ApplyMatrix` computes `result = Mat._41 + Mat._1x * v`, so when `Mat` is
`FinalMat` (which includes world position in `_41/_42/_43`), the result is
`world_position + tiny_offset` ≈ ±2 billion units. Fix: multiply `_11/_12/_13`
directly, which are the rotation-only components.

**Bug 2 — hardfp ABI mismatch on `platform_get_3d_slider()` call sites**
`oct2.c` and `title.c` called `platform_get_3d_slider()` without a forward
declaration. With `-mfloat-abi=hard`, float returns go in VFP register `s0`,
but callers assumed `int` return (from `r0`). The compiler emitted
`vcvt.f32.s32` after the call, converting whatever garbage was in `r0` (~65535)
to float, giving `eye_sep = 65535 × 30 = 1,966,050`. Fix: add
`extern float platform_get_3d_slider(void)` at each call site.

**Bug 3 — `osGet3DSliderState()` returns garbage in Mandarine**
Mandarine's stub for `osGet3DSliderState()` reads from unmapped memory and
returns ~134 million. Fix: clamp the return value to `[0, 1]` in
`platform_get_3d_slider()`, and provide a `stereo_test_slider` config key in
`Configs/main.txt` for emulator testing without relying on the hardware stub.

### Emulator Notes

Mandarine has no autostereoscopic display simulation. Since `pglTransferEye`
is a no-op, both eye passes render into the same framebuffer and only the
right-eye output is visible. This is expected — stereo separation is only
observable on real hardware (or with an anaglyph compositing step added to
`pglTransferEye`).

### Future: citro3d Display-List Replay (Optimal Stereo)

The current two-pass approach doubles all draw calls. The PICA200 hardware
has no built-in camera-doubling shortcut; however, native **citro3d** (not
picaGL) supports recording a GPU command list and replaying it with a
different projection uniform — geometry is submitted once to the GPU, replayed
twice with left/right projection offsets. This is the lowest-cost stereo path
on the hardware, but requires replacing picaGL immediate mode with a
citro3d retained-mode VBO approach.

---

## ARM Unaligned Access Fixes (from OpenPandora port)

The 3DS (ARM11 ARMv6K) and OpenPandora (Cortex-A8) share the same ARM unaligned
memory access behavior. Forsaken's binary file parsers cast `char*` buffers to
`float*` after reading `u_int16_t` fields, leaving the pointer 2-byte aligned
instead of 4-byte aligned. On ARM, unaligned float dereference produces garbage
values or data aborts.

The OpenPandora port (`/tmp/forsaken-pandora/`) added `#ifdef ARM` guards using
`memcpy` at 33 sites. We ported all of these plus 1 additional site in `lights.c`
that the Pandora port also missed.

**Build flag:** `-DARM` in Makefile.3ds activates all guards.

| File | Sites | Data Protected |
|------|-------|---------------|
| bsp.c | 1 | BSP plane normal/offset |
| camera.c | 1 | Camera pos/dir/up vectors |
| extforce.c | 3 | External force fields, zone normals |
| goal.c | 2 | Goal positions, zone normals |
| **lights.c** | **1** | **POLYANIM UV application (animated textures — lava, fire)** |
| mload.c | 6 | Level vertices, cell data, UV animation loading, SoundInfo |
| mxaload.c | 6 | Animated model fire points, spot FX, vertex interpolation |
| mxload.c | 5 | Static model UV animation, fire points, spot FX |
| node.c | 1 | AI node positions/radii |
| teleport.c | 2 | Teleport positions, zone normals |
| trigarea.c | 3 | Trigger area positions, zone normals |
| triggers.c | 1 | Trigger timing |
| water.c | 3 | Water positions, sizes, fill/drain rates |

**The `lights.c` fix is critical** — without it, animated textures (lava, fire) show
garbage because the UV values are applied to vertices with an unaligned read every frame,
even though the UV *loading* in mload.c was fixed. This fix was NOT present in the
Pandora port.

## Additional 3DS-Specific Fixes

### Texture Group Overflow (2dpolys.c, polys.c, screenpolys.c)
`MAX_TEXTURE_GROUPS` = 64 on 3DS (vs 600 on PC). Explosion/death effects can spawn
enough FmPolys/Polys to overflow this in a single frame. Mid-draw flush pattern:
when `numTextureGroups >= MAX_TEXTURE_GROUPS`, flush the batch and reset counters.

### TransExe Struct Overread (transexe.c)
`AddTransExe()` copies `RENDEROBJECT` (64 groups) from `LEVELRENDEROBJECT*` (8 groups).
Fixed with partial `memcpy` of header + valid texture groups only.

### Input Double-Swap (input_3ds.c)
`handle_events()` must NOT swap `old_input`/`new_input` — `ReadInput()` in controls.c
handles the swap. Double-swapping breaks all key/button release detection, preventing
respawn after death.

### Axis Exists Flag (input_3ds.c)
`JoystickInfo[0].Axis[i].exists` must be set to `true` in `joysticks_init()`.
Without this, `ReadJoystickInput()` skips all axis processing (circle pad does nothing).

### picaGL Stereoscopic 3D (picaGL/source/picaGL.c, misc.c)
- `pglTransferEye()`: DMA-transfers color buffer to GFX_LEFT/GFX_RIGHT framebuffers.
- `pglSwapBuffers()`: Skips mono transfer when `stereo_frame=true`, swaps with stereo.
- `glClear()`: Respects `glColorMask` write mask (needed for anaglyph stereo).
- `gfxSet3D(true/false)`: Called per-frame based on slider state.

### BSP Portal Frustum (visi.c)
Skip asymmetric frustum shift when `stereo_enabled=true` on 3DS — camera offset in
`RenderCurrentCameraInStereo` already provides separation.

### Additive Blend for TransExe (render_gl1.c)
`_3ds_additive_blend_active` flag preserves `GL_SRC_ALPHA, GL_ONE` blend set by
`set_alpha_states()` during the translucent rendering pass.

### InterpFrames Load-Time Alignment (mxaload.c, mxaload.h)
The Pandora port's ARM fix for `InterpFrames()` added two `memcpy(&fromVert, FromVert,
sizeof(MXAVERT))` calls per animated vertex per frame — a **~30% framerate drop**.
The `frame_pnts` pointers point into the raw file buffer at 2-byte-aligned offsets
(after `u_int16_t` count fields), requiring the memcpy for ARM safety.

**Fix:** Allocate one contiguous 4-byte-aligned buffer at model load time, `memcpy`
all MXAVERT frame data into it once, then fixup all `frame_pnts` pointers to reference
the aligned copy. Added `aligned_verts` field to `MXALOADHEADER` (`#ifdef ARM`).
`InterpFrames()` now uses direct pointer dereference — zero per-vertex overhead.

### TransExe Memset Removal (transexe.c)
The safe partial `memcpy` in `AddTransExe()` previously did `memset(dest, 0,
sizeof(RENDEROBJECT))` before the partial copy. Since the renderer only reads up to
`numTextureGroups`, the memset of unused slots was unnecessary overhead per
translucent object per frame. Removed.

### Animated Texture Atlas Fix (tload.c, Makefile.3ds)
Level-specific BMP textures (e.g. `therma.bmp` in vol2) have different atlas layouts
than the global re-packed PNGs in `Data/textures/`. POLYANIM UVs are authored for the
original BMPs. `GetLevelTexturePath()` in `tload.c` checks level-specific files first,
falling through to global textures if not found. Without per-level PNGs, animated
textures (lava, fire) showed wrong content.

**Fix:** `Makefile.3ds` auto-converts level BMP textures to 24-bit RGB PNGs during
romfs staging. 267 BMPs converted across all levels. The PNG loader in `texture_png.c`
handles 24-bit RGB the same as 32-bit RGBA.

### GL1 Additive Blend Backport (render_gl1.c, render_gl_shared.c)
`_additive_blend_active` flag (originally `#ifdef __3DS__` only) backported to all GL1
builds. `set_alpha_ignore()` and `unset_alpha_ignore()` check the flag unconditionally
to preserve `GL_SRC_ALPHA, GL_ONE` blending during the translucent rendering pass.
Fixes explosion/projectile rendering on PC GL1 as well.

### Controls (config.c, input_3ds.c)
- L/R shoulder buttons mapped to strafe left/right (not roll)
- Circle pad axis scale: `100.0f / 155.0f` (engine expects ±100, not ±32767)
- Deadzone applied in `handle_events()` matching SDL's `app_joy_axis` pattern
- D-pad mapped as POV hat for weapon selection (not key_state, which would override camera)
- X = forward, Y = backward, A = fire primary, B = fire secondary
- Start+Select = quit

### Debug Logging
All diagnostic file logging removed (forsaken_cpad.log, forsaken_death.log,
forsaken_polyanim.log, forsaken_polyanim_post.log, forsaken_mload_bind.log,
forsaken_tloadindex.log, forsaken_texload.log, forsaken_texids.log). The `trace()`
infrastructure in `main_3ds.c` compiles to a no-op unless `__3DS_DEBUG__` is defined.

## Native Citro3D Renderer (render_c3d.c)

Alternative renderer bypassing picaGL for direct PICA200 GPU access. Branch:
`3ds-citro3d`. Toggle: `make -f Makefile.3ds RENDERER=citro3d`.

### Build Architecture
- picaGL is **not linked** for the citro3d path
- `render_c3d.c` implements all 41 functions from `render_gl1.c` + `render_gl_shared.c`
- Own `pglInit`/`pglSwapBuffers`/`pglExit` stubs provided
- Vertex shader: `shaders/render_c3d.v.pica` (12 instructions, MVP transform)
- Shader binary embedded via `render_c3d_shbin.h` (compiled with picasso)

### Frame Lifecycle
```
pglInit()           → C3D_Init()
c3d_renderer_init() → C3D_RenderTargetCreate + C3D_RenderTargetSetOutput + shader load
FSBeginScene()      → C3D_FrameBegin + C3D_RenderTargetClear + C3D_FrameDrawOn
                      + C3D_BindProgram + GPU state init (AttrInfo, TexEnv, depth, cull)
                      + modelView uniform = identity
draw_render_object()→ Convert LVERTEX → gpu_vertex_t, upload MVP, C3D_DrawArrays
pglSwapBuffers()    → C3D_FrameEnd (triggers auto display transfer)
```

### Critical Findings

**pglSwapBuffers conflict:** picaGL's `pglSwapBuffers` calls `glFinish` →
`GX_DisplayTransfer` → `gfxScreenSwapBuffers`. This conflicts with citro3d's
`C3D_RenderTargetSetOutput` auto-transfer triggered by `C3D_FrameEnd`. The
result is a black screen. Solution: override `pglSwapBuffers` to only call
`C3D_FrameEnd` when a frame is active.

**GPU state initialization:** The PICA200 GPU registers for attribute layout
(`AttrInfo`), texture environment (`TexEnv`), depth test, and cull face must
be explicitly set in `FSBeginScene` after `C3D_FrameDrawOn`. Uninitialized
registers produce invisible geometry (no crash, no error, just no output).

**Uniform initialization:** The vertex shader reads `modelView[4]` (uniform
registers 4-7) and `projection[4]` (registers 0-3). `upload_matrices()` writes
combined MVP to registers 0-3. Registers 4-7 must be explicitly set to identity
in `FSBeginScene` — uninitialized uniforms contain garbage from the previous
frame or from picaGL's setup.

**Matrix convention:** Engine matrices are column-major (see `render_gl_shared.c`
line 929). `matrix_to_c3d()` transposes using the picaGL `glLoadMatrixf` pattern:
`r[i].x = m[0+i], r[i].y = m[4+i], r[i].z = m[8+i], r[i].w = m[12+i]`.

**Depth remap:** D3D [0,1] → PICA [-1,0] via `row[2] -= row[3]` on the
combined MVP matrix. NOT the two-step D3D→GL→PICA conversion.

**Screen rotation:** 90° tilt for portrait→landscape: swap rows 0 and 1,
negate new row 1. Matches picaGL's `matrix4x4_fix_projection`.

**VRAM budget:** GPU_RGBA8 (32-bit) textures exhaust VRAM after ~7 1024x1024
textures. GPU_RGBA4 (16-bit) halves VRAM usage, matching picaGL's default.

**Vertex format:** `gpu_vertex_t` = `{float pos[3], float color[4], float
texcoord[2]}` = 36 bytes. Colors pre-normalized from LVERTEX's packed u32 ARGB.
256KB `linearAlloc` scratch buffer holds ~7000 vertices per frame.

### Completed Phases
1. Scaffold: render_c3d.c skeleton, vertex shader, Makefile toggle
2. GPU buffers + draw path: linearAlloc, C3D_DrawArrays, state management
3. Textures: GPU_RGBA4 Morton tiling, C3D_TexBind, C3D_TexEnv combiner

### Portal Viewport Fix (render_c3d.c)
`FSSetViewPort` was storing the viewport but never calling `C3D_SetViewport`,
causing portal geometry to render at full screen regardless of the BSP group's
clipped viewport extent. This produced "wobbly" portals that shifted as the
camera moved and eventually caused crashes from out-of-bounds rendering.

**Fix:** Map game viewport (landscape, top-left origin) to PICA200 viewport
(portrait framebuffer) matching picaGL's exact transform chain:
```
GL: bottom = screenH - Y - Height; glViewport(X, bottom, W, H)
picaGL: viewportX = (400 - W) - X; viewportY = bottom
PICA: _picaViewport(viewportY, viewportX, H, W)
Result: C3D_SetViewport(bottom, (400-W)-X, H, W)
```

### Remaining Phases
4. 2D/HUD rendering polish
5. Single-pass stereo (render list + projection swap — the main perf goal)
6. GPU lighting (move light_vert to vertex shader)

## Known Remaining Issues

1. **No save support** — romfs is read-only; need sdmc redirect for pilot/config saves
2. **Rear camera disabled** — PICA200 can't sustain two full render passes at playable FPS
3. **No multiplayer** — Networking stubbed
4. **Citro3d renderer: no per-vertex lighting** — light_vert sets full-bright white
5. **Citro3d renderer: no stereo** — single-pass stereo not yet implemented
