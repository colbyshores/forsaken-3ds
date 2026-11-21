#!/usr/bin/env python3
"""Build the animated 3DS HOME-menu banner for Forsaken (see SPEC.md).

    banner/setup_tools.sh                 # once: pycgfx + venv under .tools/
    banner/.tools/venv/bin/python banner/build_banner.py [--install]

Pipeline: source art -> scene glTF (build/scene.gltf) -> pycgfx CGFX ->
post-conversion patches -> validation -> build/forsaken_banner.cgfx, then
bannertool -> build/forsaken_banner.bnr (a check that bannertool accepts it).
--install copies the CGFX to assets/banner.cgfx, which Makefile.3ds packs
into every CIA (the CIA SMDH gets the extendedbanner flag there).

Every number that shapes the banner is a named constant in the first block;
SPEC.md explains where each one comes from.
"""

import base64
import json
import math
import os
import shutil
import struct
import subprocess
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(HERE, ".tools", "pycgfx"))

import gltflib  # noqa: E402
import main as pycgfx  # noqa: E402  (pycgfx's main.py: convert_gltf / write)
from cgfx import swizzler  # noqa: E402
from cgfx.canm import (  # noqa: E402
    FloatAnimationCurve,
    FloatSegment,
    InterpolationType,
    QuantizationType,
    StepLinear64Key,
)
from cgfx.mtob import DepthFlag, MTOBFlag  # noqa: E402
from cgfx.sobj import BillboardMode  # noqa: E402
from cgfx.txob import TextureFormat  # noqa: E402

# ─── sources ──────────────────────────────────────────────────────────────
SRC_BG = os.path.join(HERE, "forsaken_background_blank.webp")
SRC_DONNA = os.path.join(HERE, "donna.png")
SRC_HEART_OBJ = os.path.join(HERE, "forsaken_heart_v3", "forsaken_heart.obj")
SRC_HEART_TEX = os.path.join(HERE, "forsaken_heart_v3", "forsaken_heart_tex.png")
SRC_TEXT = os.path.join(HERE, "forsaken_text.png")
SRC_AUDIO = os.path.join(REPO, "assets", "banner.wav")
INSTALL_CGFX = os.path.join(REPO, "assets", "banner.cgfx")
BANNERTOOL = os.path.join(REPO, "tools", "bannertool")
OUT = os.path.join(HERE, "build")

# ─── HOME-menu camera (pycgfx banner-camera.gltf) ─────────────────────────
CAM_Y = 1.0
CAM_Z = 44.786
CAM_YFOV = 0.523599  # 30 degrees
CAM_ASPECT = 400.0 / 240.0

# ─── composition (screen fractions: u 0=left..1=right, v 0=top..1=bottom) ─
# Layer depths (world z; camera looks down -z from z=44.786).
# Depth is baked into the billboard vertices and works (the heart has
# always drawn correctly in front of the background). What hardware tests 4
# and 5 showed is DRAW ORDER: with every material in translucency layer 0
# the HOME menu drew the background LAST, over Donna and the title (which
# write no depth). patch() therefore puts each material in an explicit
# render layer: 0 = bg + heart (opaque, depth), 1 = Donna (blended over
# them), 2 = title (last, no depth test, always on top).
Z_BG = -10.0
Z_DONNA = -5.0
Z_HEART = 0.0
Z_TEXT = 6.0
# Fraction of the camera view the composition fills. The HOME menu shows the
# banner through a rounded (oval) mask; the owner wants the picture to fill
# that whole oval (test 4 decision), so 1.0 plus a little overscan. Test 3
# used 0.85 to sit inside the mask.
COMPOSITION_SCALE = 1.0
BG_OVERSCAN = 1.03  # no gap can show at the edge of the view
# Donna: left edge of the PNG on the screen's left edge; height and top
# offset matched to the mock-up (eye-to-lips distance), see SPEC.md.
# Test 6 (owner): 25 % smaller so her eyes clear the HOME menu's oval mask.
# Her PNG's top and left edges are hard cuts, so smaller-and-pinned-to-the-
# corner would push the eyes INTO the masked corner; instead she sits lower
# (eye at v ~0.30, inside the oval) and her top edge is feathered into the
# background (DONNA_TOP_FEATHER). Undo: DONNA_HEIGHT_V 1.134, DONNA_TOP_V
# -0.0625, DONNA_TOP_FEATHER 0, DONNA_LEFT_U 0, DONNA_LEFT_FEATHER 0.
# Test 7: the owner liked the 25 % size but not "half of her face cut off",
# and asked for the missing half to be filled in by symmetry. donna_source()
# mirrors the PNG about her face's vertical midline (DONNA_MIRROR_AXIS_X,
# where the nose tip and lip centre run off the PNG's left edge) and extends
# the canvas left by DONNA_MIRROR_EXTEND px. The tattoo is on her visible
# cheek, so the mirrored copy of that area is filled with smooth skin
# (DONNA_ERASE_TATTOO_IN_MIRROR) -- a stand-in until the owner supplies a
# tattoo-free donna.png (then set it False).
DONNA_MIRROR_AXIS_X = 14  # PNG column of the face midline
# Owner (test 8): "too much of her face, we only need the missing sliver" --
# 300 px showed the whole other half; 80 px only completes the nose / lip
# line at the old cut edge, at the test-6 placement.
# Test 8 preview (owner): the 80 px sliver's 50 px fade ended in a visible
# straight vertical edge down her face -- the fade was only ~15 texels.
# A slightly wider sliver whose fade spans almost all of it lets the face
# melt into the dark background with no line.
DONNA_MIRROR_EXTEND = 160  # canvas columns added on the left (mirrored)
DONNA_MIRROR_FADE = 150  # outermost mirrored columns faded to transparent
DONNA_ERASE_TATTOO_IN_MIRROR = True
DONNA_TATTOO_ELLIPSE = (170, 405, 82, 120)  # PNG cx, cy, rx, ry around the tattoo
DONNA_HEIGHT_V = 1.134 * 0.75  # PNG height as a fraction of screen height
DONNA_TOP_V = 0.112  # PNG top edge
DONNA_TOP_FEATHER = 0.12  # fraction of the PNG's height faded in at the top
DONNA_LEFT_U = 0.05 - DONNA_MIRROR_EXTEND * DONNA_HEIGHT_V * 240 / 1024 / 400
# ^ keeps the ORIGINAL PNG's left edge at u = 0.05, the test-6 placement
DONNA_LEFT_FEATHER = 0.0  # extra fade at the left (the mirror fade covers it)
DONNA_CROP_X = DONNA_MIRROR_EXTEND + 792  # canvas columns right of this are fully transparent
# Heart: centre and height from the mock-up.
# u MUST be 0.5 (x = 0) and z 0: the HOME menu turns the whole model about
# the vertical axis through the origin, and a spinning (non-billboard) mesh
# only "rotates in place" if it sits on that axis -- T2K's tube seat is at
# x = 0, z = 0 for the same reason. The mock-up's 0.577 orbited on hardware.
HEART_CENTER_U = 0.5
HEART_CENTER_V = 0.62  # test 7: lowered so heart and title both fit the oval
HEART_HEIGHT_V = 0.62
# Title "FORSAKEN / REMASTERED": centred over the heart like the cover art.
# Large enough that its lower edge covers the dagger handle (owner's call).
# Test 9 (owner): the title goes "directly over the handle of the dagger.
# Not the heart itself" -- literally overlaying the dagger. Measured on the
# heart at rest (preview rows of 240): pommel ~75, crossguard ~111, top of
# the heart ~113. The lettering (~60 rows tall) therefore spans rows 52-112:
# it covers the whole handle and stops just above the heart.
TEXT_TOP_V = 0.215  # top of the lettering
TEXT_WIDTH_V = 1.05  # lettering width as a fraction of screen height (~63% of screen width, ~2.4x heart width)

# ─── timeline (seconds) ───────────────────────────────────────────────────
T_BG_HOLD = 0.20  # background alone before Donna starts
T_SLIDE = 0.50  # Donna slide-in
T_SCALE = 0.25  # heart scale-up
T_TEXT_IN = 0.50  # title scale-in, starts when the heart reaches full size
T_TURN = 2.00  # one 360-degree heart rotation (owner, test 6: half the old speed)
N_TURNS = 300  # turns before the whole animation loops (10 minutes)
FPS = 60  # pycgfx writes 60 frames per glTF second
SCALE_START = 0.001  # "infinitesimal" heart scale (0 would be degenerate)
KEY_STEP = 1.0 / 30.0  # sampling step for eased curves

# ─── texture formats (RGBA8 is broken on the HOME menu) ───────────────────
TEX = {
    # name: (source size after prep, hw format, (r,g,b,a) bits for dither)
    "bg": ((256, 256), TextureFormat.RGB565, (5, 6, 5, 8)),
    "donna": ((256, 256), TextureFormat.RGBA4, (4, 4, 4, 4)),
    "heart": ((256, 128), TextureFormat.RGB565, (5, 6, 5, 8)),
    "text": ((256, 64), TextureFormat.RGBA4, (4, 4, 4, 4)),
}
BILLBOARD_BONES = ("bg", "donna", "text")
CGFX_LIMIT = 0x80000  # 512 KiB, HOME-menu limit


def ease_out_cubic(x):
    return 1 - (1 - x) ** 3


def camera_half_extents(z):
    """Half width/height of the HOME camera's full view at world depth z."""
    hh = (CAM_Z - z) * math.tan(CAM_YFOV / 2)
    return hh * CAM_ASPECT, hh


def view_half_extents(z):
    """Half width/height of the COMPOSITION frame at depth z: the camera
    view shrunk by COMPOSITION_SCALE about its centre, so the whole picture
    sits inside the HOME menu's rounded banner mask (hardware test 3)."""
    hw, hh = camera_half_extents(z)
    return hw * COMPOSITION_SCALE, hh * COMPOSITION_SCALE


def screen_to_world(u, v, z):
    hw, hh = view_half_extents(z)
    return (u - 0.5) * 2 * hw, CAM_Y + (0.5 - v) * 2 * hh


# ─── image preparation ───────────────────────────────────────────────────
def dither(img, bits):
    """Floyd-Steinberg error diffusion to the per-channel bit depths the
    hardware format stores, so the 4-bit (RGBA4) and 5/6-bit (RGB565)
    textures band less."""
    a = np.asarray(img.convert("RGBA"), dtype=np.float32).copy()
    h, w, _ = a.shape
    levels = np.array([(1 << b) - 1 for b in bits], dtype=np.float32)
    for y in range(h):
        row = a[y]
        for x in range(w):
            old = row[x].copy()
            new = np.round(old / 255 * levels) / levels * 255
            row[x] = new
            err = old - new
            if x + 1 < w:
                row[x + 1] += err * 7 / 16
            if y + 1 < h:
                nxt = a[y + 1]
                if x > 0:
                    nxt[x - 1] += err * 3 / 16
                nxt[x] += err * 5 / 16
                if x + 1 < w:
                    nxt[x + 1] += err * 1 / 16
    return Image.fromarray(np.clip(np.round(a), 0, 255).astype(np.uint8), "RGBA")


def bleed_colour(img):
    """Fill RGB under low-alpha pixels with nearby visible colour so
    bilinear filtering of the soft alpha edge does not pull in black."""
    a = np.asarray(img, dtype=np.float32)
    alpha = a[..., 3:4] / 255
    pm = a[..., :3] * alpha
    acc_c, acc_a = pm.copy(), alpha.copy()
    for r in (1, 2, 4, 8, 16, 32):
        for dy, dx in ((r, 0), (-r, 0), (0, r), (0, -r)):
            acc_c += np.roll(pm, (dy, dx), (0, 1))
            acc_a += np.roll(alpha, (dy, dx), (0, 1))
    fill = acc_c / np.maximum(acc_a, 1e-6)
    out = a.copy()
    weak = alpha[..., 0] < 0.5
    out[..., :3][weak] = fill[weak]
    return Image.fromarray(np.clip(out, 0, 255).astype(np.uint8), "RGBA")


def donna_source():
    """donna.png completed by mirroring its face about the midline (see the
    DONNA_MIRROR_* constants). Returns an RGBA image DONNA_MIRROR_EXTEND
    px wider than the PNG, the original occupying the right part."""
    src = np.asarray(Image.open(SRC_DONNA).convert("RGBA"), np.float32)
    h, w, _ = src.shape
    E, M = DONNA_MIRROR_EXTEND, DONNA_MIRROR_AXIS_X
    fill = src.copy()
    if DONNA_ERASE_TATTOO_IN_MIRROR:
        cx, cy, rx, ry = DONNA_TATTOO_ELLIPSE
        y0, y1, x0, x1 = cy - ry - 10, cy + ry + 10, cx - rx - 10, cx + rx + 10
        reg = fill[y0:y1, x0:x1, :3].copy()
        yy, xx = np.mgrid[y0:y1, x0:x1]
        mask = (((xx - cx) / rx) ** 2 + ((yy - cy) / ry) ** 2) <= 1.0
        reg[mask] = reg[~mask].mean(0)
        for _ in range(1500):  # harmonic (diffusion) fill from the rim
            avg = (np.roll(reg, 1, 0) + np.roll(reg, -1, 0)
                   + np.roll(reg, 1, 1) + np.roll(reg, -1, 1)) / 4
            reg[mask] = avg[mask]
        rng = np.random.default_rng(1)  # a little grain so it reads as skin
        reg[mask] += rng.normal(0, 4.0, (mask.sum(), 1))
        fill[y0:y1, x0:x1, :3] = reg
    out = np.zeros((h, w + E, 4), np.float32)
    out[:, E:] = src
    for xn in range(E + M):  # canvas columns left of the midline
        sx = 2 * M - (xn - E)
        if 0 <= sx < w:
            out[:, xn] = fill[:, sx]
    ramp = np.clip(np.arange(w + E) / DONNA_MIRROR_FADE, 0, 1)
    out[..., 3] *= (ramp * ramp * (3 - 2 * ramp))[None, :]
    return Image.fromarray(np.clip(np.round(out), 0, 255).astype(np.uint8), "RGBA")


def donna_rows():
    """PNG rows of Donna that fall inside the composition frame (v 0..1):
    the rest would hang into the margin around the frame."""
    f0 = (0.0 - DONNA_TOP_V) / DONNA_HEIGHT_V
    f1 = (1.0 - DONNA_TOP_V) / DONNA_HEIGHT_V
    return max(0, round(f0 * 1024)), min(1024, round(f1 * 1024))


def prepare_images():
    imgs = {}
    bg = Image.open(SRC_BG).convert("RGBA")
    imgs["bg"] = bg.resize(TEX["bg"][0], Image.LANCZOS)  # stretched to 5:3
    r0, r1 = donna_rows()
    donna = donna_source().crop((0, r0, DONNA_CROP_X, r1))
    if DONNA_TOP_FEATHER > 0:
        a = np.asarray(donna, dtype=np.float32).copy()
        n = int(DONNA_TOP_FEATHER * 1024) - r0
        if n > 0:
            ramp = np.clip((np.arange(a.shape[0]) + r0) / (DONNA_TOP_FEATHER * 1024), 0, 1)
            a[..., 3] *= (ramp * ramp * (3 - 2 * ramp))[:, None]  # smoothstep
        donna = Image.fromarray(a.astype(np.uint8), "RGBA")
    if DONNA_LEFT_FEATHER > 0:
        a = np.asarray(donna, dtype=np.float32).copy()
        ramp = np.clip(np.arange(a.shape[1]) / (DONNA_LEFT_FEATHER * 1280), 0, 1)
        a[..., 3] *= (ramp * ramp * (3 - 2 * ramp))[None, :]
        donna = Image.fromarray(a.astype(np.uint8), "RGBA")
    donna = bleed_colour(donna.resize(TEX["donna"][0], Image.LANCZOS))
    imgs["donna"] = donna
    imgs["heart"] = Image.open(SRC_HEART_TEX).convert("RGBA").resize(TEX["heart"][0], Image.LANCZOS)
    text = Image.open(SRC_TEXT).convert("RGBA")
    text = text.crop(text.getchannel("A").getbbox())  # tight to the lettering
    imgs["text"] = bleed_colour(text.resize(TEX["text"][0], Image.LANCZOS))
    for k in imgs:
        imgs[k] = dither(imgs[k], TEX[k][2])
        imgs[k].save(os.path.join(OUT, f"tex_{k}.png"))
    return imgs


# ─── geometry ─────────────────────────────────────────────────────────────
def load_obj(path):
    vs, vts, vns, faces = [], [], [], []
    for line in open(path):
        p = line.split()
        if not p:
            continue
        if p[0] == "v":
            vs.append(tuple(map(float, p[1:4])))
        elif p[0] == "vt":
            vts.append(tuple(map(float, p[1:3])))
        elif p[0] == "vn":
            vns.append(tuple(map(float, p[1:4])))
        elif p[0] == "f":
            faces.append([tuple(int(i) - 1 for i in c.split("/")) for c in p[1:]])
    remap, pos, nrm, uv, idx = {}, [], [], [], []
    for f in faces:
        corners = []
        for c in f:
            if c not in remap:
                remap[c] = len(pos)
                pos.append(vs[c[0]])
                uv.append((vts[c[1]][0], 1.0 - vts[c[1]][1]))  # glTF v is top-down
                nrm.append(vns[c[2]])
            corners.append(remap[c])
        for i in range(1, len(corners) - 1):
            idx += [corners[0], corners[i], corners[i + 1]]
    return (np.array(pos, np.float32), np.array(nrm, np.float32),
            np.array(uv, np.float32), np.array(idx, np.uint16))


def quad(x0, y0, x1, y1, z):
    """Front-facing (+z) quad; uv (0,0) = top-left of the image."""
    pos = np.array([[x0, y1, z], [x1, y1, z], [x1, y0, z], [x0, y0, z]], np.float32)
    nrm = np.tile(np.array([0, 0, 1], np.float32), (4, 1))
    uv = np.array([[0, 0], [1, 0], [1, 1], [0, 1]], np.float32)
    idx = np.array([0, 3, 2, 0, 2, 1], np.uint16)
    return pos, nrm, uv, idx


def layout():
    L = {}
    hw, hh = view_half_extents(Z_BG)
    L["bg"] = quad(-hw * BG_OVERSCAN, CAM_Y - hh * BG_OVERSCAN,
                   hw * BG_OVERSCAN, CAM_Y + hh * BG_OVERSCAN, Z_BG)
    hw, hh = view_half_extents(Z_DONNA)
    h = DONNA_HEIGHT_V * 2 * hh  # the whole PNG's height on screen
    w = h * (DONNA_CROP_X / 1024)
    r0, r1 = donna_rows()
    x0, top = screen_to_world(DONNA_LEFT_U, DONNA_TOP_V + DONNA_HEIGHT_V * r0 / 1024, Z_DONNA)
    L["donna"] = quad(x0, top - h * (r1 - r0) / 1024, x0 + w, top, Z_DONNA)
    # Start with her right edge just past the camera's (not the frame's) left
    # edge, so she enters from off-screen and crosses the margin.
    chw, _ = camera_half_extents(Z_DONNA)
    L["donna_offscreen_dx"] = (-chw - 0.02 * 2 * chw) - (x0 + w)
    pos, nrm, uv, idx = load_obj(SRC_HEART_OBJ)
    pos -= (pos.min(0) + pos.max(0)) / 2  # pivot = bounding-box centre
    _, hh = view_half_extents(Z_HEART)
    scale = HEART_HEIGHT_V * 2 * hh / (pos[:, 1].max() - pos[:, 1].min())
    L["heart"] = (pos * scale, nrm, uv, idx)
    L["heart_center"] = (*screen_to_world(HEART_CENTER_U, HEART_CENTER_V, Z_HEART), Z_HEART)
    _, hh = view_half_extents(Z_TEXT)
    w = TEXT_WIDTH_V * 2 * hh
    x0, y0, x1, y1 = Image.open(SRC_TEXT).getchannel("A").getbbox()
    h = w * (y1 - y0) / (x1 - x0)
    cx, top = screen_to_world(HEART_CENTER_U, TEXT_TOP_V, Z_TEXT)
    L["text"] = quad(cx - w / 2, top - h, cx + w / 2, top, Z_TEXT)
    # The title's node sits ON THE TURN AXIS at the lettering's height
    # (x = 0 because it is centred on the heart, z = 0), so its scale-in
    # grows from the lettering's own centre without anything to orbit.
    L["text_center_y"] = top - h / 2
    return L


# ─── timeline ─────────────────────────────────────────────────────────────
T_SLIDE_START = T_BG_HOLD
T_SLIDE_END = T_SLIDE_START + T_SLIDE
T_SCALE_END = T_SLIDE_END + T_SCALE
T_ROT_START = T_SCALE_END
T_TEXT_START = T_SCALE_END
T_TEXT_END = T_TEXT_START + T_TEXT_IN
T_TOTAL = T_ROT_START + N_TURNS * T_TURN


def eased_keys(t0, t1, v0, v1, ease):
    ts = [0.0, t0]
    vs = [v0, v0]
    n = max(2, round((t1 - t0) / KEY_STEP))
    for i in range(1, n + 1):
        x = i / n
        ts.append(t0 + (t1 - t0) * x)
        vs.append(v0 + (v1 - v0) * ease(x))
    return ts, vs


# ─── glTF scene ───────────────────────────────────────────────────────────
class Bin:
    def __init__(self):
        self.data = bytearray()
        self.views, self.accessors = [], []

    def add(self, arr, comp, typ, target=None, minmax=False):
        while len(self.data) % 4:
            self.data.append(0)
        off = len(self.data)
        raw = np.ascontiguousarray(arr).tobytes()
        self.data += raw
        view = {"buffer": 0, "byteOffset": off, "byteLength": len(raw)}
        if target:
            view["target"] = target
        self.views.append(view)
        acc = {"bufferView": len(self.views) - 1, "componentType": comp,
               "count": int(arr.shape[0]), "type": typ}
        if minmax:
            acc["min"] = [float(x) for x in arr.min(0)]
            acc["max"] = [float(x) for x in arr.max(0)]
        self.accessors.append(acc)
        return len(self.accessors) - 1


def build_gltf(L, imgs):
    b = Bin()
    images, textures, materials, meshes = [], [], [], []

    def material(name, img_key, alpha_mode, roughness=1.0):
        path = os.path.join(OUT, f"tex_{img_key}.png")
        images.append({"name": img_key, "uri": os.path.basename(path)})
        textures.append({"source": len(images) - 1, "sampler": 0})
        m = {"name": name, "alphaMode": alpha_mode,
             "pbrMetallicRoughness": {"baseColorTexture": {"index": len(textures) - 1},
                                      "metallicFactor": 0.0, "roughnessFactor": roughness}}
        materials.append(m)
        return len(materials) - 1

    def mesh(name, geo, mat):
        pos, nrm, uv, idx = geo
        prim = {"attributes": {"POSITION": b.add(pos, 5126, "VEC3", 34962, True),
                               "NORMAL": b.add(nrm, 5126, "VEC3", 34962),
                               "TEXCOORD_0": b.add(uv, 5126, "VEC2", 34962)},
                "indices": b.add(idx, 5123, "SCALAR", 34963), "material": mat}
        meshes.append({"name": name, "primitives": [prim]})
        return len(meshes) - 1

    m_bg = mesh("bg", L["bg"], material("bg_mat", "bg", "OPAQUE"))
    m_heart = mesh("heart", L["heart"], material("heart_mat", "heart", "OPAQUE"))
    m_donna = mesh("donna", L["donna"], material("donna_mat", "donna", "BLEND"))
    text_mat = material("text_mat", "text", "BLEND")
    tpos, tnrm, tuv, tidx = L["text"]
    m_text = mesh("text", (tpos - np.array([0.0, L["text_center_y"], 0.0], np.float32),
                           tnrm, tuv, tidx), text_mat)

    hc = list(L["heart_center"])
    # The T2K structure (../t2k t2k_3ds/tools/gen_banner_level.py and
    # docs/DOCTRINE.md, learned on hardware): EVERY node is a scene root --
    # no shared parent to inherit the HOME menu's turn from.
    #  - Flat layers that must hold still (bg, donna, text): bone AT THE
    #    ORIGIN with the offset baked into the vertices (nothing to sweep),
    #    plus a YAxial billboard set in patch() (cancels the turn of its
    #    orientation). "Origin kills the orbit, billboard kills the tumble.
    #    Neither alone does."
    #  - The spinning heart: a static seat ON the turn axis (x = 0, z = 0)
    #    and the animated hold node under it, mesh centred on its origin.
    # Listed already in pycgfx's translucency sort order so the re-sort is
    # a no-op (validate() checks).
    assert abs(hc[0]) < 1e-6 and abs(hc[2]) < 1e-6, "heart must sit on the turn axis"
    nodes = [
        {"name": "bg", "mesh": m_bg},
        {"name": "heartseat", "translation": hc, "children": [2]},
        {"name": "heart", "mesh": m_heart, "scale": [SCALE_START] * 3},
        # Rest pose at the ORIGIN (hardware test 3: with the rest pose
        # off-screen she never appeared -- the billboard bone kept its rest
        # translation). The slide is only in the animation.
        {"name": "donna", "mesh": m_donna},
        # Rest pose full size: if the HOME menu ignored the scale-in the
        # title would still show.
        {"name": "text", "mesh": m_text, "translation": [0.0, L["text_center_y"], 0.0]},
    ]
    scene_roots = [0, 1, 3, 4]

    channels, samplers = [], []

    def channel(node, path, ts, vals, comps, pointer=None):
        inp = b.add(np.array(ts, np.float32).reshape(-1, 1), 5126, "SCALAR", minmax=True)
        out = b.add(np.array(vals, np.float32).reshape(-1, comps), 5126,
                    {3: "VEC3", 4: "VEC4"}[comps])
        samplers.append({"input": inp, "output": out, "interpolation": "LINEAR"})
        target = {"node": node, "path": path}
        if pointer:
            target = {"path": "pointer", "extensions": {"KHR_animation_pointer": {"pointer": pointer}}}
        channels.append({"sampler": len(samplers) - 1, "target": target})

    ts, xs = eased_keys(T_SLIDE_START, T_SLIDE_END, L["donna_offscreen_dx"], 0.0, ease_out_cubic)
    channel(3, "translation", ts, [[x, 0, 0] for x in xs], 3)
    ts, ss = eased_keys(T_SLIDE_END, T_SCALE_END, SCALE_START, 1.0, ease_out_cubic)
    channel(2, "scale", ts, [[s] * 3 for s in ss], 3)
    # Keep the hold node at its parent's origin explicitly (no inherited
    # offset to swing around).
    channel(2, "translation", [0.0, T_SCALE_END], [[0, 0, 0]] * 2, 3)
    # Placeholder; the spin is rewritten as one linear rot_y curve after
    # conversion (glTF quaternions cannot express N full turns in 2 keys).
    channel(2, "rotation", [0.0, T_SCALE_END], [[0, 0, 0, 1]] * 2, 4)
    # Title scale-in (node transform animation, the kind proven on the HOME
    # menu). Tests 3-6 tried a material-colour fade (TEV constant, then
    # diffuse) and the title never showed: the HOME menu does not apply
    # material colour animation here, so the title uses none.
    ts, ss = eased_keys(T_TEXT_START, T_TEXT_END, SCALE_START, 1.0, ease_out_cubic)
    channel(4, "scale", ts, [[s] * 3 for s in ss], 3)

    with open(os.path.join(OUT, "scene.bin"), "wb") as f:
        f.write(b.data)
    doc = {
        "asset": {"version": "2.0", "generator": "forsaken banner/build_banner.py"},
        "scene": 0, "scenes": [{"nodes": scene_roots}], "nodes": nodes, "meshes": meshes,
        "materials": materials, "textures": textures, "images": images,
        "samplers": [{"magFilter": 9729, "minFilter": 9729, "wrapS": 33071, "wrapT": 33071}],
        "buffers": [{"uri": "scene.bin", "byteLength": len(b.data)}],
        "bufferViews": b.views, "accessors": b.accessors,
        "animations": [{"name": "intro", "channels": channels, "samplers": samplers}],
    }
    path = os.path.join(OUT, "scene.gltf")
    with open(path, "w") as f:
        json.dump(doc, f, indent=1)
    return path


# ─── conversion + patches ─────────────────────────────────────────────────
def install_texture_hook():
    """pycgfx encodes every texture as RGBA4 and caps it at 256; encode ours
    with the per-texture format instead (images are already sized)."""
    def get_texture(cgfx, gltf, image_id, normal=False):
        image = gltf.model.images[image_id]
        name = image.name
        if name in cgfx.data.textures:
            return cgfx.data.textures[name]
        im = Image.open(os.path.join(OUT, image.uri))
        txob = swizzler.to_txob(im.transpose(Image.Transpose.FLIP_TOP_BOTTOM), TEX[name][1])
        txob.name = name
        cgfx.data.textures.add(name, txob)
        return txob
    pycgfx.gltf_get_texture = get_texture


def linear_curve(t0, v0, t1, v1):
    c = FloatAnimationCurve()
    seg = FloatSegment()
    c.segments.append(seg)
    c.start_frame = seg.start_frame = t0 * FPS
    c.end_frame = seg.end_frame = t1 * FPS
    seg.interpolation = InterpolationType.Linear
    seg.quantization = QuantizationType.StepLinear64
    seg.keys = [StepLinear64Key(t0 * FPS, v0), StepLinear64Key(t1 * FPS, v1)]
    return c


def items(coll):
    """Elements of a pycgfx List (by position) or DictInfo (by name)."""
    if hasattr(coll, "data") and hasattr(coll.data, "contents"):
        return list(coll.data.contents)
    return [coll[n] for n in coll]


def patch(cgfx):
    cmdl = cgfx.data.models["COMMON"]
    bones = cmdl.skeleton.bones
    # Flat layers: YAxial billboard on their origin bones (see build_gltf).
    for name in BILLBOARD_BONES:
        bones[name].billboard_mode = BillboardMode.YAxial
    # Rigid meshes name the node they ride on (ClouDS Music FA convention).
    for m in items(cmdl.meshes):
        m.mesh_node_name = m.name.split("_")[0]  # pycgfx names it "<mesh>_<material>"
    # Background and Donna show their texture unlit: one TEV stage that
    # replaces with texture 0 (rgb and alpha), later stages pass through.
    # The title is set up exactly like Donna (texture alpha only): tests 3-6
    # multiplied it by an animated material colour and it never showed.
    for name in ("bg_mat", "donna_mat", "text_mat"):
        mtob = cmdl.materials[name]
        mtob.flags = MTOBFlag(0)
        tc = mtob.fragment_shader.texture_combiners
        tc[0].src_rgb = tc[0].src_alpha = 0x003
        tc[0].combine_rgb = tc[0].combine_alpha = 0
        for c in tc[1:]:
            c.src_rgb = c.src_alpha = 0xFFF
            c.combine_rgb = c.combine_alpha = 0
    # Draw order and depth state (see the Z_* block). transluscency_kind
    # (sic, pycgfx's spelling) is the CGFX material's render layer 0..3;
    # pycgfx leaves it 0 for everything. Depth flags are what the runtime
    # builds the depth command from (pycgfx's BLEND case sets only the
    # flags); the cached command's test bit is kept in step anyway.
    for name, layer, flags in (
            ("bg_mat", 0, DepthFlag.TestEnabled | DepthFlag.MaskEnabled),
            ("heart_mat", 0, DepthFlag.TestEnabled | DepthFlag.MaskEnabled),
            ("donna_mat", 1, DepthFlag.TestEnabled),
            ("text_mat", 2, DepthFlag(0))):
        mtob = cmdl.materials[name]
        mtob.transluscency_kind = layer
        dop = mtob.fragment_operations.depth_operation
        dop.flags = flags
        dop.commands[0].param = (dop.commands[0].param & ~1) | (1 if flags & DepthFlag.TestEnabled else 0)
    # Heart: keep diffuse light, no added specular stage (it washes out).
    heart = cmdl.materials["heart_mat"]
    tc = heart.fragment_shader.texture_combiners
    tc[2].src_rgb = tc[2].src_alpha = 0xFFF
    tc[2].combine_rgb = tc[2].combine_alpha = 0
    # Spin: one linear rot_y curve over N_TURNS turns starting when the
    # scale-up ends; clamps to 0 before that.
    canm = cgfx.data.skeletal_animations["COMMON"]
    hb = canm.member_animations_data["heart"]
    hb.rot_x = 0.0
    hb.rot_z = 0.0
    hb.rot_y = linear_curve(T_ROT_START, 0.0, T_TOTAL, 2 * math.pi * N_TURNS)
    canm.frame_size = T_TOTAL * FPS
    canm.looping = True


def validate(cgfx, data):
    errors = []
    cmdl = cgfx.data.models["COMMON"]
    if list(cgfx.data.models) != ["COMMON"]:
        errors.append(f"models dict is {list(cgfx.data.models)}, want ['COMMON']")
    # The T2K rules (see build_gltf): bone order untouched by pycgfx's
    # re-sort; every layer a child of the scene root; billboards exactly on
    # the flat layers, whose bones sit at the origin; heart seat on the axis.
    bones = cmdl.skeleton.bones
    bone_names = list(bones)
    if bone_names[1:] != ["bg", "heartseat", "heart", "donna", "text"]:
        errors.append(f"bone order changed by pycgfx: {bone_names}")
    root = bone_names[0]
    for n in ("bg", "heartseat", "donna", "text"):
        if bones[n].parent is None or bones[n].parent.name != root:
            errors.append(f"bone {n} is not a child of the scene root")
    if bones["heart"].parent is None or bones["heart"].parent.name != "heartseat":
        errors.append("heart is not under heartseat")
    for n in bone_names:
        want = 5 if n in BILLBOARD_BONES else 0
        if int(bones[n].billboard_mode) != want:
            errors.append(f"bone {n} billboard mode {int(bones[n].billboard_mode)}, want {want}")
    for n in ("bg", "donna"):
        p = bones[n].position
        if (p.x, p.y, p.z) != (0, 0, 0):
            errors.append(f"billboard bone {n} is not at the origin")
    p = bones["text"].position
    if abs(p.x) > 1e-6 or abs(p.z) > 1e-6:
        errors.append("text bone is off the turn axis")
    p = bones["heartseat"].position
    if abs(p.x) > 1e-6 or abs(p.z) > 1e-6:
        errors.append("heartseat is off the turn axis")
    anim_bones = sorted(cgfx.data.skeletal_animations["COMMON"].member_animations_data)
    if anim_bones != ["donna", "heart", "text"]:
        errors.append(f"skeletal animation targets {anim_bones}, want donna, heart, text")
    if list(cgfx.data.material_animations):
        errors.append("material animation present (does not work on the HOME menu)")
    if list(cgfx.data.skeletal_animations) != ["COMMON"]:
        errors.append("skeletal animation is not named COMMON")
    for sh in items(cmdl.shapes):
        for ps in items(sh.primitive_sets):
            if ps.skinning_mode != 0:
                errors.append(f"shape {sh.name} uses skinning")
        for va in items(sh.vertex_attributes):
            if va.usage.name in ("BoneIndex", "BoneWeight"):
                errors.append(f"shape {sh.name} has bone streams")
    canm = cgfx.data.skeletal_animations["COMMON"]
    for name in canm.member_animations_data:
        if int(canm.member_animations_data[name].primitive_type) != 5:
            errors.append(f"animation member {name} is not Transform (5)")
    if len(data) > CGFX_LIMIT:
        errors.append(f"CGFX is {len(data)} bytes, limit {CGFX_LIMIT}")
    return errors


def main():
    os.makedirs(OUT, exist_ok=True)
    imgs = prepare_images()
    L = layout()
    gltf_path = build_gltf(L, imgs)
    install_texture_hook()
    gltf = gltflib.GLTF.load(gltf_path, load_file_resources=True)
    cgfx = pycgfx.convert_gltf(gltf)
    patch(cgfx)
    data = pycgfx.write(cgfx)
    errors = validate(cgfx, data)
    out_cgfx = os.path.join(OUT, "forsaken_banner.cgfx")
    with open(out_cgfx, "wb") as f:
        f.write(data)
    print(f"{out_cgfx}: {len(data)} bytes ({len(data) / CGFX_LIMIT:.0%} of 512 KiB)")
    for name, (size, fmt, _) in TEX.items():
        print(f"  texture {name}: {size[0]}x{size[1]} {fmt.name}")
    print(f"  heart: {len(L['heart'][0])} vertices, {len(L['heart'][3]) // 3} triangles")
    print(f"  timeline: slide {T_SLIDE_START:.2f}-{T_SLIDE_END:.2f}s, scale -{T_SCALE_END:.2f}s, "
          f"title in {T_TEXT_START:.2f}-{T_TEXT_END:.2f}s, spin from {T_ROT_START:.2f}s, loop at {T_TOTAL:.0f}s ({T_TOTAL * FPS:.0f} frames)")
    if errors:
        for e in errors:
            print("ERROR:", e)
        sys.exit(1)
    bnr = os.path.join(OUT, "forsaken_banner.bnr")
    subprocess.run([BANNERTOOL, "makebanner", "-ci", out_cgfx, "-a", SRC_AUDIO, "-o", bnr], check=True)
    if "--install" in sys.argv:
        shutil.copyfile(out_cgfx, INSTALL_CGFX)
        print(f"installed {INSTALL_CGFX} (Makefile.3ds builds the CIA banner from it)")

if __name__ == "__main__":
    main()
