# Forsaken animated HOME Menu banner

The 3D banner the 3DS HOME Menu plays when Forsaken is selected:
1. the background holds still;
2. Donna slides in from the left;
3. the heart pops in and spins in place, 2 s per turn;
4. "FORSAKEN / REMASTERED" grows in over the dagger handle.

It was approved on a New 3DS XL on 2026-11-21.

- **`forsaken_banner.cgfx`** is the finished banner. It is identical to
  `../assets/banner.cgfx`, which is the copy the CIA build uses.
- **`SPEC.md`** has the full design, every number and where it came from,
  and the hardware test log. Read §2 before changing anything structural.

## Rebuild it

You need Python ≥ 3.12, git, devkitPro (for the test app), and the repo's
`tools/bannertool` and `tools/makerom`. `make -f Makefile.3ds tools-fetch`
downloads those two.

```
banner/setup_tools.sh                                      # once: pycgfx @ 6c2e0cd + venv in banner/.tools/
banner/.tools/venv/bin/python banner/build_banner.py       # -> banner/build/forsaken_banner.cgfx (+ .bnr)
banner/.tools/venv/bin/python banner/preview_banner.py     # -> banner/build/preview.gif, preview_final.png
banner/.tools/venv/bin/python banner/build_banner.py --install   # also copies it to assets/banner.cgfx
```

The build is deterministic: rebuilding from these sources gives the same
bytes as `forsaken_banner.cgfx`. It fails if a hardware rule is broken
(see "Rules" below).

## See it on a console

The banner can't be viewed on a PC; Mandarine doesn't run the HOME Menu.
The preview is a software approximation that is good for layout and
timing, but not exact.

To check it on a real console, build the hello-world test CIA. It has its
own title (UniqueId 0xff4c0, shown as "Banner Test") and uses
`assets/banner.cgfx`, so it never touches a Forsaken install:

```
banner/test_app/build_cia.sh       # -> banner/test_app/build_cia/banner_test.cia
```

1. Copy the CIA to the SD card. With ftpd-pro running:
   `curl -T banner/test_app/build_cia/banner_test.cia ftp://<console-ip>:5000/cia/`
2. In FBI, **uninstall the old "Banner Test" first**, then install the new
   one.
3. Select "Banner Test" on HOME.

The HOME Menu caches banners, so if you still see the old one, reboot.

## Ship it

`make -f Makefile.3ds release` (or `cia`) packs `assets/banner.cgfx` into
every Forsaken CIA with `bannertool -ci`. It also gives the CIAs an SMDH
with the `extendedbanner` flag set; without that flag the HOME Menu ignores
the 3D banner. If `assets/banner.cgfx` is missing, the build falls back to
the still `assets/banner.png`.

## Change it

Every placement and timing value is a named constant at the top of
`build_banner.py`, in screen fractions and seconds:

| What | Constants |
|---|---|
| Timeline | `T_BG_HOLD`, `T_SLIDE`, `T_SCALE`, `T_TEXT_IN`, `T_TURN`, `N_TURNS` |
| Title placement | `TEXT_TOP_V`, `TEXT_WIDTH_V` |
| Heart | `HEART_CENTER_V`, `HEART_HEIGHT_V` |
| Donna | `DONNA_HEIGHT_V`, `DONNA_TOP_V`, `DONNA_MIRROR_*`, `DONNA_TOP_FEATHER` |

After a change, rebuild, preview, test on the console, and add the result
to SPEC.md §9a.

New art: replace the source file and rebuild.
- **Donna:** her face is completed by mirroring about PNG column 14.
- **Tattoo-free `donna.png`** (for example from Nano Banana): set
  `DONNA_ERASE_TATTOO_IN_MIRROR = False`, then recheck
  `DONNA_MIRROR_AXIS_X` if the face moved.

## Rules learned on hardware (details in SPEC §2 and §9a)

The pinned pycgfx revision is `6c2e0cd`, which is the hardware-tested
`1f78850` plus its MIT licence.

1. **Naming and size.** The model and its animation are both named
   `COMMON`. The CGFX must be under 512 KiB.
2. **Animation.** Use rigid node translate/rotate/scale animation only:
   soft skinning freezes real consoles.
3. **No colour fades.** The HOME Menu ignored material colour animation
   (tests 3–6).
4. **Textures.** RGBA8 doesn't display on the HOME Menu; use RGB565 or
   RGBA4.
5. **Still layers.** The HOME Menu turns the whole model about the
   vertical axis through its centre. A layer that must hold still needs two
   things:
   - its own top-level node with YAxial billboard (5);
   - that node at the origin, with the layer's offset baked into the
     vertices.

   A spinning mesh must sit on that axis (x = 0, z = 0). `../t2k` uses the
   same rule.
6. **Draw order.** Set it with each material's render layer
   (`transluscency_kind`): 0 = background + heart, 1 = Donna, 2 = title.
   With every material on layer 0, the HOME Menu drew the background last,
   over everything.
7. **Fit to the mask.** The HOME Menu shows the banner through an oval
   mask. Keep faces and lettering inside it; the preview's green ellipse is
   a good model of it.
8. **Check the file itself.** When something looks wrong on the console,
   read the shipped bytes with `decode_bones.py` before theorising.
