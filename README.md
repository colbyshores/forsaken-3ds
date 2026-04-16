# Forsaken 3DS

A native Nintendo 3DS port of [ForsakenX](https://github.com/ForsakenX/forsaken), the community source port of Acclaim's 1998 six-degrees-of-freedom space combat game *Forsaken*.

This fork provides a complete, playable 3DS homebrew build with a custom GPU renderer, hardware stereoscopic 3D, and full single-player campaign support.

## Features

- **Native citro3d renderer** — bypasses picaGL for direct PICA200 GPU access with draw call batching, texture sorting, and per-frame state caching
- **Hardware stereoscopic 3D** — single-pass display list replay driven by the 3D slider, with anaglyph fallback for emulators
- **CPU per-vertex lighting** — full point light and spot light support ported from the GL1 path
- **60 FPS** at native 400x240 resolution on New 3DS (804 MHz), with dips to 30+ during heavy particle effects
- **In-game pause menu** — Start button toggles save/load/quit menu with D-pad + A/B navigation
- **Save/Load** — 16 save slots on SD card (`sdmc:/3ds/forsaken/savegame/`)
- **ARM alignment fixes** — 34+ unaligned access sites fixed for ARM11 (ARMv6K)
- **Optimized animated models** — load-time vertex alignment eliminates per-frame memcpy overhead

## Requirements

### Build Tools
- [devkitPro](https://devkitpro.org/) with devkitARM (ARM cross-compiler)
- [libctru](https://github.com/devkitPro/libctru) (3DS standard library)
- [citro3d](https://github.com/devkitPro/citro3d) (GPU library)
- libpng, zlib, lua5.1 (install via `(dkp-)pacman -S 3ds-libpng 3ds-zlib 3ds-lua`)
- [picaGL](https://github.com/masterfeizz/picaGL) (only for the `GL=1` renderer path)

### Game Data
You need a legitimate copy of Forsaken. Place the game data directories (`Data/`, `Configs/`, `Pilots/`, `Demos/`, `Scripts/`) inside a `romfs/` directory at the project root. The build process embeds this into the `.3dsx` file.

```
forsaken-3ds/
  romfs/
    data/         <- Data/ from Forsaken install
    configs/      <- Configs/
    pilots/       <- Pilots/
    scripts/      <- scripts/ (Lua files from this repo)
```

File and directory names inside `romfs/` must be **lowercase** — the Makefile handles this automatically during the staging step.

### Running
- **Real hardware**: Copy `forsaken3ds.3dsx` to your SD card and launch via a homebrew launcher (Luma3DS + Rosalina recommended). **New 3DS strongly recommended** for performance.
- **Emulator**: [Mandarine](https://github.com/mandarine3ds/mandarine) or Citra. For audio, place `dspfirm.cdc` in the emulator's `sysdata/` directory and enable `LLE\DSP=true` and `audio_emulation=1` in the config.

## Building

```bash
# Set devkitPro environment (if not already in your shell profile)
export DEVKITPRO=/opt/devkitpro
export DEVKITARM=$DEVKITPRO/devkitARM

# Native citro3d renderer (recommended)
make -f Makefile.3ds RENDERER=citro3d

# picaGL (OpenGL 1.x shim) renderer
make -f Makefile.3ds

# Debug build (autoboot to Volcano level, trace logging)
make -f Makefile.3ds RENDERER=citro3d DEBUG=1

# Clean
make -f Makefile.3ds clean

# Clean romfs staging (re-copies and lowercases all game data)
make -f Makefile.3ds romfs-clean
```

Output: `forsaken3ds.3dsx` (ready to run on 3DS or emulator).

## Branches

| Branch | Description |
|--------|-------------|
| `master` | Stable, merged — all features from both port branches |
| `3ds-citro3d` | Native citro3d renderer development (recommended) |
| `3ds-port` | Earlier picaGL-based port (functional, less optimized) |

## Controls

| Button | Action |
|--------|--------|
| Circle Pad | Move / strafe |
| C-Stick | Look / aim |
| A | Fire primary |
| B | Fire secondary |
| X | Next primary weapon |
| Y | Previous primary weapon |
| L / R | Strafe left / right |
| D-pad | Cycle secondary weapons |
| Start | Pause menu (save/load/quit) |
| Select | Rear view |
| ZL | Nitro |
| ZR | Drop mine |

## Architecture

The citro3d renderer (`render_c3d.c`) replaces the entire OpenGL rendering pipeline:

- **Vertex shader** (`shaders/render_c3d.v.pica`) — MVP transform with separate projection and modelView uniforms
- **GPU_RGBA4 textures** — 16-bit with Morton/Z-order tiling and colourkey transparency (black = alpha 0)
- **Draw call batching** — consecutive texture groups with matching state merged into single `C3D_DrawArrays` calls
- **Texture sorting** — groups sorted by texture pointer before batching to maximize coherence
- **Display list replay** — stereo second eye replays recorded draw calls with updated view matrix (no second BSP traversal)
- **1MB scratch buffer** — vertex conversion from LVERTEX (20 bytes) to gpu_vertex_t (36 bytes, float colors)
- **1MB GPU command buffer** — 4x default, handles heavy particle/laser beam frames

## Known Limitations

- Missile chase camera has limited draw distance (BSP portal visibility issue under investigation)
- No bottom screen usage (map/HUD could be offloaded there in future)
- No multiplayer/networking on 3DS
- Audio requires DSP firmware dump (`dspfirm.cdc`) on emulators

## Credits

- **Original game**: Probe Entertainment / Acclaim (1998)
- **Community source port**: [ForsakenX](https://github.com/ForsakenX/forsaken)
- **3DS port**: Colby Shores
- **picaGL**: [masterfeizz](https://github.com/masterfeizz/picaGL)

## License

The original source code is considered Abandonware. See [LICENSE](LICENSE) for details.
