# Forsaken 3DS

A native Nintendo 3DS port of [ForsakenX](https://github.com/ForsakenX/forsaken),
the community source port of Acclaim's 1998 six-degrees-of-freedom space combat
game *Forsaken*.

A complete, playable build with a custom GPU renderer, hardware stereoscopic
3D, the full single-player campaign, and HD-source textures sized to fit
PICA200's budget. Holds a mostly stable 60 fps in stereoscopic 3D at 512×512
texture resolution on **both Old and New 3DS** — the original game's draw
counts are tiny by modern standards, and the close-to-metal citro3d
submission plus on-GPU per-vertex lighting leave plenty of headroom on
either platform.

## Highlights

- **Hardware stereoscopic 3D.** Two render targets (left/right) bound per
  frame; the engine's per-eye callback runs fully for each. The 3D slider
  drives eye separation. ETC1 textures + single-tap fragment shading
  leave plenty of GPU headroom on both Old and New 3DS.
- **HD textures (ETC1 / ETC1A4) with Gaussian mipmaps.** Per-level walls
  upscaled from a 4K source pack, downscaled to 512×512 ETC1 with Gaussian
  mip chains — the wider kernel dissolves AI-upscaler hallucination pixels
  that bilinear mips would amplify into sparkle on distant walls. Sprites
  and grates use ETC1A4 with an alpha-from-original-palette detector so
  see-through pixels survive. The full 512×512 pack loads on **both Old
  and New 3DS** post-Q3-memory-refactor — the recovered ~20 MB BSS gave
  OG enough linear-heap headroom that the earlier OG-only mip-0 strip
  is no longer needed.
- **Threaded DSP-ADPCM music.** Tracks are streamed from romfs in 0.5 s
  chunks on a background thread (25 ms tick), decoded by the DSP hardware
  for zero CPU cost. Per-buffer ADPCM context tracking eliminates
  buffer-boundary glitches.
- **Concurrent-voice cap on the SFX pool** (8 voices, oldest-evict). PICA200's
  DSP firmware mixes channels in software and starts skipping audio — and
  burning ARM cycles — when too many voices are active. The cap recovered
  the music channel under heavy combat AND took back ~15 fps.
- **Hybrid GPU vertex lighting.** A custom PICA200 vshader does the
  per-vertex point-light contribution for **both** the level mesh and
  animated/static model meshes — ambient + per-light distance falloff
  for up to 8 simultaneous lights, sorted by camera distance so the
  closest lights win when the visible-light count exceeds the slot
  budget (observed up to 59 in dense firefights). The vshader reads
  world-space position from a separate `model→world` uniform, so
  ship/enemy/pickup models that pass vertices in MODEL space get
  correctly world-spaced light distances without a CPU pre-transform.
  Frees the CPU's per-vertex lighting cycles for OG 3DS in particular,
  where the pre-GPU-lighting path was the dominant bottleneck.
- **Boss_Ramqan jump AI port.** The Forsaken Remastered N64-port enemy
  Boss_Ramqan uses a parametric-arc hop brain (`kexForsakenAIBrainJump`)
  that has no 1998 counterpart; the boss is supposed to leap between
  rock pads in his lava lair. Constants extracted from the Remaster
  binary (.rodata), algorithm decompiled and ported to a 1998-style
  state machine in `aijump.c`. Lands on `SolidPos` (Nodeload's
  floor-snapped target), apex set from the authored `Pos.y` (above the
  pad), with a 30%-of-horizontal-distance fallback for KEX-authored
  decomp-to-1998 mapping.
- **Visibility data baked offline.** Forsaken Remastered's KEX-engine cooker
  ships flat per-portal VISTREE + zeroed `IndirectVisibleGroup` for the 22
  Night-Dive-authored levels (KEX uses runtime portal-frustum culling, not
  the 1998 PVS). The 1998 engine reads zeros as "no visibility" → black voids
  through doorways + missing dynamic blaster light. `mxv_visi_repair.py`
  rebuilds both the recursive VISTREE and the flat tables at extract time
  via BFS over the preserved portal graph. Runtime engine reads healthy
  data on every level — no flood-fill, no per-frame BFS.

## Quick start

### Build the 3DSX (homebrew launcher)

```bash
# Set devkitPro environment (skip if already in your shell profile)
export DEVKITPRO=/opt/devkitpro
export DEVKITARM=$DEVKITPRO/devkitARM

# Default renderer is citro3d (native, hardware stereo).
# `RENDERER=picagl` builds the mono fallback path.
make -f Makefile.3ds

# Output: forsaken3ds.3dsx
```

### Build a CIA (installable home-menu title)

```bash
# One-time: fetch makerom + bannertool (~5 MB)
make -f Makefile.3ds tools-fetch

# CIA builds do NOT bundle dspfirm.cdc — CFW users on real hardware
# already have sdmc:/3ds/dspfirm.cdc from the standard DSP1 dump step
# (https://github.com/zoogie/DSP1). The .3dsx path does bundle it
# (from assets/dspfirm.cdc) so emulator testing is zero-friction:
# sound_init() auto-copies it to the virtual SD on first launch.
# Either way, dump the blob from your own 3DS before building the
# .3dsx and place it at assets/dspfirm.cdc.

make -f Makefile.3ds cia

# Output: forsaken3ds.cia (~279 MB; install via FBI on your 3DS)
```

See [`assets/README.md`](assets/README.md) for asset / CIA pipeline details.

## Game data

Forsaken's level / model / sound data is not included. Provide it via the
included extraction script — pointed at a legitimate disc image you already
own:

```bash
sudo apt install p7zip-full bchunk ffmpeg
pip install --user yt-dlp demucs   # only if you want to rebuild banner audio

# From a BIN/CUE (gives you music too)
python3 extract_assets.py "Forsaken (USA).bin"

# Or from an ISO (game data only — no CD audio)
python3 extract_assets.py "Forsaken (USA).iso"
```

The script extracts game data, converts CD audio to DSP-ADPCM (via
`gc-dspadpcm-encode` if available, falling back to WAV), lowercases all
filenames, and populates `romfs/`.

### Forsaken Remastered build flavour (optional)

The default build (`make -f Makefile.3ds`) ships the 1998 game: 15-level
SP campaign, 30 MP arenas, 9 CD-audio tracks. The **Remaster flavour**
adds 28 extra levels (9 SP, 8 N64-secret, 11 MP), 9 new music tracks,
N64-specific enemy/pickup/boss meshes, and the Remaster's colorized
crate-menu banners. Two source-of-truth pipelines, pick one:

**Option A: Build from the Remaster KPF (no 1998 ISO needed).**

```bash
sudo apt install p7zip-full bchunk ffmpeg
python3 extract_remaster_levels.py \
    ~/.steam/steam/steamapps/common/Forsaken\ Remastered/ForsakenEX.kpf
make -f Makefile.3ds EDITION=remaster
```

The KPF is mostly a superset of the 1998 game data — for the levels
the 1998 game shipped, every `.mx`/`.cob`/`.bsp`/`.mxv`/texture/sound
is byte-for-byte identical to the 1998 originals. Levels Night Dive
authored from scratch (defend2, stableizers, powerdown, plus the rest
of the 21 KEX-original maps that ship in the port — biolab dropped
with stripped visibility data — KEX's renderer uses runtime
portal-frustum culling and doesn't need the 1998 engine's pre-baked
PVS tables or recursive VISTREE.
`extract_remaster_levels.py` runs `mxv_visi_repair.py` as its final
pass to rebuild both at extract time so the 1998 engine sees a
healthy file. Idempotent and gated on the data signal — runs only on
"Visibility-Table Repair" for the algorithm.

The script also bulk-extracts every asset into `Data/`, downscales
the Remaster's 1280×720 loading-screen art to 256×128 crate-menu
banners, and converts the OGG-only tracks 10–18 to DSP-ADPCM. The
~110 KB of 1998-engine-runtime data KEX dropped (`.off` sprite-
offsets, font files, `enemies.txt`/`statsmessages.txt` engine config,
the splash icon) ships with the port in `assets/engine_runtime_1998/`
and is copied automatically.

**Option B: Build from the 1998 ISO + KPF overlay.**

```bash
sudo apt install p7zip-full bchunk ffmpeg
python3 extract_assets.py "Forsaken (USA).bin"
python3 extract_remaster_levels.py --skip-bulk --skip-runtime \
    ~/.steam/steam/steamapps/common/Forsaken\ Remastered/ForsakenEX.kpf
make -f Makefile.3ds EDITION=remaster
```

Use this if you'd rather seed `Data/` from a 1998 ISO you already own
and just layer the Remaster additions on top. Functionally equivalent
to Option A.

`EDITION=remaster` does two things at build time:

- Defines `-DEDITION_REMASTER`, which switches `music_3ds.c`'s
  level→track mapping to a 32-entry table keyed on the Remaster's
  authoritative `defs/mapInfo.txt` curation, and activates runtime
  template-copy entries for the new N64 enemy/pickup IDs (100–108
  enemies, 100–105 pickups) so they render with real Remaster meshes
  and behave with the closest-matching 1998 AI brain.
- Substitutes `Data/Levels/mission_remaster.dat` (24 SP campaign
  entries + 8 N64 secret levels) and `Data/Levels/battle_remaster.dat`
  (24 MP arenas) over the 1998 originals during ROMFS staging, so the
  in-game crate menu sees the expanded line-up.

Both `_remaster.dat` files and the original `mission.dat` / `battle.dat`
are force-tracked in git, so a fresh clone has both orderings ready and
the build flag picks one.

#### What's included in each flavour

|                          | `make`                    | `make EDITION=remaster`     |
|--------------------------|---------------------------|------------------------------|
| Single-player campaign   | 15 levels (1998)          | 23 levels (Remaster) + 8 N64 secrets |
| Multiplayer arenas       | 30                        | 24 (Remaster's curated set)  |
| Music tracks             | 9 (`track02-10.dsp`)      | 18 (`track02-19.dsp`)        |
| Level→track mapping      | 1998 CD                   | Remaster `mapInfo.txt`       |
| Level data fidelity      | Lossless from CD          | Lossless from CD for shared levels; KEX-authored levels get visibility tables/VISTREE rebuilt offline |
| New-track audio source   | n/a                       | OGG Vorbis 256 kbps → DSP-ADPCM (lossy) |

The new music tracks (10–18 in the OGG library, surfaced as
`track11.dsp`–`track19.dsp` on disk) are `Labyrinth`, `Nubia`,
`Pyrolite`, `The Dead System [Force Of Angels Remix]`, four N64
arrangements (`The Dead System`, `Condemned`, `Pure Power`,
`Flame Out`), and `Cyclotron`. Tracks 1–9 of the Remaster's OGG library
are the same compositions as the 1998 CD's tracks 02–10, so the
existing CD-rip DSPs are kept as-is — lossless source preserved where
it exists. There's no legitimate lossless release of tracks 10–18; the
OGG → DSP-ADPCM path is the highest-fidelity option available for them
to end users.

**Per-level RAM cost is unchanged** in either flavour — only one level
loads at a time, and the new MXV / BSP sizes are within the existing
per-level envelope (largest new map: `temple.mxv` 1.7 MB, comparable
to the existing largest 1998 map `thermal.mxv` at 1.77 MB). On-SD
storage for the Remaster build grows by ~38 MB for level data,
~5–10 MB for per-level texture PNGs, and ~36 MB for the 9 new music
tracks.

### HD textures (optional, recommended)

Source pack: **[Upscaled Forsaken Textures — 4K Texture Pack][4kpack]**
on Moddb (~7.5 GB).

[4kpack]: https://www.moddb.com/mods/upscaled-forsaken-textures/addons/4k-texture-pack

Moddb is behind Cloudflare's bot challenge so the download has to be done
manually in a browser. Save the `.rar` anywhere convenient — the regen
script auto-detects:

```
./4ktexturepack.rar          (project root)
./assets/4ktexturepack.rar
~/Downloads/4ktexturepack.rar
$FORSAKEN_4K_PACK            (env var override)
```

Then:

```bash
sudo apt install unrar unzip imagemagick
./regen_hd_textures_4k.sh
```

The script extracts the pack (one-time, cached at `/tmp/4k_pack/`), then
generates ETC1 / ETC1A4 t3x files into `romfs/hd_textures/`. ~3 minutes on a
modern desktop with 14-thread parallelism. Subsequent runs reuse the
extracted cache and re-encode in <1 minute.

If you skip this step the engine just falls back to the standard PNGs in
`romfs/data/textures/` — game still runs, no HD uplift.

## Controls

| Input | Action |
|---|---|
| Circle Pad | Pitch / yaw |
| A | Fire primary |
| B | Fire secondary |
| X | Move forward |
| Y | Move backward |
| L / R | Strafe left / right |
| ZL *(New 3DS only)* | Turbo (consumes nitro fuel) |
| ZR *(New 3DS only)* | Drop mine |
| D-pad left / right | Cycle primary weapons (cannons) |
| Start | Pause menu (save / load / quit) |
| Select | Rear view *(New 3DS)* / **Turbo (nitro)** *(Old 3DS)* |
| Start + Select | Quit to HOME *(New 3DS only)* |
| 3D slider *(hardware)* | Stereoscopic depth |

Old 3DS doesn't have ZL/ZR — those buttons were added in the New 3DS
hardware refresh (2014). Without ZL there's no way to engage turbo,
which the third SP level (`thermal`) requires to clear a long lava
chase, so on Old 3DS the SELECT button is re-bound to **turbo (nitro)**.
Rear-view and the Start+Select quit chord are dropped on Old 3DS as a
result — the player can quit via HOME instead. Drop-mine has no
binding on OG since ZR also doesn't exist; primary fire, secondaries,
and weapon-switching cover the rest. Platform detected at startup via
`APT_CheckNew3DS`.

In the in-game pause menu, D-pad navigates and A confirms / B backs out.

## Performance

| Scenario | Old 3DS | New 3DS |
|---|---|---|
| Single-eye, no HD textures | 60 fps | 60 fps |
| Single-eye, HD textures | 60 fps | 60 fps |
| Stereoscopic, HD textures | mostly stable 60 fps | mostly stable 60 fps |
| Heavy combat (10+ enemies firing) | brief dips | brief dips |

There's almost no difference between the two platforms in practice. PICA200
even at the Old 3DS clock has more than enough vertex / fragment headroom for
Forsaken's draw counts, and the renderer side is single-pass stereo + ETC1
single-tap fragments → no fillrate or bandwidth wall to hit. The "heavy
combat" dips are particle-overdraw bound, not platform-specific.

Context: Forsaken originally targeted a Pentium 166 with Direct3D 3.0 in
1998. The compute and memory budget the game actually asks for is small;
most of the savings here come from direct citro3d submission (no
high-level-API tax) and moving per-vertex lighting from CPU to the
PICA200 vshader, which had spare ALU on both 3DS generations.

## Memory budgets

3DS hardware partitions a fixed amount of RAM to the running application.
Which partition you get depends on how the build is launched and what
the `.rsf` declares:

| Mode | Total app RAM | Use |
|---|---|---|
| OG `.3dsx` via Homebrew Launcher (applet) | ~64 MB | dev iteration |
| OG CIA, `SystemMode: 96MB` (HIMEM) | 96 MB | shipping target |
| New 3DS CIA, `SystemModeExt: 124MB` | 124 MB | shipping target |

`assets/forsaken.rsf` declares both HIMEM (OG) and SystemModeExt (N3DS),
so installed CIAs get the larger budget on each platform.

The build's heap layout (set in `main_3ds.c`):

```
4 MB code + 18 MB BSS + 32 MB malloc + 32 MB linear + 1 MB stack +
9 MB OS reserve ≈ 96 MB
```

This fits New 3DS comfortably (29 MB headroom) and OG CIA HIMEM tightly
(1 MB margin). It does not fit OG `.3dsx` HBL applet mode — that path is
for development iteration only; **installed CIA is the shipping target**
on OG.

The 32 MB malloc figure was chosen after the autotest harness caught a
heap-exhaustion crash in `LoadCompObj` on Forsaken Remastered's denser
levels (military.nme has 205 enemies, each instantiating a `COMP_OBJ`
tree of ~1-15 KB). The previous 24 MB heap was overrun by ~5 MB on
those levels.

The linear heap holds the 4 MB GPU command buffer, render targets, HD
texture pages, level vertex/index buffers, and audio. **Both Old and
New 3DS load the full 512×512 HD pack** — the OG mip-0 strip (carried
over from the pre-Q3 24 MB linear heap) was retired once the bumped
32 MB linear heap proved to fit OG with comfortable headroom across
the heaviest Remaster levels (validated on military, the historical
worst-case).

If the OG-CIA 1 MB margin proves fragile in production testing, the
right fix is a `__system_allocateHeaps` weak override that splits
per-platform (32+32 on N3DS, 28+24 on OG). Available but not yet
needed.

## Architecture notes

### Renderer (`render_c3d.c`)
- Vertex shader (`shaders/render_c3d.v.pica`) does the MVP transform plus
  per-vertex point-light contribution: ambient + per-light distance
  falloff for up to 8 simultaneous lights, saturated to [0, 1]. Lights
  are uploaded once per camera pass (camera-level filter) and re-uploaded
  per visible level-mesh group with a BSP-visibility filter so lights
  don't bleed through walls. Distance-to-camera ranking decides which 8
  lights win when more than 8 are visible (observed up to 59 in dense
  combat). Replaces the 1998 engine's per-vertex CPU dynamic-light loop
  on the level mesh; static models still use CPU lighting.
- LVERTEX (24 B: `xyz` + packed `COLOR` + `tu/tv`) is converted to a
  shader-friendly `gpu_vertex_t` (36 B) in a 4 MB linear scratch buffer
  (sized for worst-case combat frames with thousands of laser / explosion
  vertices).
- Consecutive draws with matching state and texture are merged before
  submission. Groups are sorted by texture pointer first to maximise
  batching potential.
- Hardware stereo runs both eye callbacks fully each frame, drawing into
  separate left/right render targets. An earlier display-list-replay
  optimisation that recorded the first eye and replayed it for the
  second was removed because it silently dropped any 2D / HUD draw
  issued after the replay trigger (menu text, reticles, missile inset)
  — visual correctness wins over the saved BSP traversal.
- HD texture loader (`try_load_hd_texture`) uses `Tex3DS_TextureImportStdio`
  for ETC1 / ETC1A4 t3x files. Sets colorkey from the texture format
  (only ETC1A4 enables alpha test).
- 4 MB GPU command buffer (16× citro3d default) needed for laser-beam /
  explosion frames that produce thousands of draws.

### Audio (`sound_3ds.c`, `music_3ds.c`)
- SFX: capped at 8 concurrent voices on channels 0-7, oldest-evict round
  robin replacement. Music owns channel 23 separately.
- Music: streams DSP-ADPCM from romfs in 0.5 s chunks, 4-buffer queue
  (~2 s headroom). Background thread refills DONE buffers on a 25 ms tick
  guarded by a `LightLock`. Track size is derived from `ftell` on the file
  rather than the DSP header's `num_adpcm_nibbles` field, because
  `gc-dspadpcm-encode` writes that count including frame-header nibbles
  and our division by 14 over-estimated end-of-file.
- ndspInit's `dspfirm.cdc` requirement is met differently per build:
  the **.3dsx** bundles a copy in romfs and `sound_init()` writes it to
  `sdmc:/3ds/dspfirm.cdc` if absent so emulator testing plays audio
  without a separate setup step. The **CIA** does NOT bundle it; CFW
  users installing on real hardware already have `sdmc:/3ds/dspfirm.cdc`
  from the DSP1 dump step, and leaving the firmware out of the CIA
  avoids redistributing a Nintendo blob. If a CIA user skipped `dsp1`,
  the engine shows a pre-boot console screen with instructions instead
  of booting into silent audio with no explanation.

### HD texture pipeline (`regen_hd_textures_4k.sh`)
- Source: Forsaken Remastered 4K texture pack (`~/Downloads/4ktexturepack.rar`).
  KPF files are zip archives; the script extracts them once into `/tmp/4k_pack/`.
- Per-level textures take their atlas from the 4K pack (verified to match
  the original layout). Globals (UI, sprites, fonts) take their atlas from
  `Data/textures/` originals — Night Dive's 4K versions of those textures
  re-arranged the menu UI atlas, breaking UV math.
- Walls: `tex3ds -f etc1 -m gaussian` (no alpha plane, Gaussian-kernel
  mip chain). Gaussian's wider footprint dilutes isolated outlier pixels
  from the AI upscaler that bilinear mips would amplify into distant-wall
  sparkle.
- Grates / sprites / pickups: `tex3ds -f auto-etc1` (auto picks ETC1A4 if
  alpha is present).
- Alpha detection uses the **original BMP's palette** (presence of pure
  `(0, 0, 0)`) rather than scanning the upscaled image — AI upscaling
  introduces stray pure-black pixels into nearly every wall, which breaks
  count-based heuristics.
- Unified pack for both 3DS generations. OG 3DS rebuilds each texture at
  half dimensions at load time (memcpy'ing mip 1..N into mip 0..N-1 of a
  smaller allocation) to fit its tighter linear-heap budget. PICA200's
  Morton tiling is dimension-dependent per mip level, so the byte
  layouts match with no tiling conversion.

### ARM / engine fixes
- 34+ unaligned-access sites fixed under `#ifdef ARM` (`bsp.c`, `mload.c`,
  `mxaload.c`, `lights.c`, `goal.c`, `node.c`, `water.c`, etc.). PICA200's
  ARM11 cannot dereference misaligned `float*` cast from `char*` after a
  `u_int16_t` field read — the original Forsaken file parsers do this
  constantly.
- `MAX_TEXTURE_GROUPS` reduced from 600 (PC) to 64 with a mid-draw flush
  pattern in `screenpolys.c`, `2dpolys.c`, `polys.c`. Heavy sprite scenes
  used to overflow.
- `InterpFrames()` in `mxaload.c` aligns animated-vertex data once at load
  time rather than memcpy-ing two structs per vertex per frame —
  reclaimed ~30 % framerate on sequences with many animated models.
- Cross-level memory hygiene. The original level / model / animated-model
  loaders allocate per-execbuf vertex copies (`originalVerts[]` in
  `mload.c`, `mxload.c`, `mxaload.c`) but never free them. Without the
  matching releases this accumulates ~3–5 MB per level transition and
  exhausts the heap on the fifth load, manifesting as a hard crash mid-
  campaign. The matching frees are now in each `ReleaseM*loadheader` so
  malloc stays flat at 10–14 MB across the full 15-level 1998 SP set.

## Known limitations

- Missile chase camera has limited draw distance (BSP portal visibility
  edge case).
- No multiplayer / networking on 3DS.

## Credits

- **Original game**: Probe Entertainment / Acclaim (1998)
- **Forsaken 64**: Iguana Entertainment (1998) — origin of the N64-port
  level / enemy / boss meshes that ship in the Remaster build flavour
- **Forsaken Remastered**: Night Dive Studios (2018) — source of the
  Remaster level set, additional music tracks, colorized crate-menu
  banners, and KEX-engine asset format reverse-engineered into the
  KPF extraction pipeline
- **Community source port**: [ForsakenX](https://github.com/ForsakenX/forsaken)
- **3DS port**: Colby Shores
- **picaGL**: [masterfeizz](https://github.com/masterfeizz/picaGL)
- **HD texture source**: Forsaken Remastered 4K texture pack
- **CIA tooling**: 3DSGuy/Project_CTR (`makerom`),
  carstene1ns/3ds-bannertool

## License

GNU General Public License, version 2 — same as upstream ForsakenX.
Full text in [`LICENSE`](LICENSE). Original 1998 Forsaken source is
considered abandonware; portions have been substantially modified or
replaced, and the combined work is distributed under GPLv2.
