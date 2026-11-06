#!/usr/bin/env python3
"""Generates src/models/torch.glb: a medieval torch (reference: src/art/torch.webp).

Pointed wooden handle, leather-wrapped grip, metal collar and, at the top, an iron cage
(vertical bars and two riveted hoops) with coals inside; above it, the flame.
Same art direction as the arena: wood and iron band come from src/art/arenaAtlas.png
(the atlas embedded in arena.glb); leather, coals and flame are procedural.
Standard library only (reuses helpers from textureArena.py).

Two nodes in the GLB:
  Torch  handle, grip, collar, cage and coals (lit normally)
  Flame  a quad the scene turns toward the camera; procedural flame in the shader (kFlame)

Units: approximate meters. Origin at the tip of the handle, +y axis up. The flame starts at
FLAME_BASE_Y and is centered at FLAME_CENTER_Y (scenes place the point light there).
The atlas (512x512, RGBA because of the flame) is saved to src/art/torchAtlas.png.

Usage: python3 tools/makeTorch.py
"""

import json
import math
import os
import random
import struct
import sys
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from textureArena import Image, blit_scaled, crop, metal_fill, read_png  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_GLB = os.path.join(ROOT, "src", "models", "torch.glb")
OUT_ATLAS = os.path.join(ROOT, "src", "art", "torchAtlas.png")
ARENA_ATLAS = os.path.join(ROOT, "src", "art", "arenaAtlas.png")
ARENA_PLANKS = (20, 1538, 1530, 1972)
ARENA_BAND = (0, 1986, 1536, 2046)
ARENA_METAL = (74, 76, 82)

SEGMENTS = 14
HANDLE_TOP = 0.30
WRAP_TOP = 0.56
COLLAR_TOP = 0.64
CAGE_BOTTOM = 0.66
CAGE_TOP = 0.98
CAGE_R0, CAGE_R1 = 0.085, 0.118
N_BARS = 8
COALS_Y = 0.80
FLAME_BASE_Y = 0.82
FLAME_CENTER_Y = 1.04
FLAME_TOP_Y = 1.48
FLAME_HALF_W = 0.20

ATLAS = 512
WOOD = (0, 0, 256, 256)
WRAP = (256, 0, 512, 256)
METAL = (0, 256, 256, 320)
HOOP = (0, 320, 512, 352)
COALS = (256, 256, 384, 320)
FLAME = (384, 256, 512, 512)


def write_png_rgba(path, width, height, rgba):
    raw = bytearray()
    stride = width * 4
    for y in range(height):
        raw.append(0)
        raw.extend(rgba[y * stride:(y + 1) * stride])

    def chunk(ctype, data):
        c = struct.pack(">I", len(data)) + ctype + data
        return c + struct.pack(">I", zlib.crc32(ctype + data) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)
    return png


class Noise:
    def __init__(self, seed):
        rng = random.Random(seed)
        self.table = [rng.random() for _ in range(256 * 256)]

    def at(self, x, y):
        ix, iy = math.floor(x), math.floor(y)
        fx, fy = x - ix, y - iy
        fx = fx * fx * (3 - 2 * fx)
        fy = fy * fy * (3 - 2 * fy)
        t = self.table
        i0, i1 = (ix & 255), ((ix + 1) & 255)
        j0, j1 = (iy & 255) << 8, ((iy + 1) & 255) << 8
        a = t[j0 | i0] + (t[j0 | i1] - t[j0 | i0]) * fx
        b = t[j1 | i0] + (t[j1 | i1] - t[j1 | i0]) * fx
        return a + (b - a) * fy

    def fbm(self, x, y, octaves=3):
        v, amp, total = 0.0, 1.0, 0.0
        for _ in range(octaves):
            v += self.at(x, y) * amp
            total += amp
            amp *= 0.5
            x, y = x * 2.1 + 7.3, y * 2.1 + 3.1
        return v / total


def clamp8(v):
    return 0 if v < 0 else (255 if v > 255 else int(v))


class Canvas:
    """RGBA; exposes an RGB view (Image from textureArena) for the arena helpers."""

    def __init__(self, size):
        self.w = self.h = size
        self.px = bytearray(size * size * 4)
        for i in range(size * size):
            self.px[i * 4 + 3] = 255

    def put(self, x, y, rgb, a=255):
        i = (y * self.w + x) * 4
        self.px[i] = clamp8(rgb[0])
        self.px[i + 1] = clamp8(rgb[1])
        self.px[i + 2] = clamp8(rgb[2])
        self.px[i + 3] = clamp8(a)

    def rgb_view(self):
        rgb = bytearray(self.w * self.h * 3)
        for i in range(self.w * self.h):
            rgb[i * 3:i * 3 + 3] = self.px[i * 4:i * 4 + 3]
        return Image(self.w, self.h, rgb)

    def from_rgb_view(self, img, region):
        x0, y0, x1, y1 = region
        for y in range(y0, y1):
            for x in range(x0, x1):
                self.put(x, y, img.get(x, y))


def paint_arena_parts(canvas, arena, rng):
    view = canvas.rgb_view()
    planks = crop(arena, ARENA_PLANKS)
    blit_scaled(view, planks, *WOOD, rot90=True)
    band = crop(arena, ARENA_BAND)
    blit_scaled(view, band, *HOOP)
    metal_fill(view, *METAL, rng, base=ARENA_METAL)
    for region in (WOOD, HOOP, METAL):
        canvas.from_rgb_view(view, region)


def paint_wrap(canvas, region, seed=21):
    """Wrapped dark leather: diagonal strips with a light edge and shadow, fine grain."""
    x0, y0, x1, y1 = region
    noise = Noise(seed)
    base = (72, 54, 40)
    band_h = 22
    for yy in range(y0, y1):
        for xx in range(x0, x1):
            k = (yy - y0 + (xx - x0) * 0.18) % band_h
            shade = 0.9 + (noise.fbm(xx * 0.12, yy * 0.12) - 0.5) * 0.5
            if k < 2:
                shade *= 0.45
            elif k < 5:
                shade *= 1.25
            elif k > band_h - 3:
                shade *= 0.75
            canvas.put(xx, yy, (base[0] * shade, base[1] * shade, base[2] * shade))


def paint_coals(canvas, region, seed=33):
    x0, y0, x1, y1 = region
    noise = Noise(seed)
    for yy in range(y0, y1):
        for xx in range(x0, x1):
            n = noise.fbm(xx * 0.15, yy * 0.15, 4)
            heat = max(0.0, (n - 0.35) / 0.5)
            r = 60 + 195 * heat
            g = 20 + 120 * heat * heat
            b = 10 + 30 * heat * heat * heat
            canvas.put(xx, yy, (r, g, b))


def paint_flame(canvas, region, seed=44):
    """Teardrop flame: near-white core, orange middle, red edge, soft alpha."""
    x0, y0, x1, y1 = region
    w, h = x1 - x0, y1 - y0
    noise = Noise(seed)
    for yy in range(y0, y1):
        t = 1.0 - (yy - y0 + 0.5) / h
        wobble = (noise.fbm(3.0, t * 6.0) - 0.5) * 0.25
        half = 0.92 * math.sin(math.pi * (t ** 0.55)) ** 0.9
        center = wobble * (t ** 1.5)
        for xx in range(x0, x1):
            nx = ((xx - x0 + 0.5) / w) * 2.0 - 1.0 - center
            d = abs(nx) / max(half, 1e-3)
            n = noise.fbm(xx * 0.08, yy * 0.05 + 3.0, 3)
            d = d * (0.85 + 0.3 * n)
            if d >= 1.0:
                canvas.put(xx, yy, (0, 0, 0), 0)
                continue
            core = (1.0 - d) ** 3 * (1.0 - t) ** 0.6
            alpha = (1.0 - d * d) ** 1.2 * (1.0 - t ** 2.2)
            r = 230 + 25 * core
            g = 60 + 120 * (1.0 - d) + 70 * core
            b = 10 + 40 * (1.0 - d) ** 2 + 150 * core
            canvas.put(xx, yy, (r, g, b), alpha * 255)


def build_atlas():
    rng = random.Random(7)
    canvas = Canvas(ATLAS)
    aw, ah, argb = read_png(ARENA_ATLAS)
    arena = Image(aw, ah, argb)
    paint_arena_parts(canvas, arena, rng)
    paint_wrap(canvas, WRAP)
    paint_coals(canvas, COALS)
    paint_flame(canvas, FLAME)
    return canvas


class Mesh:
    def __init__(self, name):
        self.name = name
        self.pos, self.uv, self.idx = [], [], []

    def quad(self, a, b, c, d, ta, tb, tc, td):
        base = len(self.pos)
        self.pos += [a, b, c, d]
        self.uv += [ta, tb, tc, td]
        self.idx += [base, base + 1, base + 2, base, base + 2, base + 3]


def region_uv(region, fu, fv):
    x0, y0, x1, y1 = region
    px = x0 + 0.5 + fu * (x1 - x0 - 1)
    py = y0 + 0.5 + fv * (y1 - y0 - 1)
    return (px / ATLAS, py / ATLAS)


def lathe(mesh, profile, region, segments=SEGMENTS, v_repeat=1.0):
    """Surface of revolution around +y. profile: list of (y, r), bottom to top.
    u runs around the region; v runs along the length of the profile."""
    lengths = [0.0]
    for i in range(1, len(profile)):
        (y0, r0), (y1, r1) = profile[i - 1], profile[i]
        lengths.append(lengths[-1] + math.hypot(y1 - y0, r1 - r0))
    total = max(lengths[-1], 1e-6)
    for i in range(len(profile) - 1):
        (ya, ra), (yb, rb) = profile[i], profile[i + 1]
        va, vb = lengths[i] / total * v_repeat, lengths[i + 1] / total * v_repeat
        for s in range(segments):
            a0 = 2 * math.pi * s / segments
            a1 = 2 * math.pi * (s + 1) / segments
            u0, u1 = s / segments, (s + 1) / segments
            p0 = (ra * math.cos(a0), ya, ra * math.sin(a0))
            p1 = (ra * math.cos(a1), ya, ra * math.sin(a1))
            p2 = (rb * math.cos(a1), yb, rb * math.sin(a1))
            p3 = (rb * math.cos(a0), yb, rb * math.sin(a0))
            mesh.quad(p0, p1, p2, p3, region_uv(region, u0, 1 - va % 1.0),
                      region_uv(region, u1, 1 - va % 1.0), region_uv(region, u1, 1 - vb % 1.0),
                      region_uv(region, u0, 1 - vb % 1.0))


def bar(mesh, y0, y1, r0, r1, angle, width, thick, region):
    """Vertical cage bar: thin box oriented radially, tilted outward."""
    c, s = math.cos(angle), math.sin(angle)
    tx, tz = -s, c

    def p(y, r, dw, dr):
        rr = r + dr
        return (rr * c + tx * dw, y, rr * s + tz * dw)

    hw, ht = width / 2, thick / 2
    corners_b = [p(y0, r0, -hw, -ht), p(y0, r0, hw, -ht), p(y0, r0, hw, ht), p(y0, r0, -hw, ht)]
    corners_t = [p(y1, r1, -hw, -ht), p(y1, r1, hw, -ht), p(y1, r1, hw, ht), p(y1, r1, -hw, ht)]
    uv = lambda fu, fv: region_uv(region, fu * 0.2, fv)
    for k in range(4):
        a, b = corners_b[k], corners_b[(k + 1) % 4]
        d, c2 = corners_t[k], corners_t[(k + 1) % 4]
        mesh.quad(a, b, c2, d, uv(0, 1), uv(1, 1), uv(1, 0), uv(0, 0))
    mesh.quad(*corners_t, uv(0, 0), uv(1, 0), uv(1, 0.1), uv(0, 0.1))


def build_torch():
    m = Mesh("Torch")
    lathe(m, [(0.0, 0.0), (0.05, 0.022), (0.09, 0.03), (0.11, 0.034), (HANDLE_TOP, 0.034)], WOOD,
          v_repeat=1.0)
    lathe(m, [(HANDLE_TOP, 0.034), (HANDLE_TOP + 0.01, 0.042), (0.43, 0.046), (WRAP_TOP - 0.01, 0.042),
              (WRAP_TOP, 0.036)], WRAP, v_repeat=2.0)
    lathe(m, [(WRAP_TOP, 0.036), (WRAP_TOP, 0.05), (0.60, 0.05), (0.61, 0.04), (COLLAR_TOP, 0.04),
              (CAGE_BOTTOM, 0.06), (CAGE_BOTTOM, CAGE_R0), (CAGE_BOTTOM + 0.015, CAGE_R0),
              (CAGE_BOTTOM + 0.015, 0.0)], METAL)
    for k in range(N_BARS):
        bar(m, CAGE_BOTTOM + 0.015, CAGE_TOP, CAGE_R0, CAGE_R1, 2 * math.pi * k / N_BARS, 0.026,
            0.012, METAL)
    for frac in (0.28, 0.78):
        y = CAGE_BOTTOM + (CAGE_TOP - CAGE_BOTTOM) * frac
        r = CAGE_R0 + (CAGE_R1 - CAGE_R0) * frac + 0.008
        lathe(m, [(y - 0.016, r), (y + 0.016, r)], HOOP, v_repeat=1.0)
    rc = CAGE_R0 + (CAGE_R1 - CAGE_R0) * ((COALS_Y - CAGE_BOTTOM) / (CAGE_TOP - CAGE_BOTTOM)) - 0.01
    lathe(m, [(COALS_Y - 0.06, rc - 0.005), (COALS_Y, rc), (COALS_Y, 0.0)], COALS)
    return m


def build_flame():
    """A quad in the XY plane (normal +z); the scene rotates it to face the camera and keeps it
    vertical. Plain 0..1 UV: the shader (Gpu::DrawMode::kFlame) draws the flame procedurally
    from UV and time; u = width, v = 0 at the top and 1 at the base. The FLAME region of the
    atlas is only a reference/preview."""
    m = Mesh("Flame")
    hw = FLAME_HALF_W
    m.quad((-hw, FLAME_TOP_Y, 0.0), (hw, FLAME_TOP_Y, 0.0), (hw, FLAME_BASE_Y, 0.0),
           (-hw, FLAME_BASE_Y, 0.0), (0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0))
    return m


def pad4(b, fill=b"\x00"):
    return b + fill * ((4 - len(b) % 4) % 4)


def write_glb(path, meshes, png_bytes):
    views, accessors, gl_meshes, nodes = [], [], [], []
    blob = bytearray()

    def add_view(b, target=None):
        start = len(blob)
        blob.extend(pad4(b))
        v = {"buffer": 0, "byteOffset": start, "byteLength": len(b)}
        if target:
            v["target"] = target
        views.append(v)
        return len(views) - 1

    for mesh in meshes:
        idx_b = b"".join(struct.pack("<I", i) for i in mesh.idx)
        pos_b = b"".join(struct.pack("<fff", *p) for p in mesh.pos)
        uv_b = b"".join(struct.pack("<ff", *t) for t in mesh.uv)
        mins = [min(p[k] for p in mesh.pos) for k in range(3)]
        maxs = [max(p[k] for p in mesh.pos) for k in range(3)]
        a0 = len(accessors)
        accessors.append({"bufferView": add_view(idx_b, 34963), "componentType": 5125,
                          "count": len(mesh.idx), "type": "SCALAR", "min": [0],
                          "max": [len(mesh.pos) - 1]})
        accessors.append({"bufferView": add_view(pos_b, 34962), "componentType": 5126,
                          "count": len(mesh.pos), "type": "VEC3", "min": mins, "max": maxs})
        accessors.append({"bufferView": add_view(uv_b, 34962), "componentType": 5126,
                          "count": len(mesh.uv), "type": "VEC2"})
        gl_meshes.append({"name": mesh.name, "primitives": [{
            "attributes": {"POSITION": a0 + 1, "TEXCOORD_0": a0 + 2}, "indices": a0,
            "mode": 4, "material": 0}]})
        nodes.append({"name": mesh.name, "mesh": len(gl_meshes) - 1})
    v_img = add_view(png_bytes)

    gltf = {
        "asset": {"version": "2.0", "generator": "PONTIFICE tools/makeTorch.py"},
        "scene": 0,
        "scenes": [{"nodes": list(range(len(nodes)))}],
        "nodes": nodes,
        "meshes": gl_meshes,
        "materials": [{"name": "TorchAtlas", "doubleSided": True, "alphaMode": "BLEND",
                       "pbrMetallicRoughness": {"baseColorTexture": {"index": 0},
                                                "metallicFactor": 0.2, "roughnessFactor": 0.8}}],
        "textures": [{"source": 0, "sampler": 0}],
        "samplers": [{"magFilter": 9729, "minFilter": 9987, "wrapS": 33071, "wrapT": 33071}],
        "images": [{"bufferView": v_img, "mimeType": "image/png", "name": "TorchAtlas"}],
        "accessors": accessors,
        "bufferViews": views,
        "buffers": [{"byteLength": len(blob)}],
    }
    json_bytes = pad4(json.dumps(gltf, separators=(",", ":")).encode("utf-8"), b" ")
    bin_bytes = bytes(blob)
    total = 12 + 8 + len(json_bytes) + 8 + len(bin_bytes)
    with open(path, "wb") as f:
        f.write(struct.pack("<4sII", b"glTF", 2, total))
        f.write(struct.pack("<I4s", len(json_bytes), b"JSON"))
        f.write(json_bytes)
        f.write(struct.pack("<I4s", len(bin_bytes), b"BIN\x00"))
        f.write(bin_bytes)


def main():
    torch, flame = build_torch(), build_flame()
    print("torch: %d triangles; flame: %d" % (len(torch.idx) // 3, len(flame.idx) // 3))
    atlas = build_atlas()
    png_bytes = write_png_rgba(OUT_ATLAS, atlas.w, atlas.h, atlas.px)
    print("atlas saved to", OUT_ATLAS, len(png_bytes), "bytes")
    write_glb(OUT_GLB, [torch, flame], png_bytes)
    print("wrote", OUT_GLB, os.path.getsize(OUT_GLB), "bytes")
    import normalMap
    normalMap.add_to_glb(OUT_GLB, strength=2.0)
    print("flame: base y=%.2f center y=%.2f" % (FLAME_BASE_Y, FLAME_CENTER_Y))


if __name__ == "__main__":
    main()
