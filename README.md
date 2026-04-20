# Forsaken 3DS

A native Nintendo 3DS port of [ForsakenX](https://github.com/ForsakenX/forsaken),
the community source port of Acclaim's 1998 six-degrees-of-freedom space combat
game *Forsaken*.

A complete, playable build with a custom GPU renderer, hardware stereoscopic
3D, the full single-player campaign, and HD-source textures sized to fit
PICA200's budget. Holds a mostly stable 60 fps in stereoscopic 3D at 512×512
texture resolution on **both Old and New 3DS** — the original game's draw
counts are tiny by modern standards, and the close-to-metal renderer plus
single-pass stereo leave plenty of headroom on either platform.

## Highlights

- **Single-pass stereo via display list replay.** The first eye records GPU
  draw commands; the second eye replays them with an updated view matrix —
  no second BSP traversal, no second vertex transform pass. That headroom is
  what lets HD textures and stereo coexist at 60 fps.
- **HD textures (ETC1 / ETC1A4) with selective mipmaps.** Per-level walls
  upscaled from a 4K source pack, downscaled to 512×512 ETC1 with bilinear
  mipmaps. Sprites and grates use ETC1A4 with an alpha-from-original-palette
  detector so see-through pixels survive. Mipmaps are skipped on
  POLYANIM-animated lava/fire/water surfaces (alpha+mip = halo holes).
- **Threaded DSP-ADPCM music.** Tracks are streamed from romfs in 0.5 s
  chunks on a background thread (25 ms tick), decoded by the DSP hardware
  for zero CPU cost. Per-buffer ADPCM context tracking eliminates
  buffer-boundary glitches.
- **Concurrent-voice cap on the SFX pool** (8 voices, oldest-evict). PICA200's
  DSP firmware mixes channels in software and starts skipping audio — and
  burning ARM cycles — when too many voices are active. The cap recovered
  the music channel under heavy combat AND took back ~15 fps.
- **CPU per-vertex lighting**, ported from the GL1 path. Single-pass stereo
  leaves enough CPU headroom to do this without a framerate hit; in exchange
  we avoid PICA's hardware 8-light cap.

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
./regen_hd_old_4k.sh
```

The script extracts the pack (one-time, cached at `/tmp/4k_pack/`), then
generates ETC1 / ETC1A4 t3x files into `romfs/hd_old/`. ~3 minutes on a
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
| ZR | Drop mine |
| D-pad left / right | Cycle primary weapons (cannons) |
| Start | Pause menu (save / load / quit) |
| Select | Rear view |
| Start + Select | Quit to HOME |
| 3D slider | Stereoscopic depth (hardware) |

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
most of the savings here come from removing the high-level-API tax (citro3d
direct submission) and not paying for stereo twice (display list replay).

## Architecture notes

### Renderer (`render_c3d.c`)
- Vertex shader (`shaders/render_c3d.v.pica`) does MVP transform with
  separate projection / modelView uniforms.
- LVERTEX (24 B: `xyz` + packed `COLOR` + `tu/tv`) is converted to a
  shader-friendly `gpu_vertex_t` (36 B) in a 1 MB linear scratch buffer.
- Consecutive draws with matching state and texture are merged before
  submission. Groups are sorted by texture pointer first to maximise
  batching potential.
- Stereo replay records `s_dl[2048]` of `dl_entry_t` (scratch offset, vertex
  count, matrices, viewport, texture, blend state). The second eye triggers
  a single `replay_display_list()` and skips all per-object draw calls.
- HD texture loader (`try_load_hd_texture`) uses `Tex3DS_TextureImportStdio`
  for ETC1 / ETC1A4 t3x files. Sets colorkey from the texture format
  (only ETC1A4 enables alpha test).
- 1 MB GPU command buffer (4× default) needed for laser-beam / explosion
  frames that produce thousands of draws.

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
  avoids redistributing a Nintendo blob.

### HD texture pipeline (`regen_hd_old_4k.sh`)
- Source: Forsaken Remastered 4K texture pack (`~/Downloads/4ktexturepack.rar`).
  KPF files are zip archives; the script extracts them once into `/tmp/4k_pack/`.
- Per-level textures take their atlas from the 4K pack (verified to match
  the original layout). Globals (UI, sprites, fonts) take their atlas from
  `Data/textures/` originals — Night Dive's 4K versions of those textures
  re-arranged the menu UI atlas, breaking UV math.
- Walls: `tex3ds -f etc1 -m bilinear` (no alpha plane, mipmaps).
- Grates / sprites / pickups: `tex3ds -f auto-etc1` (auto picks ETC1A4 if
  alpha is present, no mipmaps).
- Alpha detection uses the **original BMP's palette** (presence of pure
  `(0, 0, 0)`) rather than scanning the upscaled image — AI upscaling
  introduces stray pure-black pixels into nearly every wall, which breaks
  count-based heuristics.
- Animated POLYANIM textures (`therma`, `nuke*`, `lava*`, etc.) are
  hard-blacklisted from mipmaps regardless — alpha and animated UVs both
  produce visible block-edge artifacts at distance.

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

## Known limitations

- Missile chase camera has limited draw distance (BSP portal visibility
  issue, under investigation).
- No multiplayer / networking on 3DS.

## Credits

- **Original game**: Probe Entertainment / Acclaim (1998)
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
