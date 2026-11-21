#!/usr/bin/env python3
"""Software preview of the banner (see SPEC.md): renders build/scene.gltf
through the HOME-menu camera at 400x240 with the same timeline the CGFX
carries, and writes build/preview.gif plus build/preview_final.png.

This checks composition and timing only. Lighting is a plain Lambert term,
not the HOME menu's; the real check is the HOME menu on hardware.

    banner/.tools/venv/bin/python banner/preview_banner.py [seconds]
"""

import json
import math
import os
import sys

import numpy as np
from PIL import Image

import build_banner as bb

W, H = 400, 240
SS = 2  # supersampling


def load_scene():
    doc = json.load(open(os.path.join(bb.OUT, "scene.gltf")))
    buf = open(os.path.join(bb.OUT, "scene.bin"), "rb").read()

    def acc(i):
        a = doc["accessors"][i]
        v = doc["bufferViews"][a["bufferView"]]
        dt = {5126: np.float32, 5123: np.uint16}[a["componentType"]]
        n = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}[a["type"]]
        return np.frombuffer(buf, dt, a["count"] * n, v["byteOffset"]).reshape(a["count"], n)

    meshes = {}
    for m in doc["meshes"]:
        p = m["primitives"][0]
        mat = doc["materials"][p["material"]]
        img = doc["images"][doc["textures"][mat["pbrMetallicRoughness"]["baseColorTexture"]["index"]]["source"]]
        tex = np.asarray(Image.open(os.path.join(bb.OUT, img["uri"])).convert("RGBA"), np.float32) / 255
        meshes[m["name"]] = dict(pos=acc(p["attributes"]["POSITION"]), nrm=acc(p["attributes"]["NORMAL"]),
                                 uv=acc(p["attributes"]["TEXCOORD_0"]), idx=acc(p["indices"]).reshape(-1, 3),
                                 tex=tex, blend=mat["alphaMode"] == "BLEND", lit=m["name"] == "heart")
    return meshes


def state(t, L):
    """Node transforms at time t, mirroring the CGFX curves."""
    def eased(t0, t1, v0, v1):
        x = min(max((t - t0) / (t1 - t0), 0.0), 1.0)
        return v0 + (v1 - v0) * bb.ease_out_cubic(x)
    dx = eased(bb.T_SLIDE_START, bb.T_SLIDE_END, L["donna_offscreen_dx"], 0.0)
    s = eased(bb.T_SLIDE_END, bb.T_SCALE_END, bb.SCALE_START, 1.0)
    ang = max(0.0, t - bb.T_ROT_START) / bb.T_TURN * 2 * math.pi
    text_s = eased(bb.T_TEXT_START, bb.T_TEXT_END, bb.SCALE_START, 1.0)
    return dx, s, ang, text_s


def project(p):
    hh = math.tan(bb.CAM_YFOV / 2)
    d = bb.CAM_Z - p[:, 2]
    x = p[:, 0] / (d * hh * bb.CAM_ASPECT)
    y = (p[:, 1] - bb.CAM_Y) / (d * hh)
    return np.stack([(x * 0.5 + 0.5) * W * SS, (0.5 - y * 0.5) * H * SS, d], 1)


def raster(color, depth, mesh, pos, nrm, alpha_mul=1.0, depth_test=True, depth_write=True):
    sp = project(pos)
    tex = mesh["tex"]
    th, tw = tex.shape[:2]
    light = np.array([0.3, 0.5, 1.0]) / np.linalg.norm([0.3, 0.5, 1.0])
    for tri in mesh["idx"]:
        a, b, c = sp[tri]
        x0, x1 = int(max(0, min(a[0], b[0], c[0]))), int(min(W * SS - 1, max(a[0], b[0], c[0]) + 1))
        y0, y1 = int(max(0, min(a[1], b[1], c[1]))), int(min(H * SS - 1, max(a[1], b[1], c[1]) + 1))
        if x0 > x1 or y0 > y1:
            continue
        area = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])
        if abs(area) < 1e-9:
            continue
        ys, xs = np.mgrid[y0:y1 + 1, x0:x1 + 1] + 0.5
        w0 = ((b[0] - xs) * (c[1] - ys) - (b[1] - ys) * (c[0] - xs)) / area
        w1 = ((c[0] - xs) * (a[1] - ys) - (c[1] - ys) * (a[0] - xs)) / area
        w2 = 1 - w0 - w1
        inside = (w0 >= 0) & (w1 >= 0) & (w2 >= 0)
        if not inside.any():
            continue
        # perspective-correct weights
        iz = w0 / a[2] + w1 / b[2] + w2 / c[2]
        p0, p1, p2 = w0 / a[2] / iz, w1 / b[2] / iz, w2 / c[2] / iz
        z = 1 / iz
        reg = (slice(y0, y1 + 1), slice(x0, x1 + 1))
        ok = inside & (z < depth[reg]) if depth_test else inside
        if not ok.any():
            continue
        uv = mesh["uv"][tri]
        u = p0 * uv[0, 0] + p1 * uv[1, 0] + p2 * uv[2, 0]
        v = p0 * uv[0, 1] + p1 * uv[1, 1] + p2 * uv[2, 1]
        tx = np.clip((u * tw).astype(int), 0, tw - 1)
        ty = np.clip((v * th).astype(int), 0, th - 1)
        rgba = tex[ty, tx]
        rgb = rgba[..., :3]
        if mesh["lit"]:
            n = nrm[tri]
            nn = p0[..., None] * n[0] + p1[..., None] * n[1] + p2[..., None] * n[2]
            nn /= np.linalg.norm(nn, axis=-1, keepdims=True) + 1e-9
            rgb = rgb * (0.35 + 0.75 * np.clip(nn @ light, 0, 1))[..., None]
        dst = color[reg]
        if mesh["blend"]:
            al = rgba[..., 3:4] * alpha_mul
            dst[ok] = (rgb * al + dst * (1 - al))[ok]
        else:
            dst[ok] = rgb[ok]
            if depth_write:
                depth[reg][ok] = z[ok]


def render(t, meshes, L):
    dx, s, ang, text_s = state(t, L)
    color = np.zeros((H * SS, W * SS, 3), np.float32)
    depth = np.full((H * SS, W * SS), np.inf, np.float32)
    # depth state mirrors build_banner.patch(): bg neither tests nor writes,
    # Donna tests, the title ignores depth.
    raster(color, depth, meshes["bg"], meshes["bg"]["pos"], meshes["bg"]["nrm"],
           depth_test=False, depth_write=False)
    m = meshes["heart"]
    ca, sa = math.cos(ang), math.sin(ang)
    rot = np.array([[ca, 0, sa], [0, 1, 0], [-sa, 0, ca]], np.float32)
    raster(color, depth, m, (m["pos"] * s) @ rot.T + np.array(L["heart_center"], np.float32), m["nrm"] @ rot.T)
    m = meshes["donna"]
    raster(color, depth, m, m["pos"] + np.array([dx, 0, 0], np.float32), m["nrm"])
    m = meshes["text"]  # after Donna, as in the CGFX mesh order
    raster(color, depth, m, m["pos"] * text_s + np.array([0, L["text_center_y"], 0], np.float32),
           m["nrm"], depth_test=False)
    img = Image.fromarray((np.clip(color, 0, 1) * 255).astype(np.uint8))
    return img.resize((W, H), Image.LANCZOS)


def main():
    seconds = float(sys.argv[1]) if len(sys.argv) > 1 else 2.5
    L = bb.layout()
    meshes = load_scene()
    frames = [render(i / 30, meshes, L) for i in range(int(seconds * 30) + 1)]
    frames[0].save(os.path.join(bb.OUT, "preview.gif"), save_all=True, append_images=frames[1:],
                   duration=33, loop=0)
    render(bb.T_TEXT_END, meshes, L).save(os.path.join(bb.OUT, "preview_final.png"))
    print("wrote build/preview.gif and build/preview_final.png")


if __name__ == "__main__":
    main()
