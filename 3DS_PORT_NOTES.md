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

### 17. D3D Left-Handed to GL Right-Handed Conversion (`render_gl_shared.c`) ★ KEY FIX
**Problem:** The game's matrices are built for D3D's left-handed coordinate system
where +Z points into the screen. PICA200 (via picaGL or citro3d) uses right-handed
coordinates where -Z points into the screen. Without conversion, the scene renders
"inside-out" — back faces appear in front of front faces, geometry looks inverted.
This issue affects BOTH picaGL and native citro3d rendering paths.
**Fix:** Negate the Z basis vector (3rd row, elements [8..11]) of the modelview matrix
in `reset_modelview()` before passing to `glLoadMatrixf()`. This flips the Z axis,
converting D3D left-handed to GL right-handed. In the citro3d path, the same negation
should be applied to row[2] of the C3D_Mtx modelview before upload.

### 18. picaGL D3D Depth Range Detection (`picaGL/source/utils/math_utils.c`) ★ KEY FIX
**Problem:** picaGL's `fix_projection` assumed GL projection convention (depth [-1,1])
and applied `z' = 0.5z - 0.5w` to remap to PICA's [-1,0]. But Forsaken uses a D3D
projection (depth [0,1]) which needs `z' = z - w`. The wrong remap caused depth
buffer issues.
**Fix:** Auto-detect D3D vs GL by checking `row[3].z` sign (+1 = D3D, -1 = GL) and
apply the correct depth remap: `z - w` for D3D, `0.5z - 0.5w` for GL.

### 19. Skip to New Menu System (`oct2.c`) — REVERTED
**Tried:** On 3DS, `MenuRestart(&MENU_NEW_Start)` to skip straight to disc menu.
**Why reverted:** This skipped the camera's start→disc zoom animation, which
is needed for the camera position to be correct. Keep the original `MENU_Start`.

### 20. Camera Handedness Fix via LookAt (`title.c` DisplayTitle) ★ KEY FIX
**Problem:** The Z basis row negate in `reset_modelview` fixes inside-out geometry
but leaves the camera on the WRONG SIDE of the scene (looking at the back of objects).
The camera's orientation is built via `MakeViewMatrix(View, Look, Up, Mat)` — a
classic LookAt function. The direction vector `Look - View` determines the camera's
forward basis. In D3D convention, this produces a left-handed basis; in GL
convention, right-handed.
**Fix:** Negate BOTH `View.z` (camera position) AND `Look.z` (target position)
before calling `MakeViewMatrix`. Both Z values must be flipped together so the
direction vector `Look - View` is preserved — only the absolute positions
mirror across the XY plane. This puts the camera on the opposite side of the
scene while maintaining the correct relative orientation to its target. Because
`MakeViewMatrix` is called every frame during camera animations (the start→disc
zoom), the fix works continuously throughout the title room pan.

```c
#ifdef __3DS__
VECTOR View_flipped = View;
VECTOR Look_flipped = Look;
View_flipped.z = -View.z;
Look_flipped.z = -Look.z;
MakeViewMatrix(&View_flipped, &Look_flipped, &Up, &CurrentCamera.Mat);
MatrixTranspose( &CurrentCamera.Mat, &CurrentCamera.InvMat );
CurrentCamera.Pos = View_flipped;
#else
MakeViewMatrix(&View, &Look, &Up, &CurrentCamera.Mat);
MatrixTranspose( &CurrentCamera.Mat, &CurrentCamera.InvMat );
CurrentCamera.Pos = View;
#endif
```

### 21. Horizontal Mirror Fix via Hardware Display Flip (`picaGL/source/picaGL.c`) ★★ CRITICAL FIX ★★
**Problem:** The modelview Z row negate is mathematically a reflection (one axis
negated = determinant becomes -1). Reflections fix the inside-out appearance of
geometry (handedness conversion) but have an unavoidable side effect: the final
scene appears horizontally mirrored on screen. Text reads backwards.

We tried MANY approaches to un-mirror via matrix math (all failed, each one
broke a different thing):
  - Negate `row[0].y` in `fix_projection` → made scene inside-out
  - Negate `row[1].x` in `fix_projection` → made scene inside-out
  - Negate both X and Z rows of modelview (180° rotation) → camera on wrong side again
  - Negate `_11` in projection → broke depth
  - Negate X column of modelview → broke geometry
  - Flip View/Look.x along with Z in LookAt → camera in wrong place

**The math reason:** a single reflection changes handedness (fixes inside-out),
a second reflection would cancel it back (breaks handedness again), and a rotation
(two reflections composed) preserves handedness but doesn't fix the mirror. You
cannot fix inside-out AND un-mirror using pure matrix operations on the same pipeline.

**THE SOLUTION: Hardware display-level flip.** The 3DS's `GX_DisplayTransfer`
hardware unit supports a vertical flip (`GX_TRANSFER_FLIP_VERT(1)`) during the
copy from the framebuffer to the physical display. Because picaGL renders to a
240×400 PORTRAIT framebuffer that the display hardware rotates to 400×240
LANDSCAPE for output, a vertical flip on the portrait buffer corresponds to a
HORIZONTAL flip on the final landscape image.

This is a **pure display-level operation** that runs after all matrix math is
done. It doesn't interact with the rendering pipeline at all. The GPU renders
the already-handedness-corrected (but mirrored) scene normally, and the display
hardware flips it as it sends pixels to the screen.

```c
/* In picaGL/source/picaGL.c pglSwapBuffers() */
if(pglState->display == GFX_TOP)
{
    GX_DisplayTransfer(
        (u32*)pglState->colorBuffer, GX_BUFFER_DIM(240, 400),
        output_framebuffer, GX_BUFFER_DIM(240, 400),
        GX_TRANSFER_OUT_FORMAT(output_format) | GX_TRANSFER_FLIP_VERT(1));
}
else
{
    GX_DisplayTransfer(
        (u32*)pglState->colorBuffer + (240*80),GX_BUFFER_DIM(240, 320),
        output_framebuffer, GX_BUFFER_DIM(240, 320),
        GX_TRANSFER_OUT_FORMAT(output_format) | GX_TRANSFER_FLIP_VERT(1));
}
```

**Key insight:** the 3DS screen rotation is the trick. Because the physical
framebuffer is portrait but displayed as landscape, the hardware vertical flip
on the portrait axis becomes a horizontal flip on screen. This lets us fix the
mirror without ANY matrix manipulation — it's purely a display-time operation
that runs at zero cost.

## Key Learnings About Camera Handedness

The camera has its own handedness that's embedded in how LookAt functions compute
the view matrix. When `MakeViewMatrix(eye, target, up)` builds the rotation:
- D3D: forward = normalize(target - eye), right = cross(up, forward), up = cross(forward, right)
  — this creates a left-handed basis where +Z points INTO the screen
- GL: forward = normalize(eye - target), right = cross(up, forward), up = cross(forward, right)
  — this creates a right-handed basis where -Z points INTO the screen

The ForsakenX engine uses the D3D convention. Passing the D3D view matrix
directly to picaGL causes the camera to look the WRONG way because picaGL's
`fix_projection` applies GL-convention transforms on top of it. Flipping the
input View/Look Z coordinates before MakeViewMatrix effectively changes which
convention is used, producing a GL-compatible view matrix.

**Side effects:** The Z flip of view/look produces a scene that is horizontally
mirrored (determinant of the total transform changes sign). This is corrected
by negating `row[0].y` in picaGL's `fix_projection` 90° tilt rotation.

### 22. FOV / Aspect Ratio Swap for Screen Rotation (`oct2.c` SetFOV) ★ KEY FIX
**Problem:** picaGL's `fix_projection` applies a 90° screen rotation that swaps
the projection's X and Y output axes (to convert from landscape rendering to
the 3DS's portrait framebuffer). The game's `SetFOV()` computes `proj._11` (X
scale) using `viewport.Width` (400) and `proj._22` (Y scale) using
`viewport.Height` (240). After the rotation swap, `_11` ends up on the screen
Y axis and `_22` ends up on the screen X axis — the aspect ratio is inverted.
Effect: the horizontal FOV becomes effectively ~55° instead of 90°, making
objects appear zoomed in by ~1.67x.
**Fix:** On 3DS, swap Width and Height in the projection scale calculation
and invert the pixel aspect ratio. After picaGL's rotation swap, the scales
land on the correct screen axes.

```c
#ifdef __3DS__
/* Pre-swap to compensate for picaGL's 90° rotation */
pixel_aspect_ratio = 1.0f / (render_info.aspect_ratio * screen_height / screen_width);
viewplane_distance = (float)( viewport.Height / ( 2 * tan( DEG2RAD(fov) * 0.5 ) ) );
proj._11 = 2 * viewplane_distance / viewport.Height;
proj._22 = 2 * viewplane_distance / ( viewport.Width / pixel_aspect_ratio );
#else
pixel_aspect_ratio = render_info.aspect_ratio * screen_height / screen_width;
viewplane_distance = (float)( viewport.Width / ( 2 * tan( DEG2RAD(fov) * 0.5 ) ) );
proj._11 = 2 * viewplane_distance / viewport.Width;
proj._22 = 2 * viewplane_distance / ( viewport.Height / pixel_aspect_ratio );
#endif
```

## Key Concepts for PICA200 Porting

### D3D vs OpenGL Coordinate Handedness
D3D uses a **left-handed** coordinate system (+Z into screen). OpenGL/PICA200 uses
**right-handed** (-Z into screen). Games ported from D3D will render inside-out unless
the Z axis is negated in the modelview matrix. This affects:
- picaGL path: negate row 2 of modelview before `glLoadMatrixf`
- citro3d path: negate `r[2]` of C3D_Mtx modelview before `C3D_FVUnifMtx4x4`

### D3D vs OpenGL Depth Range
D3D projection outputs depth in [0, 1]. OpenGL outputs [-1, 1]. PICA200 needs [-1, 0].
- D3D → PICA: `z_pica = z_d3d - 1` (or matrix: `row[2] -= row[3]`)
- GL → PICA: `z_pica = 0.5 * z_gl - 0.5` (picaGL's default `fix_projection`)

## Known Remaining Issues

1. **Texture loading speed** - ~2 minutes on emulated ARM (2048x2048 PNG decompression)
2. **No save support** - romfs is read-only; need sdmc redirect for pilot/config saves
3. **Input navigation** - HID input connected but menu navigation needs testing
4. **Sound not tested** - ndsp init code present but audio playback not verified
5. **Lighting** - Scene may appear darker than reference; vertex colors may need adjustment
