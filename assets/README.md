# CIA build assets

Files in this directory feed `make -f Makefile.3ds cia` to produce an installable
`.cia` for the 3DS home menu.

| File | What it is | Tracked? |
|---|---|---|
| `icon.png` | 48×48 home-menu icon (PNG with alpha) | yes |
| `banner.png` | 256×128 home-menu banner image | yes |
| `banner.wav` | banner audio (silent placeholder) | yes |
| `forsaken.rsf` | makerom config (title metadata, perms) | yes |
| `dspfirm.cdc` | Nintendo DSP firmware blob | **NO** (gitignored) |

## First-time setup

1. **Fetch the CIA build tools** (one-shot):
   ```sh
   make -f Makefile.3ds tools-fetch
   ```
   Downloads `makerom` (3DSGuy/Project_CTR) and `bannertool` (carstene1ns/3ds-bannertool)
   into `./tools/`. ~5 MB total.

2. **Place `dspfirm.cdc`** at `assets/dspfirm.cdc`. ndspInit (audio) requires
   this Nintendo firmware blob; it gets bundled into the CIA and copied to
   `sdmc:/3ds/dspfirm.cdc` on first launch by `sound_init()`. Get it by
   running [DSP1 by zoogie](https://github.com/zoogie/DSP1/releases) on your
   own 3DS — it dumps to `/3ds/dspfirm.cdc` on the SD card. Copy that file
   here. **Do not redistribute.**

3. **Build the CIA**:
   ```sh
   make -f Makefile.3ds RENDERER=citro3d cia
   ```
   Output: `forsaken3ds.cia` (~440 MB, includes romfs + HD textures + music).

4. **Install**: open the `.cia` in [FBI](https://github.com/Steveice10/FBI/releases)
   on your 3DS → Install. The icon will appear on the home menu.

## Replacing the icon

Drop a new PNG at `assets/icon.png` (any reasonable size, will be resampled to
48×48 by smdhtool). PNGs with alpha are supported. The current icon is
based on [this SteamGridDB upload](https://www.steamgriddb.com/logo/3010272).

## Notes on the RSF

`assets/forsaken.rsf` is the makerom ROM Spec File. UniqueId `0xff4ba` is in
the developer/test range; leave it as-is for personal builds. If you ever
want to share more broadly (which would also need a non-bundled DSP path),
pick a different UniqueId so installs don't collide.
