# Forsaken 3DS — animated HOME Menu banner (SPEC)

Status (2026-11-21): **FINAL, approved on hardware** (New 3DS XL, test 9:
"THAT IS PERFECT!!! I LOVE IT!!"). `forsaken_banner.cgfx` in this folder is
the approved banner, byte-identical to `assets/banner.cgfx`, which
`Makefile.3ds` packs into every CIA. §9a is the hardware test log that got
it there; `README.md` has the step-by-step rebuild directions.

## 1. What was asked for

When the Forsaken title is selected on the 3DS HOME Menu, the top screen plays:

1. **Background first.** `forsaken_background_blank.webp` (stormy sky over
   cracked lava ground) is shown on its own.
2. **Donna slides in from the left.** Once the background is up, `donna.png`
   slides in from off-screen left and stops exactly when the **left edge of
   her portrait reaches the left edge of the screen**. The slide takes
   **~0.5 s**. The PNG's alpha must work, so the background shows through
   the transparent and faded parts (her hair fades out to the right and
   bottom).
3. **The heart scales in.** The 3D heart (`forsaken_heart_v3/forsaken_heart.obj`,
   the dagger + barbed wire + "FORSAKEN" ribbon) grows from an
   infinitesimal point to its full size in **~0.25 s**. It sits between
   Donna and the right side of the screen; placement and size follow the
   mock-up `reference_mockup.webp`.
4. **The heart spins.** At full size it turns 360° in place, **2 s per
   turn** (slowed from 1 s at the owner's request after test 6), and keeps
   turning.
5. **The title comes in** (added 2026-11-20). `forsaken_text.png`
   ("FORSAKEN" with "REMASTERED" underneath, white lettering on
   transparent) appears **right after the heart reaches full size**. A
   fade was asked for, but the HOME Menu would not run a material-colour
   fade (tests 3–6), so it **scales in** from its own centre instead. It
   sits **above the heart, centred on it**, the way the Forsaken
   Remastered cover art (`reference_cover.png`) stacks the title over the
   heart. The owner asked for the lettering to be **big**: it may (and
   should) **cover part of the dagger handle** rather than be shrunk to
   fit the space above the heart. The first two sizes tried (0.46 and 0.72
   screen heights wide) were rejected as too small. The current size,
   about 63 % of the screen width, was approved from the preview.

The end state should look like `reference_mockup.webp` with the title from
`reference_cover.png` above the heart.

## 2. How a 3DS animated banner works (constraints the design follows)

- A CIA's ExeFS `banner` is a **BNR** (= CBMD container + CWAV sound). The
  CBMD holds an LZ11-compressed **CGFX** (a.k.a. BCRES) 3D scene that the
  HOME Menu renders on the top screen with its own camera. `bannertool
  makebanner -ci file.cgfx -a sound.wav` builds the BNR.
- The SMDH needs the **`extendedbanner` flag** (bit 0x20 of the u32 at
  0x2028). Without it the HOME Menu does not treat the banner as a 3D
  scene.
- The CGFX model must be named **`COMMON`**, and its skeletal animation
  **`COMMON`**.
- The uncompressed CGFX must be **under 512 KiB**.
- **Real hardware freezes on soft-skinned animation** (it works in
  emulators). Only **rigid mesh-node TRS animation** is safe: each moving
  mesh rides one node, the animation members are plain Transform (type 5)
  translation/rotation/scale, the primitive sets use skinning mode 0, and
  there are no bone-index or bone-weight streams.
- **Texture format RGBA8 is broken on the HOME Menu.** We use RGB565
  (opaque) and RGBA4 (with alpha). Textures are at most 256×256.
- The HOME Menu plays every animation at once and loops the CGFX
  animation at its frame size. Frames run at 60 per second (pycgfx's
  convention).
- **The HOME Menu turns the whole banner model about the vertical axis
  through the model origin.** Anything not built for that orbits. This was
  learned on T2K's banner and again here (§9a, tests 1 and 2), and is
  recorded in `../t2k/docs/DOCTRINE.md` ("THE BILLBOARD PINS THE TURN; THE
  ORIGIN PINS THE ORBIT. BOTH ARE NEEDED") and
  `../t2k/t2k_3ds/tools/gen_banner_level.py`. The rules:
  1. **Every layer is its own scene-root node.** No shared parent: a
     billboard does not carry over to children (test 1: a billboard on a
     common `card` parent, and every child still orbited).
  2. **A flat layer that must hold still** (background, Donna, title)
     needs *both* things on its own bone:
     - **YAxial billboard (5)**, which replaces the bone's world rotation
       and so cancels the turn of its orientation;
     - **the bone at the origin**, with the layer's offset baked into the
       vertices. A billboard leaves world *translation* alone, so a bone
       away from the origin is swept around a circle. At the origin there
       is nothing to sweep.

     "Origin kills the orbit, billboard kills the tumble. Neither alone
     does."
  3. **A spinning mesh can only spin in place if it sits on the turn axis**
     (x = 0, z = 0). It uses T2K's seat/hold pair: a static seat node on
     the axis carries the height, and the animated hold node under it
     carries the mesh centred on its own origin. T2K's tube seat is at
     (0, 2.46, −0.67). This is why the heart is at screen centre
     (u = 0.5) rather than the mock-up's 0.577.
  4. pycgfx re-sorts bones by translucency after building the hierarchy,
     so the nodes are listed already in that order and the build checks the
     order did not change.
  5. **Read the shipped bytes.** `decode_bones.py` decodes the bones
     straight from the CGFX (name, parent, billboard, position); compare
     with T2K's shipped banner, `../t2k/t2k_3ds/build/banner.cgfx` (build
     it there with `t2k_3ds/tools/build_banner.sh` if it is missing).

Sources: the gbatemp tutorial "How to Make a 3D Banner Like an Official
Nintendo 3DS Game" by Epic0522 (hardware-tested with ClouDS Music FA,
https://github.com/Epic0522/ClouDS-Music-FA), pycgfx
(https://github.com/skyfloogle/pycgfx), and 3dbrew CBMD
(https://3dbrew.org/wiki/CBMD).

## 3. Toolchain

| Piece | What it is |
|---|---|
| `setup_tools.sh` | One-time setup. Clones **pycgfx pinned at `6c2e0cd`** into `.tools/pycgfx` (that commit is the hardware-tested `1f78850` plus its MIT licence; the code is identical). Creates `.tools/venv` with gltflib, pillow and numpy. Needs Python ≥ 3.12. |
| `build_banner.py` | Source art → `build/scene.gltf` → pycgfx `convert_gltf` → post-conversion patches → validation → `build/forsaken_banner.cgfx` → test-packs `build/forsaken_banner.bnr` with `tools/bannertool`. With `--install` it also copies the CGFX to `assets/banner.cgfx`. |
| `preview_banner.py` | Software render of `build/scene.gltf` through the HOME camera at 400×240 on the same timeline, written to `build/preview.gif` and `build/preview_final.png`. It checks composition and timing only; its lighting is a stand-in. |
| `Makefile.3ds` | If `assets/banner.cgfx` exists, the BNR is built from it (`-ci`); otherwise from `assets/banner.png` as before. Every CIA (unified, alt, HIMEM, HIMEM alt) uses `build/3ds/forsaken3ds_cia.smdh`: the smdhtool SMDH with only the extendedbanner bit added (flags 0x141 → 0x161; everything else is byte-identical). The `.3dsx` keeps the plain SMDH. |

Rebuild:

```
banner/setup_tools.sh                                   # once
banner/.tools/venv/bin/python banner/build_banner.py --install
banner/.tools/venv/bin/python banner/preview_banner.py  # optional, ~75 s
make -f Makefile.3ds cia                                # or `release`
```

`.tools/` and `build/` are git-ignored. `assets/banner.cgfx` is the
artifact the CIA build consumes; commit it alongside `assets/banner.wav`.

## 4. Scene

### HOME Menu camera (pycgfx `banner-camera.gltf`)

Perspective, position (0, 1, 44.786), looking down −Z, vertical FOV 30°
(0.523599 rad), aspect 400/240. Near 26.5, far 1000. At depth z the view is
`2·(44.786 − z)·tan 15°` units tall, centred on y = 1.

### Node hierarchy (all rigid, no skinning)

```
COMMON                 CMDL model; pycgfx's "Scene root" bone, the only parent
├── bg                 billboard 5, bone at origin, quad baked at z = -10;
│                      opaque, unlit, never animated
├── heartseat          static seat ON the turn axis: (0, y, 0), no billboard
│   └── heart          heart mesh centred on its origin; animated S, R (T held
│                      at 0); spins in place
├── donna              billboard 5, quad baked at its final place; animated T
│                      (off-screen -> 0, so it rests at the origin)
└── text               billboard 5, bone ON THE AXIS at the lettering's
                       height (0, y, 0), quad centred on it; animated S
                       (scale-in from its own centre)
```

Decoded from the shipped CGFX (`decode_bones.py`), every bone except
`heart` is a child of the scene root (parent 0). Billboard 5 is on bg,
donna and text; bg and text sit at (0, 0, 0); heartseat is at
(0, 1.17, 0). The T2K shipped banner has the same shape: LogoSeat
(billboard 5, origin), plus TubeSeat (on axis) → TubeSpin.

Each mesh's `mesh_node_name` is its node's name. All geometry is baked
into the vertices at its final place; nodes carry only the animated
offsets. The exception is the heart: its vertices are centred on its
bounding-box centre and `heartseat` lifts it to its height on the turn
axis. During Donna's 0.5 s slide her bone is away from the origin, so the
HOME turn can sweep her slightly while she slides. Once she stops, her
bone is at the origin and she is fixed.

### Layers

Screen coordinates are fractions of the **composition frame**: u 0→1
left→right, v 0→1 top→bottom. The frame is the HOME camera's view scaled by
`COMPOSITION_SCALE` about its centre. It is 1.0 now: the picture fills the
HOME Menu's rounded banner mask, at the owner's choice after test 4. Test 3
tried 0.85, which sits inside the mask.

| Layer | Depth z | Placement | Size |
|---|---|---|---|
| bg | −10 | centred, fills the camera view | view × 1.03 (`BG_OVERSCAN`); the HOME Menu's oval mask trims it |
| donna | −5 | original PNG's left edge at u = 0.05 (test-6 placement), PNG top at v = 0.112 | PNG height = 0.85 × screen height (25 % below the original 1.134); a 160 px strip mirrored about PNG column 14 completes the nose/lip line at the old cut edge; 150 px of it fade gradually to transparent, so there is no edge; top 12 % feathered, no left feather; only columns 0–791 of the 1280-wide PNG (everything right of that is fully transparent) |
| heart | 0 (centre) | centre at **u = 0.5** (forced: on the HOME turn axis), v = 0.62 (test 7; was 0.493) | model height (pommel to blood drips) = 0.62 × screen height |
| text | +6 (layer 2, no depth test) | horizontally centred on the heart (u = 0.5); top of the lettering at v = 0.215 (test 8: over the dagger handle; was 0.12, before that 0.025) | lettering width = 1.05 × screen height (≈ 63 % of screen width, ≈ 2.4 × the heart's width); height from the PNG's aspect (alpha bounding box 1178×279, so ≈ 0.25 × screen height). Its lower part (the "REMASTERED" line) covers the dagger's handle and pommel. |

Layering uses **render layers plus depth** (§9a tests 4–6). Each
material's translucency kind (CGFX render layer 0–3, drawn in that order)
is set explicitly:
- **Layer 0:** the background (z −10) and the heart, opaque, with depth
  test + write, so their mutual order doesn't matter.
- **Layer 1:** Donna (z −5), blended, depth test on, no write. She lies
  over the background, and the heart's near half covers her faded hair.
- **Layer 2:** the title (z +6), blended, no depth test, so it is always on
  top of the dagger.

With every material in layer 0 (pycgfx's default), the HOME Menu drew the
background last and covered everything that doesn't write depth. The 3D
slider separates the layers: title nearest, sky deepest.

### How the mock-up was mapped (design decisions)

- **Aspect.** The mock-up is 800×560 (1.43:1) and the top screen is
  400×240 (1.67:1). The screen shows the mock-up's full height. The
  background (1024×717, the same 1.43 aspect) is **stretched 17 %
  horizontally** to fill the width; that is not noticeable on clouds and
  cracked ground, and nothing is cropped.
- **Donna's scale and position** are taken from the mock-up's features:
  eye at (65, 105) and lips at y ≈ 300 of 800×560, against eye (150, 226)
  and lips y ≈ 540 in the 1280×1024 PNG. That gives scale 0.62 and a top
  offset of −0.0625 H. Her final left edge is the screen edge, as
  requested ("stopping right at the far left edge of her portrait").
- **Heart placement.** Its bounding box in the mock-up is roughly
  x 340–605, y 103–450, so centre (472, 276) and height 0.62 H. The
  extra screen width (1.67 vs 1.43) is split evenly on both sides, which
  gives u = 0.577. **Hardware overruled it:** a spinning mesh off the HOME
  turn axis orbits (test 2), so the heart is at u = 0.5. It still sits
  between Donna's opaque edge and the right edge. The title is centred on
  it.
- **Title placement.** On the cover the title is centred over the heart
  with a gap above the pommel. Our screen has less room above the heart
  (its pommel is at v ≈ 0.18), so, at the owner's direction, the title is
  sized for legibility and overlaps the handle instead of shrinking into
  the gap. Note the mock-up heart already carries a "FORSAKEN" ribbon.
  The title adds the logotype and "REMASTERED".
- **The heart model's resting pose** is its front view: ribbon reading
  "FORSAKEN", dagger pommel up-left, as in the mock-up.

## 5. Timeline

All curves are written at 60 frames per second.

| t (s) | frame | Event |
|---|---|---|
| 0.00 | 0 | Background only. Donna is parked off-screen left; the heart is at scale 0.001. |
| 0.20–0.70 | 12–42 | Donna slides from fully off-screen (x offset −(her width + 2 % screen)) to her final position, **ease-out cubic** (fast in, settles to a stop). Keys every 1/30 s, linear between them. |
| 0.70–0.95 | 42–57 | Heart scales 0.001 → 1.0, ease-out cubic. |
| 0.95–1.45 | 57–87 | Title scales in, 0.001 → 1, ease-out cubic over `T_TEXT_IN` = 0.5 s, from the lettering's own centre. It starts the moment the heart reaches full size, together with the spin. Before frame 57 the alpha curve clamps at 0. |
| 0.95 → | 57 → | Heart rotates about its own vertical (Y) axis at 180°/s (`T_TURN` = 2 s per turn), positive rot_y (counter-clockwise seen from above). |
| 600.95 | 36057 | The animation loops (the intro replays); 300 turns × 2 s. |

- **The 0.20 s background hold** is our choice. The request said
  "after it is loaded", with no length given. Change it with `T_BG_HOLD`.
- **Infinitesimal start.** Scale 0.001, not 0, so the node matrix never
  becomes singular. At that scale the heart is sub-pixel.
- **Endless spin without replaying the intro every turn.** A looping
  animation would replay Donna's slide on every loop. Instead, the spin
  is one **linear rot_y curve with two keys**: frame 57 → 0 rad and frame
  36057 → 2π·300 rad. The CANM frame size is 36057 (300 turns of 2 s, 10
  minutes), so the intro replays only after 10 minutes on the same
  title. Curves clamp before their first key and after their last, so the
  heart's rotation is 0 until frame 57. The euler value is unbounded, so
  interpolation never wraps. The glTF holds a placeholder rotation
  channel; `patch()` replaces it after conversion, because glTF
  quaternion keys can't express 600 turns in two keys. Change `N_TURNS`
  to move the loop point.
- The HOME Menu restarts the banner animation when the title is
  selected, so every selection plays the full intro.

## 6. Textures and budget

| Texture | Source | Size | Format | Notes |
|---|---|---|---|---|
| bg | webp 1024×717 | 256×256 | RGB565 | stretched to fill the quad; UVs cover the whole image |
| donna | PNG cols 0–791 × 1024 rows | 256×256 | RGBA4 | soft 16-level alpha; RGB under weak alpha is filled with nearby colour (`bleed_colour`) so bilinear filtering gives no dark fringe |
| heart | 2048×1024 atlas | 256×128 | RGB565 | |
| text | PNG 1440×324, cropped to the alpha bounding box (137–1315 × 45–324) | 256×64 | RGBA4 | colour bled under weak alpha, like Donna |

All three are **Floyd–Steinberg dithered** to the bits the format stores
(5/6/5, or 4/4/4/4), which hides the 4-bit banding on Donna's face. The
prepared PNGs are saved as `build/tex_*.png`.

Budget of the current build: **488,856 bytes = 93 % of 512 KiB**
(451,512 before the title). The heart
is 2,613 vertices and 3,716 triangles with float position, normal and UV.
The mesh is not decimated (the tutorial warns that decimation caused
cracks). BNR (LZ11 + CWAV): 443,296 bytes.

About 23 KiB remains, so there is little room left. Switching the heart
atlas to 256×256 would add 64 KiB and exceed the limit. A larger title
texture won't fit either (256 is the maximum width anyway). Encoding Donna as ETC1A4 instead of RGBA4
would halve her texture and remove the banding, but pycgfx has no ETC1
encoder.

## 7. Materials (post-conversion patches in `patch()`)

- **bg, donna — unlit.** Fragment lighting is off. TEV stage 0 replaces
  RGB and alpha with texture 0 (source 3, combine Replace); stages 1–5
  pass the previous result through (source 0xF). So they show exactly
  their image colours. Donna is glTF `alphaMode: BLEND`: SrcAlpha /
  InvSrcAlpha, depth test on, depth write off, drawn last (pycgfx sorts
  translucent meshes last), so the heart in front occludes her and she
  blends over the background.
- **text — unlit, like Donna.** Fragment lighting is off. TEV stage 0
  replaces RGB and alpha with texture 0; later stages pass through.
  Render layer 2, blended, no depth test, so it is always drawn on top.
  It has **no material animation**:
  - **Tests 3–6:** the title's alpha was multiplied by an animated
    material colour, first TEV constant 0, then the material diffuse
    through pycgfx's `KHR_animation_pointer` path. The title never
    showed.
  - **Two explanations are known:** pycgfx writes every TEV stage's
    constant-colour bytes as zero, and the HOME Menu does not apply
    material colour animation here.
  - **So** the title enters by a node scale animation, the kind proven
    on hardware.
- **heart — lit, matte.** pycgfx's diffuse fragment lighting (texture ×
  vertex colour, × diffuse light) is kept. The added-specular TEV stage 2
  is set to pass-through, because pycgfx's PBR specular washes models
  out white under the HOME light.

## 8. Validation built into `build_banner.py`

The build fails if any of these does not hold: models dict ==
`["COMMON"]`; skeletal animation dict == `["COMMON"]`; every primitive set
has skinning mode 0; no BoneIndex or BoneWeight vertex streams; every
skeletal animation member is Transform (5) and targets exactly donna,
heart and text; there is no material animation; the bone order is the glTF
order; billboard 5 is on exactly bg, donna and text; bg and donna bones are
at the origin; heartseat and text bones are on the turn axis; CGFX ≤ 512 KiB.

## 9. Verification

Done (software):
- pycgfx serialised the CGFX. A byte-level read-back confirmed the spin
  segment: start 57, end 36057, linear, StepLinear64 keys (57, 0) and
  (36057, 3769.911). It also confirmed CANM looping = 1, frame size 36057,
  and exactly one `COMMON` model. It also found both CANMs (Skeletal and
  Material) looping at 36057 frames, and the title's alpha keys
  (57, 0) → (87, 1).
- bannertool accepted the CGFX: the CBMD's LZ11 stream decompresses to
  451,512 bytes.
- `make -f Makefile.3ds` built the BNR and the CIA SMDH (flags 0x161, one
  byte different from the plain SMDH).
- makerom accepted the BNR and SMDH in a CIA built in a scratch directory
  with the existing ELF.
- `preview_banner.py` renders the expected sequence and the final frame
  matches the mock-up's layout.
- SPICA was tried as an independent CGFX reader. It could not parse even
  stock pycgfx output, so it says nothing about this file.

**Banner test CIA (`banner/test_app/`).** A hello-world libctru app with
its own title: UniqueId **0xff4c0**, product code CTR-P-FBNT, HOME name
"Banner Test". It carries the same `assets/banner.cgfx` + `banner.wav`, and
its SMDH (bannertool) has the extendedbanner flag. It lets the banner be
tested and reinstalled without touching the Forsaken install. Its
RSF is `forsaken.rsf` with only the title, product code and UniqueId
changed. Build it with `banner/test_app/build_cia.sh`, which writes
`banner/test_app/build_cia/banner_test.cia` (~558 KB). Install it with FBI,
select "Banner Test" on HOME, and run the checklist below. The app itself
only prints "Hello, world!"; START exits. Uninstall it from FBI
afterwards. 0xff4c0 is outside the Forsaken release set (0xff4ba / bb /
be / bf); don't reuse it for a release.

### 9a. Hardware test log

- **Test 1 (2026-11-20, New 3DS, banner_test.cia).** The heart's look was
  right, but **the background, Donna and the heart all orbited** instead
  of holding still and turning in place. The background also read as a
  cut-off oval instead of filling the banner.
  - Cause: the YAxial billboard on `card`. It was the only thing in the
    scene able to swing the layers. The bone order and the animation
    targets were checked and were correct (`heart`, `donna`).
  - Fix: no billboard anywhere, the heart moved to a heartplace/heart
    place/hold pair (T2K's proven shape), background overscan 1.03 → 1.10.
    The build now fails if any bone has a billboard, if the bone order
    differs from the glTF node order, or if the skeletal animation
    targets anything but `heart` and `donna`.
- **Test 2 (2026-11-20).** No billboards and a heartplace/heart pair at
  u = 0.577. **Everything still orbited, the heart included.** That
  disproved the test-1 reading: the HOME Menu itself turns the model.
  - The owner pointed to T2K. Its shipped banner (`../t2k/t2k_3ds/build/banner.cgfx`) and
    DOCTRINE give the rule in §2: root-level billboard-5 layers with bones
    at the origin, and the spinning mesh on the axis.
  - The Obsidian MCP server was down (502), so the vault could not be
    checked. T2K's DOCTRINE.md and generator comments held the banked
    findings.
- **Test 3 (2026-11-20).** The §2 structure. "Much much better": the
  background and heart are right (still, and turning in place). Three
  faults:
  1. **Donna never appeared.** Her billboard bone's rest translation was
     off-screen (the slide's start). The billboard layer evidently stayed at
     its rest translation, so the slide animation never moved it on screen.
     Fix: Donna's rest pose is the origin like the other billboard layers;
     only the animation holds the off-screen start. If the HOME Menu does
     not animate billboard bones at all, she now at least shows in place.
  2. **The title never appeared.** Its TEV stage multiplied texture alpha
     by TEV constant 0. pycgfx writes every stage's constant-colour bytes
     as zero (the `xxxx` in `TextureCombiner.struct`), and the material's
     Constant0 is not copied into the stage, so alpha = 0. Fix: alpha =
     texture × primary (vertex) colour, with the fade on the material
     **Diffuse**, pycgfx's own supported colour animation. Base diffuse
     alpha is 1, so if the fade is ignored the title just shows.
  3. **Oval edges.** The HOME Menu draws the banner through a rounded
     (oval-cornered) mask, and the full-bleed picture went past it. Fix:
     `COMPOSITION_SCALE = 0.85`. Every layout number is now in a
     composition frame that is the camera view shrunk to 85 % about its
     centre (the heart stays on the axis). The background exactly fills
     that frame (`BG_OVERSCAN` 1.0), and Donna's PNG is cropped to the
     frame's rows so nothing hangs outside it. 0.85 is an estimate; a photo
     of the top screen would let it be fitted exactly.
  - Donna's slide now starts with her right edge past the *camera's* left
    edge, so she slides across the margin into the frame.
- **Test 4 (2026-11-20).** Donna was visible only at the edges as she
  slid, **behind the background**, and the title never showed. The
  background had depth baked at z −10, Donna −5 and the title +6, all
  billboards. Depth baked into billboard vertices does not order the
  layers on the HOME Menu: the opaque background, which writes depth,
  covered them.
  - The owner also **changed the oval decision**: the picture should fill
    the whole oval.
  - Fix:
    - all flat layers are on the **z = 0 plane**;
    - layering is by **draw order and depth state** (mesh order: bg →
      heart → donna → text);
    - **bg**: no depth test, no depth write (it can never cover anything);
    - **donna**: blended, depth test on, no write (sits over the
      background, under the heart's near half);
    - **text**: blended, no depth test (always on top, over the dagger);
    - **heart**: normal depth test and write;
    - `COMPOSITION_SCALE` back to 1.0, with `BG_OVERSCAN` 1.03.

    `preview_banner.py` applies the same depth rules.
  - Stereo: the flat layers now share the screen plane; only the heart
    has depth.
- **Test 5 (2026-11-20). Only the background showed.** With its depth
  test off, it covered the heart, Donna and the title, so the HOME Menu
  draws the background **after** them, not first. Reading tests 4 and 5
  together:
  - Baked depth works. The heart, which writes depth, always survived
    the background drawn after it.
  - The real fault is **draw order**. Every material was in translucency
    layer 0 (pycgfx's default), so the HOME Menu sorted them as opaque and
    drew the background last, over the layers that write no depth.
  - The test-4 reading ("baked depth untrustworthy") was **wrong**.
  - Fix: explicit render layers through the MTOB translucency kind
    (pycgfx field `transluscency_kind`):
    - 0 = background (z −10) and heart, both depth test + write;
    - 1 = Donna (z −5), blended, depth test, no write;
    - 2 = title (z +6), blended, no depth test.

    The depths are restored, so stereo depth is back.
- **Test 6 (2026-11-20). "Now we are looking good."** The layering is
  right: background behind, Donna sliding over it, heart spinning. Still
  open: **the title never appears.** Owner requests:
  - the heart turns at **half speed** (2 s per turn);
  - Donna **25 % smaller so her eyes are visible.** They sat in the
    corner the HOME oval mask cuts; owner: "we may need to undo it".

  Changes:
  - **Title:** no material animation at all. TEV like Donna (texture
    alpha). The entrance is a **scale-in** of its node, which sits on the
    turn axis at the lettering's height. Its rest pose is full size, so it
    shows even if the animation is ignored. The build now fails if any
    material animation is present.
  - **Spin:** `T_TURN` = 2.0, `N_TURNS` = 300 (still a 10-minute loop).
  - **Donna:**
    - `DONNA_HEIGHT_V` 1.134 → 0.85.
    - Her PNG's top and left edges are hard cuts. Shrinking her with the
      top-left pinned would have pushed her eyes *further* into the masked
      corner.
    - So she sits lower and slightly right (`DONNA_TOP_V` 0.112,
      `DONNA_LEFT_U` 0.05). Her top 12 % and left 10 % are feathered into
      the background, putting her eye at about (0.13, 0.30), inside an
      inscribed-ellipse model of the mask.
    - Undo: `DONNA_HEIGHT_V` 1.134, `DONNA_TOP_V` −0.0625,
      `DONNA_TOP_FEATHER` 0, `DONNA_LEFT_U` 0, `DONNA_LEFT_FEATHER` 0.
- **Test 7 (2026-11-20).** "The text looks great!" The scale-in works and
  so does the slower spin. **Smaller Donna was undone** ("half of her face
  is cut off"): back to `DONNA_HEIGHT_V` 1.134 and `DONNA_TOP_V` −0.0625,
  with both feathers 0 and `DONNA_LEFT_U` 0. The feather code stays for
  later tries.
  - Owner: lower the title and the heart so both fit, and let the title
    cover the dagger handle. Measured against an inscribed-ellipse model
    of the HOME oval mask (the green outline drawn on the preview):
    - `TEXT_TOP_V` 0.025 → **0.12**: the outer letters' top corners
      (u ≈ 0.19 / 0.81) need v ≥ 0.11 to clear the oval.
    - `HEART_CENTER_V` 0.493 → **0.62**: the heart's top (v ≈ 0.31) tucks
      under the title's lower edge (≈ 0.37), so the title covers the
      handle, and the drips end at v ≈ 0.93, inside the oval's bottom.
- **Owner, after test 7:** keep Donna at the 25 % size ("I like how
  donna looks at the 25%"), but complete her face instead of fading or
  cutting it: "match with the side you do have since both sides of a face
  are symmetrical". The owner may remove the tattoo from `donna.png` with
  Nano Banana.
  - `donna_source()` mirrors the PNG about her face's midline,
    `DONNA_MIRROR_AXIS_X` = 14. That is the column where the nose tip and
    lip centre run off the PNG's left edge; the join was checked up close
    and is seamless.
  - It extends the canvas 300 px to the left, and the outer 80 px of
    mirrored hair fade out.
  - The tattoo is on her visible cheek, so its mirrored copy is erased
    (`DONNA_ERASE_TATTOO_IN_MIRROR`): an ellipse, PNG (170, 405) with radii
    82 × 120, is filled with a harmonic (diffusion) fill plus slight grain.
    Set it False once a tattoo-free `donna.png` replaces the current one.
  - Placement: the 25 % size again (`DONNA_HEIGHT_V` 0.85, `DONNA_TOP_V`
    0.112, top feather 12 %). The canvas's left edge is at u = −0.01, which
    puts both eyes inside the oval.
- **Owner, on the preview:** "too much of her face, we only need the
  missing sliver". The 300 px mirror showed the whole other half of her
  face.
  - Now `DONNA_MIRROR_EXTEND` = 80 (fade 50): just enough to complete the
    nose and lip line at the old cut edge.
  - Donna is back at the exact test-6 placement: the original PNG's left
    edge at u = 0.05 (`DONNA_LEFT_U` is derived from that), height 0.85,
    top 0.112, top feather 12 %.
  - **No left feather.** That feather is what faded half her face in
    test 6.
- **Owner, on the preview:** the 80 px sliver left "a little sliver here
  that needs correcting". Its 50 px fade is only ~15 texels, so the strip
  ended in a visible straight vertical edge down her face. Now
  `DONNA_MIRROR_EXTEND` = 160 with `DONNA_MIRROR_FADE` = 150: the fade
  spans almost the whole strip, so the face melts into the dark
  background with no line. The original PNG's left edge stays at
  u = 0.05.
- **Test 8 (2026-11-20).** "Donna and the spinning heart look great!
  They are positioned perfectly!" The owner wants the title over the
  dagger: "place the text directly over the handle of the dagger. Not the
  heart itself", literally overlaying the dagger.
  - Measured on the heart at rest (preview rows of 240): pommel ≈ 75,
    crossguard ≈ 111, top of the heart ≈ 113. The lettering is about 60
    rows tall, so `TEXT_TOP_V` 0.12 → **0.215** puts it on rows 52–112:
    it covers the whole handle and stops just above the heart.
  - A literal "3/4 inch down" reading (≈ 72 rows on the New 3DS XL's
    4.88" screen) was built first. In the preview it reached the heart's
    ribbon, so it was replaced before upload.
- **Test 9 (pending):** check that the title sits over the dagger handle
  with the heart body clear, and is fully in view.
- (Earlier test-8 checklist, still valid:) check that Donna's full face is inside the oval
  and the mirror join and skin fill look natural, the title and heart sit
  inside the oval, and the title covers the handle.

Not done. Mandarine has no HOME Menu in its NAND, so the banner has never
run in a HOME Menu. Hardware checklist:
1. `make -f Makefile.3ds cia`, install with FBI. The HOME Menu caches
   banners: if the old banner still shows, reboot or uninstall and
   reinstall.
2. Select Forsaken. Check the background, then Donna's slide (~0.5 s), then
   the heart's pop (~0.25 s), then the spin at one turn per 2 s.
3. Check Donna's soft alpha against the background: no dark fringe, no
   box around her.
4. Check the heart's lighting is not washed out or black.
5. Check the title scales in after the heart pops, covers the dagger
   handle, and has no box around the lettering.
6. Leave the cursor on the title for a while, move away and back, and
   launch and exit the game. Check there is no freeze.
7. Move the 3D slider and check the layer separation.
8. Test on both an Old and a New 3DS if possible.

## 10. Known risks and fallbacks

| Risk | Fallback |
|---|---|
| The HOME Menu might cap the CANM frame size or behave oddly with a 36,057-frame animation. | Lower `N_TURNS` (for example 60 → a 61 s loop). The intro then replays once per loop. |
| Unlit TEV setup (lighting flag 0) rendered wrong. | Keep pycgfx's lit setup and set the diffuse material so the light term ≈ 1. |
| Donna's RGBA4 banding is visible despite dithering. | Write an ETC1A4 encoder (8 bpp, better colour), or move her to 128×256 RGBA4 plus a separate RGB565 colour plane. |
| Material (colour) animation **did not work** on the HOME Menu (tests 3–6). | Resolved: only node TRS animation is used. A true fade would need the TEV constant written into the stage command bytes (pycgfx leaves them zero) and is untested; don't retry without a hardware test. |
| The HOME UI covers part of the banner. | Adjust `HEART_*` / `DONNA_*`. Every layout number is a named constant at the top of `build_banner.py`. |

## 11. Files in `banner/`

| File | Role |
|---|---|
| `README.md` | step-by-step directions: rebuild, preview, test on hardware, ship |
| `SPEC.md` | this document |
| `forsaken_banner.cgfx` | **the approved banner** (= `assets/banner.cgfx`) |
| `forsaken_banner.bnr` | the same banner packed by bannertool with `assets/banner.wav` (what goes into a CIA's ExeFS) |
| `forsaken_background_blank.webp` | source art: background |
| `donna.png` | source art: Donna (RGBA) |
| `forsaken_text.png` | source art: "FORSAKEN / REMASTERED" lettering |
| `forsaken_heart_v3/forsaken_heart.obj`, `.mtl`, `forsaken_heart_tex.png` | source art: the 3D heart (mesh + texture) |
| `reference_mockup.webp` | design target: the composition mock-up |
| `reference_cover.png` | design target: the Remastered cover (title over heart) |
| `setup_tools.sh` | one-time: pycgfx (pinned) + venv into `.tools/` |
| `build_banner.py` | the pipeline: sources → glTF → CGFX → patches → validation → `build/` (`--install` copies to `assets/`) |
| `preview_banner.py` | software preview through the HOME camera → `build/preview*.png/.gif` |
| `decode_bones.py` | decodes skeleton bones straight from CGFX bytes |
| `test_app/` | hello-world test CIA (UniqueId 0xff4c0, "Banner Test"): `source/main.c`, the devkitPro template `Makefile`, `banner_test.rsf`, `build_cia.sh` |

Generated and git-ignored, recreated by the scripts: `.tools/`, `build/`,
and `test_app/build*`. The heart was delivered with extras the build never
used: a `.glb`, a views sheet, an HTML viewer and the original `.zip`.
They were moved out during the 2026-11-21 clean-up.
