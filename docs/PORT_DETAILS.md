# Forsaken 3DS — port details

The technical side of the port: how the renderer, audio, memory and data
pipelines work on the 3DS, and what was changed in the 1998 engine to get
there. For building and installing, see the [README](../README.md).

## Source layout

```
forsaken_core/   the game, shared by both platforms
forsaken_3ds/    3DS layer: platform, input, audio, citro3d renderer, shaders
forsaken_pc/     PC layer (development/comparison target): SDL2, OpenAL, Vulkan
tests/           host unit tests (make -C tests)
tools/           build matrix, verification, perf gates, asset pipelines
assets/          icons, banners, RSFs, level orderings, 1998 engine files
scripts/         the game's Lua scripts
build/           everything make writes (3ds/, pc/)
```

The engine talks to each platform through a small set of seam headers
(`rendering/render.h`, `util/platform.h`, `audio/sound.h`, `net/net.h`,
`input/input.h`); core code never includes a platform header.

## Renderer (citro3d)

`forsaken_3ds/src/rendering/render_c3d.cpp` implements the engine's `FS*` /
`draw_*` renderer interface directly on citro3d. The overview comment at the
top of that file is the reference; in short:

- **Submission.** Engine vertices (`LVERTEX`, packed colour) are converted to
  a shader vertex (`gpu_vertex_t`, float colour) in a 4 MB linear scratch and
  drawn with `C3D_DrawArrays`. The GPU command buffer is 4 MB (16× the
  citro3d default) for laser- and explosion-heavy frames.
- **Stereoscopic 3D.** Left and right render targets; the 3D slider drives eye
  separation. A stereo memo lets the right eye resubmit vertices the left eye
  already converted, keyed by batch so no draw is ever skipped. In mono,
  level walls stay converted in retained buffers until a texture animation
  step.
- **Lighting on the GPU.** The vertex shader (`shaders/render_c3d.v.pica`)
  adds ambient plus up to 8 point lights (distance falloff) to each vertex's
  baked colour, for both the level mesh and models. When more lights are
  visible (up to ~60 in heavy fights) the 8 nearest the camera win. The
  shader gets world-space positions from a separate model→world uniform, so
  models need no CPU pre-transform.
- **Object shine.** Enemies, pickups and mines get a per-pixel Phong
  highlight from PICA200 fragment lighting, tinted by the level's lighting
  at the object. Doors and background objects are excluded on all three
  model draw paths.
- **Wall detail.** Walls get PICA200 ProcTex grain in one of three material
  classes (metal, rock, organic), chosen by texture name, with per-mesh
  and per-texture phase variation so adjacent walls don't tile identically.
  Both features are toggles in Pause → Options → Visuals, saved in
  `configs/main.txt`, and cost nothing when off.
- **Portal precision.** The engine renders each visible room through a
  sub-rect viewport with its own magnified projection; on PICA200 that
  produced 1–2 px cracks and flicker at room boundaries. On 3DS every
  visible room shares the camera's projection and a GPU scissor masks it to
  its rect, so shared vertices land on identical pixels. Portals cut by the
  near plane get the screen bounds of their visible fragment
  (`world/clipmath.h`) instead of the whole window.

## Textures

- **HD packs.** Textures load as `.t3x` (ETC1 for walls, ETC1A4 where alpha
  is needed) from one of two packs in the romfs: `hd_textures/` (512×512)
  and `hd_textures_lomem/` (256×256). The memory tier picks the pack (below);
  the other is the fallback. A texture with no `.t3x` loads from its PNG,
  converted to RGBA4444.
- **Gaussian mipmaps.** The 4K source pack is AI-upscaled and carries
  scattered outlier pixels; bilinear mips amplify them into sparkle on
  distant walls, Gaussian mips (`tex3ds -m gaussian`) dissolve them.
- **Alpha detection** uses the original BMP's palette (presence of pure
  black), not the upscaled image, which has stray black pixels everywhere.
- **Atlases.** Per-level textures take their layout from the 4K pack; global
  UI/sprite textures keep the original layout, since the 4K versions moved
  things and broke the engine's UV maths.
- PNGs that have a `.t3x` are left out of the romfs (the loader never opens
  them); `FSTextureExists` lets the level-texture path choice accept either.
  The original `.bmp` textures are not shipped either: the build converts
  the ones without a `.t3x` to PNG, and the game only opens `.png` / `.t3x`.

Pipeline: `tools/texture/regen_hd_textures_4k.sh`.

## Audio

- **Music** streams DSP-ADPCM tracks from the romfs in 0.5 s chunks through a
  4-buffer queue, refilled by a background thread every 25 ms and decoded by
  the DSP at no CPU cost. Each buffer carries its own ADPCM context, so there
  are no glitches at buffer boundaries. Track length comes from the file
  size, not the DSP header (encoders disagree about the header's nibble
  count).
- **Sound effects** use at most 8 DSP voices, evicting the oldest. Too many
  active voices made the DSP skip audio and cost frame rate.
- **DSP firmware.** `ndspInit` needs `sdmc:/3ds/dspfirm.cdc`. Real hardware
  running custom firmware has it from the standard DSP1 dump; the CIA does not
  ship it, and shows an explanation screen if it is missing. A development
  `.3dsx` can bundle `assets/dspfirm.cdc` (from your own console) and copies it
  to the SD card on first launch, for emulator testing.

## Memory

One CIA works on every 3DS. At start-up the game asks the OS how much memory
it was given (`osGetMemRegionSize`) and picks a tier:

| Console / build | App memory | Heap (malloc + linear) | Texture pack |
|---|---|---|---|
| New 3DS (CIA, `SystemModeExt: 124MB`) | 124 MB | 32 + 32 MB | 512×512 |
| Old 3DS, HIMEM CIA (`SystemMode: 96MB`) | 96 MB | 32 + 32 MB | 512×512 |
| Old 3DS, standard CIA | 64 MB | 32 + 22 MB | 256×256 |

The standard CIA does not request Old 3DS HIMEM, because some HOME menus crash
granting it; the separate HIMEM CIA does, for consoles where it works. A
`.3dsx` started from the Homebrew Launcher gets less memory than an installed
title; it runs on a New 3DS, but an Old 3DS should use the CIA.

What made the Old 3DS fit:

- A Quake 3-style two-tag hunk allocator (`util/hunk_alloc.cpp`): one buffer,
  start-up data from one end, per-level data from the other, freed in one
  call on level change. With per-level model-header arrays sized to the
  level, this recovered ~20 MB of static memory.
- Loader leaks fixed: each loaded mesh's copy of its baked vertex colours
  was never freed, ~3–5 MB per level transition, which ran the heap out
  after a few levels.
- File buffers are freed after loading (5.8–7.9 MB per level), since
  everything kept is copied into aligned memory (below).

## Engine changes for ARM11

- **Unaligned data.** The 3DS's ARM11 faults on misaligned VFP loads and
  load-multiples, and Forsaken's packed file formats put float data at
  2-byte-aligned offsets all the time. Every loader now reads through
  `util/file_reader.h` (`FileReader`), which returns each field by value
  through `memcpy`, so the compiler only emits loads that are safe at any
  address.
- **Hardware square root.** ARM11 VFPv2 has `vsqrt`/`vdiv`; the Quake III
  inverse-square-root bit hack was slower there (and less accurate) and was
  removed.
- **Texture groups.** `RENDEROBJECT`s hold texture-group arrays sized at load
  time from the hunk; the per-frame poly batchers flush mid-draw when their
  64-group buffer fills.
- **Animated models.** `.mxa` frame vertices are copied once at load into one
  aligned block, instead of being copied per vertex per frame.

## Game data and editions

Game data is not included; the extractors build `data/` from a disc or Steam
copy you own.

| | `make -f Makefile.3ds` (1998) | `EDITION=remaster` |
|---|---|---|
| Source | 1998 disc (`tools/assets/extract_assets.py`) | Forsaken Remastered's `ForsakenEX.kpf` (`tools/assets/extract_remaster_levels.py`) |
| Single-player | 15 levels | 23 levels + 7 N64 secret levels |
| Multiplayer arenas | 30 | 24 |
| Music | 9 CD tracks | 18 tracks (9 CD + 9 converted from the Remaster's OGGs) |

The Remaster pipeline:

- converts the KPF into the 1998 engine's layout;
- adds the N64-port enemy, pickup and boss meshes, and downscales the
  loading-screen art to crate-menu banners;
- supplies the ~110 KB of 1998 engine files the KPF lacks from
  `assets/engine_runtime_1998/`.

Night Dive's KEX engine does visibility at runtime, so the levels it authored
ship without the 1998 engine's baked visibility tables. Without them you get
black walls through doorways and no blaster light. `mxv_visi_repair.py`
rebuilds those tables offline at extract time; levels that already have them
are left alone. `EDITION=remaster` also selects the Remaster's level order,
music table and the N64 enemy and pickup entries.

## Known limitations

- Most N64 bosses (Manmech, Maldroid, DreadNaught, ShieldTurret) use their
  Remaster meshes but the closest 1998 AI brain; the KEX brains are not ported
  yet. Boss_Ramqan's jumping AI is ported (`ai/aijump.cpp`).
- `biolab` is left out of the level rotation: its AI node graph sends ground
  enemies through walls the collision data doesn't contain.
- `tuben64` is also left out: its boss alternated between tank and Hunter
  visuals (a model-pointer corruption whose root cause was not isolated), so
  it was dropped rather than shipped broken.
- No multiplayer on 3DS.
