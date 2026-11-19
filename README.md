# Forsaken 3DS

A native Nintendo 3DS port of *Forsaken* (Acclaim / Probe, 1998), the
six-degrees-of-freedom tunnel shooter, built on the
[ForsakenX](https://github.com/ForsakenX/forsaken) community source port.

It runs at a mostly steady 60 fps, in stereoscopic 3D, on both Old and New 3DS.

## Features

- **Native citro3d renderer** written for the 3DS GPU, with no OpenGL layer
  in between.
- **Hardware stereoscopic 3D**, with depth set by the 3D slider.
- **HD textures** from the 4K community pack, compressed to ETC1 with
  Gaussian mipmaps: 512×512 on New 3DS (and Old 3DS with HIMEM), 256×256 on
  a standard Old 3DS install.
- **GPU lighting**: dynamic lights computed per vertex in the vertex shader,
  plus optional per-pixel shine on enemies and pickups and procedural wall
  detail. Both can be toggled under Pause → Options → Visuals.
- **Streamed DSP-ADPCM music** (the original CD soundtrack), decoded by the
  3DS's DSP hardware.
- **Two editions:** the 1998 game, or the Forsaken Remastered content (more
  levels including the N64 ones, new music tracks, N64 enemies and bosses)
  from a Steam copy you own.
- **One CIA for every 3DS**: it detects the console's memory at start-up and
  picks the matching heap and texture set.
- **Remappable controls** (Pause → 3DS Controls), saved on the SD card.

The technical details — renderer, memory, data pipelines, engine changes —
are in [docs/PORT_DETAILS.md](docs/PORT_DETAILS.md).

## Installing

Install `forsaken3ds.cia` with FBI on a 3DS running custom firmware. The game
needs the DSP firmware dump at `sdmc:/3ds/dspfirm.cdc` for sound; if you
haven't made it, run [DSP1](https://github.com/zoogie/DSP1) once, otherwise
the game shows a reminder at start-up.

| Default control | Action |
|---|---|
| Circle Pad | Pitch / yaw |
| A / B | Fire primary / secondary |
| X / Y | Forward / back |
| L / R | Strafe left / right |
| D-pad left / right | Previous / next primary weapon |
| D-pad up / down | Slide vertically |
| ZL / ZR *(New 3DS)* | Turbo / drop mine |
| Select | Rear view *(New 3DS)*, turbo *(Old 3DS)* |
| Start | Pause menu |

## Building

Requirements: [devkitPro](https://devkitpro.org/wiki/Getting_Started) with
the 3DS toolchain (`3ds-dev`), `citro3d`, `tex3ds`, `3ds-libpng`, `3ds-zlib`
and `3ds-liblua51`; Python 3; `p7zip`, `bchunk` and `ffmpeg` for asset
extraction.

**1. Game data** (not included — use a copy you own). Either:

```bash
# the 1998 game, from the disc image (BIN/CUE also gives the CD music)
python3 tools/assets/extract_assets.py "Forsaken (USA).bin"

# or Forsaken Remastered, from the Steam install
python3 tools/assets/extract_remaster_levels.py \
    ~/.steam/steam/steamapps/common/Forsaken\ Remastered/ForsakenEX.kpf
```

**2. HD textures** (optional; without them the game uses the original
textures). Download the
[4K texture pack](https://www.moddb.com/mods/upscaled-forsaken-textures/addons/4k-texture-pack)
to `~/Downloads/4ktexturepack.rar`, then:

```bash
tools/texture/regen_hd_textures_4k.sh
```

**3. Build.**

```bash
make -f Makefile.3ds                      # 1998 edition
make -f Makefile.3ds EDITION=remaster     # Remastered edition
make -f Makefile.3ds tools-fetch          # once, before the first CIA build
make -f Makefile.3ds EDITION=remaster cia # installable CIA
make -f Makefile.3ds release              # all public artifacts, from clean
```

Everything is written to `build/3ds/` (`forsaken3ds.3dsx`, `.cia`, `.elf`,
...). The `.3dsx` runs from the Homebrew Launcher on a New 3DS; on an Old 3DS
the launcher doesn't give it enough memory, so install the CIA. Packaging
details are in
[assets/README.md](assets/README.md).

The repository also builds a PC version (`make`, SDL2 + Vulkan, output
`build/pc/projectx`, run from the repository root) used for development and
comparison.

## Credits

- **Original game:** Probe Entertainment / Acclaim (1998)
- **Forsaken 64:** Iguana Entertainment (1998) — the N64 levels, enemies and
  bosses in the Remastered edition
- **Forsaken Remastered:** Night Dive Studios (2018)
- **Community source port:** [ForsakenX](https://github.com/ForsakenX/forsaken)
- **3DS port:** Colby Shores
- **HD texture source:** the Upscaled Forsaken Textures 4K pack
- **CIA tooling:** 3DSGuy/Project_CTR (`makerom`), carstene1ns/3ds-bannertool

## License

GNU General Public License, version 2, the same as upstream ForsakenX. See
[`LICENSE`](LICENSE).
