#!/usr/bin/env python3
"""Gera src/models/Door.glb: portão medieval da tela de loading (docs/loading.md).

Inspirado em src/art/Door.jpg: parede de pedra com arco de aduelas grandes, um segundo arco
recuado menor e, dentro dele, uma porta de tábuas verticais com bandas de ferro rebitadas,
dobradiças à esquerda e argola à direita. A pedra é procedural; a madeira e o ferro vêm do
atlas da arena (src/art/ArenaAtlas.png, o mesmo embutido em Arena.glb), para manter a mesma
direção de arte. Só biblioteca padrão (reaproveita utilitários de texture_arena.py).

Dois nós no GLB, para a cena animar a porta separadamente da parede:
  Wall  parede, arcos e passagem retangular atrás deles (fixa)
  Door  folha da porta com ferragens; gira em torno da dobradiça em x = -DOOR_R, z = DOOR_Z

Unidades: metros aproximados. Frente da parede em z = 0, câmera em +z, porta abre para -z.
O atlas procedural (1024x1024) vai embutido no GLB e também é salvo em src/art/DoorAtlas.png.

Uso: python3 tools/make_door.py
"""

import json
import math
import os
import random
import struct
import sys
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from texture_arena import Image, blit_scaled, crop, metal_fill, read_png  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_GLB = os.path.join(ROOT, "src", "models", "Door.glb")
OUT_ATLAS = os.path.join(ROOT, "src", "art", "DoorAtlas.png")
ARENA_ATLAS = os.path.join(ROOT, "src", "art", "ArenaAtlas.png")
# Recortes no atlas da arena (x0, y0, x1, y1): bloco de tábuas e banda de ferro rebitada.
ARENA_PLANKS = (20, 1538, 1530, 1972)
ARENA_BAND = (0, 1986, 1536, 2046)
ARENA_METAL = (74, 76, 82)  # base do metal_fill usado pela arena

# ----------------------------------------------------------------------------- medidas
WALL_HALF = 1.9      # meia largura da parede
WALL_TOP = 4.1       # altura da parede
WALL_DEPTH = 0.8     # espessura (z de 0 a -WALL_DEPTH)
ARCH_CY = 2.1        # altura do centro dos arcos (linha de nascença)
ARCH_R0 = 1.5        # raio do arco externo (abertura na frente)
VOUSSOIR_W = 0.4     # largura da faixa de aduelas grandes
RECESS_Z = -0.22     # profundidade do primeiro recuo
ARCH_R1 = 1.1        # raio do arco interno (abertura recuada)
DOOR_Z = -0.42       # frente da porta
DOOR_R = 1.07        # meia largura / raio da porta
DOOR_T = 0.10        # espessura da porta
ARCH_SEGMENTS = 28

# ----------------------------------------------------------------------------- atlas (px)
ATLAS = 1024
PX_PER_UNIT = 128
STONE = (0, 0, 512, 512)         # pedra da parede (planar, 128 px/unidade)
WOOD = (512, 0, 1024, 512)       # tábuas da porta
VOUSSOIR = (0, 512, 1024, 640)   # aduelas grandes (u ao longo do arco)
SMALL_ARCH = (0, 640, 1024, 704) # arco interno, pedras menores
METAL_RIVET = (0, 704, 1024, 768)  # banda de ferro com rebites a cada 64 px
METAL_PLAIN = (0, 768, 512, 832)   # ferro liso


# ----------------------------------------------------------------------------- PNG

def write_png(path, width, height, rgb):
    raw = bytearray()
    stride = width * 3
    for y in range(height):
        raw.append(0)
        raw.extend(rgb[y * stride:(y + 1) * stride])

    def chunk(ctype, data):
        c = struct.pack(">I", len(data)) + ctype + data
        return c + struct.pack(">I", zlib.crc32(ctype + data) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)
    return png


# ----------------------------------------------------------------------------- ruído

class Noise:
    """Ruído de valor 2D com interpolação suave; lattice de 256x256 repetida."""

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
    def __init__(self, size):
        self.w = self.h = size
        self.px = bytearray(size * size * 3)

    def put(self, x, y, rgb):
        i = (y * self.w + x) * 3
        self.px[i] = clamp8(rgb[0])
        self.px[i + 1] = clamp8(rgb[1])
        self.px[i + 2] = clamp8(rgb[2])


# ----------------------------------------------------------------------------- texturas

def paint_stone(canvas, region, rng, base, row_h, min_w, max_w, mortar=4, seed=1):
    """Fiadas de blocos com juntas escuras, bisel e grão."""
    x0, y0, x1, y1 = region
    noise = Noise(seed)
    rows = []
    y = y0
    while y < y1:
        xs = [x0 - rng.randint(0, max_w)]
        while xs[-1] < x1:
            xs.append(xs[-1] + rng.randint(min_w, max_w))
        shades = [rng.uniform(0.86, 1.14) for _ in xs]
        rows.append((y, min(y + row_h, y1), xs, shades))
        y += row_h
    for (ry0, ry1, xs, shades) in rows:
        for yy in range(ry0, ry1):
            k = 0
            for xx in range(x0, x1):
                while k + 1 < len(xs) and xs[k + 1] <= xx:
                    k += 1
                bx0, bx1 = xs[k], xs[k + 1] if k + 1 < len(xs) else x1
                in_mortar = (xx - bx0 < mortar) or (yy - ry0 < mortar)
                g = noise.fbm(xx * 0.05, yy * 0.05) - 0.5
                grain = noise.at(xx * 0.9, yy * 0.9) - 0.5
                if in_mortar:
                    shade = 0.42 + g * 0.2
                else:
                    shade = shades[k] + g * 0.35 + grain * 0.12
                    # bisel: borda superior/esquerda clara, inferior/direita escura
                    dl, dt = xx - bx0 - mortar, yy - ry0 - mortar
                    dr, db = bx1 - 1 - xx, ry1 - 1 - yy
                    if dl < 3 or dt < 3:
                        shade *= 1.18
                    elif dr < 3 or db < 3:
                        shade *= 0.78
                canvas.put(xx, yy, (base[0] * shade, base[1] * shade, base[2] * shade))


def paint_arena_wood(canvas, region, arena):
    """Tábuas da arena giradas 90° (verticais na porta), em duas colunas espelhadas."""
    x0, y0, x1, y1 = region
    planks = crop(arena, ARENA_PLANKS)
    dst = Image(canvas.w, canvas.h, canvas.px)
    mid = (x0 + x1) // 2
    blit_scaled(dst, planks, x0, y0, mid, y1, rot90=True)
    blit_scaled(dst, planks, mid, y0, x1, y1, rot90=True, flip_x=True)


def paint_arena_band(canvas, region, arena):
    """Banda de ferro rebitada da arena, esticada ao longo da faixa."""
    x0, y0, x1, y1 = region
    band = crop(arena, ARENA_BAND)
    dst = Image(canvas.w, canvas.h, canvas.px)
    blit_scaled(dst, band, x0, y0, x1, y1)


def paint_plain_metal(canvas, region, rng):
    x0, y0, x1, y1 = region
    dst = Image(canvas.w, canvas.h, canvas.px)
    metal_fill(dst, x0, y0, x1, y1, rng, base=ARENA_METAL)


def build_atlas():
    rng = random.Random(42)
    canvas = Canvas(ATLAS)
    aw, ah, argb = read_png(ARENA_ATLAS)
    arena = Image(aw, ah, argb)
    paint_stone(canvas, STONE, rng, (142, 142, 146), row_h=64, min_w=90, max_w=170, seed=1)
    paint_arena_wood(canvas, WOOD, arena)
    paint_stone(canvas, VOUSSOIR, rng, (160, 160, 164), row_h=128, min_w=80, max_w=92,
                mortar=5, seed=2)
    paint_stone(canvas, SMALL_ARCH, rng, (134, 134, 138), row_h=64, min_w=46, max_w=56,
                mortar=4, seed=3)
    paint_arena_band(canvas, METAL_RIVET, arena)
    paint_plain_metal(canvas, METAL_PLAIN, rng)
    return canvas


# ----------------------------------------------------------------------------- malha

class Mesh:
    def __init__(self, name):
        self.name = name
        self.pos, self.uv, self.idx = [], [], []

    def tri(self, a, b, c, ta, tb, tc):
        base = len(self.pos)
        self.pos += [a, b, c]
        self.uv += [ta, tb, tc]
        self.idx += [base, base + 1, base + 2]

    def quad(self, a, b, c, d, ta, tb, tc, td):
        base = len(self.pos)
        self.pos += [a, b, c, d]
        self.uv += [ta, tb, tc, td]
        self.idx += [base, base + 1, base + 2, base, base + 2, base + 3]


def region_uv(region, px, py):
    """Pixel absoluto dentro de uma região do atlas (clampado) para UV."""
    x0, y0, x1, y1 = region
    px = min(max(px, x0 + 0.5), x1 - 0.5)
    py = min(max(py, y0 + 0.5), y1 - 0.5)
    return (px / ATLAS, py / ATLAS)


def planar(region, a, b, scale=PX_PER_UNIT):
    """Mapeamento planar: a, b em unidades a partir do canto superior esquerdo da região."""
    return region_uv(region, region[0] + a * scale, region[1] + b * scale)


def strip_uv(region, a, b, height_units, scale=PX_PER_UNIT):
    """Faixa horizontal (bandas de metal): centrada verticalmente na região."""
    h_px = region[3] - region[1]
    top = region[1] + (h_px - height_units * scale) / 2
    return region_uv(region, region[0] + a * scale, top + b * scale)


def box(mesh, x0, x1, y0, y1, z0, z1, region, uvmode="planar", scale=PX_PER_UNIT):
    """Caixa alinhada aos eixos. uvmode planar: cada face mapeada pelo seu plano;
    strip: faces mapeadas na faixa (comprimento em x ou z, altura em y)."""
    sx, sy, sz = x1 - x0, y1 - y0, z1 - z0

    def uv(a, b, h):
        if uvmode == "strip":
            return strip_uv(region, a, b, h, scale)
        return planar(region, a, b, scale)

    # frente (+z) e trás (-z)
    for z, flip in ((z1, False), (z0, True)):
        mesh.quad((x0, y1, z), (x1, y1, z), (x1, y0, z), (x0, y0, z),
                  uv(0, 0, sy), uv(sx, 0, sy), uv(sx, sy, sy), uv(0, sy, sy))
    # topo (+y) e base (-y)
    for y in (y1, y0):
        mesh.quad((x0, y, z0), (x1, y, z0), (x1, y, z1), (x0, y, z1),
                  uv(0, 0, sz), uv(sx, 0, sz), uv(sx, sz, sz), uv(0, sz, sz))
    # lados (±x)
    for x in (x0, x1):
        mesh.quad((x, y1, z0), (x, y1, z1), (x, y0, z1), (x, y0, z0),
                  uv(0, 0, sy), uv(sz, 0, sy), uv(sz, sy, sy), uv(0, sy, sy))


def arch_outline(r, cy, extra_angles=()):
    """Contorno de uma abertura em arco pleno: sobe a ombreira esquerda, percorre o arco e
    desce a ombreira direita. Retorna lista de (x, y, s) com s = comprimento acumulado."""
    pts = [(-r, 0.0)]
    pts.append((-r, cy))
    angles = sorted(set([math.pi * (1 - i / ARCH_SEGMENTS) for i in range(ARCH_SEGMENTS + 1)]
                        + list(extra_angles)), reverse=True)
    for a in angles:
        if 0.0 < a < math.pi:
            pts.append((r * math.cos(a) + 0.0, cy + r * math.sin(a)))
    pts.append((r, cy))
    pts.append((r, 0.0))
    out, s = [], 0.0
    for i, (x, y) in enumerate(pts):
        if i > 0:
            s += math.hypot(x - pts[i - 1][0], y - pts[i - 1][1])
        out.append((x, y, s))
    return out


def offset_point(x, y, cy, d):
    """Afasta um ponto do contorno em d: radial no arco, horizontal nas ombreiras."""
    if y > cy + 1e-6:
        ang = math.atan2(y - cy, x)
        r = math.hypot(x, y - cy) + d
        return (r * math.cos(ang), cy + r * math.sin(ang))
    return (x + d if x > 0 else x - d, y)


def project_to_rect(x, y, cy, half_w, top):
    """Ponto do contorno projetado na borda do retângulo da parede (raio a partir do centro
    do arco no arco; horizontal nas ombreiras)."""
    if y <= cy + 1e-6:
        return (half_w if x > 0 else -half_w, y)
    dx, dy = x, y - cy
    t = float("inf")
    if dx > 1e-6:
        t = min(t, half_w / dx)
    elif dx < -1e-6:
        t = min(t, -half_w / dx)
    if dy > 1e-6:
        t = min(t, (top - cy) / dy)
    return (dx * t, cy + dy * t)


def ring(mesh, inner, outer, z, uv_inner, uv_outer):
    """Anel plano entre dois contornos com o mesmo número de pontos."""
    for i in range(len(inner) - 1):
        a, b = inner[i], inner[i + 1]
        c, d = outer[i + 1], outer[i]
        mesh.quad((a[0], a[1], z), (b[0], b[1], z), (c[0], c[1], z), (d[0], d[1], z),
                  uv_inner(a, i), uv_inner(b, i + 1), uv_outer(c, i + 1), uv_outer(d, i))


def soffit(mesh, outline, z0, z1, region, scale):
    """Faces internas da abertura (intradorso e ombreiras) entre duas profundidades."""
    for i in range(len(outline) - 1):
        a, b = outline[i], outline[i + 1]
        mesh.quad((a[0], a[1], z0), (b[0], b[1], z0), (b[0], b[1], z1), (a[0], a[1], z1),
                  planar(region, a[2] * scale, 0), planar(region, b[2] * scale, 0),
                  planar(region, b[2] * scale, abs(z1 - z0)), planar(region, a[2] * scale, abs(z1 - z0)))


def stone_uv(x, y):
    return planar(STONE, x + WALL_HALF, WALL_TOP - y)


def build_wall():
    m = Mesh("Wall")
    corner_angles = (math.atan2(WALL_TOP - ARCH_CY, WALL_HALF),
                     math.atan2(WALL_TOP - ARCH_CY, -WALL_HALF))
    o0 = arch_outline(ARCH_R0, ARCH_CY, corner_angles)
    total0 = o0[-1][2]
    o0v = [(*offset_point(x, y, ARCH_CY, VOUSSOIR_W), s) for (x, y, s) in o0]
    rect = [(*project_to_rect(x, y, ARCH_CY, WALL_HALF, WALL_TOP), s) for (x, y, s) in o0]

    # Frente: faixa de aduelas em volta da abertura e pedra até a borda da parede.
    n_vouss = 12
    ring(m, o0, o0v, 0.0,
         lambda p, i: region_uv(VOUSSOIR, p[2] / total0 * n_vouss * 85.0, VOUSSOIR[3] - 2),
         lambda p, i: region_uv(VOUSSOIR, p[2] / total0 * n_vouss * 85.0, VOUSSOIR[1] + 2))
    ring(m, o0v, rect, 0.0, lambda p, i: stone_uv(p[0], p[1]), lambda p, i: stone_uv(p[0], p[1]))

    # Intradorso do arco externo até o recuo, anel do arco interno e passagem até o fundo.
    soffit(m, o0, 0.0, RECESS_Z, STONE, PX_PER_UNIT * 0.45)
    o1 = arch_outline(ARCH_R1, ARCH_CY)
    o1o = [(*offset_point(x, y, ARCH_CY, ARCH_R0 - ARCH_R1), s) for (x, y, s) in o1]
    total1 = o1[-1][2]
    n_small = 20
    ring(m, o1, o1o, RECESS_Z,
         lambda p, i: region_uv(SMALL_ARCH, p[2] / total1 * n_small * 51.0, SMALL_ARCH[3] - 2),
         lambda p, i: region_uv(SMALL_ARCH, p[2] / total1 * n_small * 51.0, SMALL_ARCH[1] + 2))
    # Passagem atrás do arco recuado: retangular (ombreiras retas e teto plano na altura do
    # ápice), senão o topo arqueado da porta atravessaria o intradorso ao girar para dentro.
    # Os cantos entre o arco e o retângulo são tampados logo atrás do anel.
    pass_h = ARCH_CY + ARCH_R1
    corners = [(*project_to_rect(x, y, ARCH_CY, ARCH_R1, pass_h), s) for (x, y, s) in o1]
    ring(m, o1, corners, RECESS_Z - 0.01, lambda p, i: stone_uv(p[0], p[1]),
         lambda p, i: stone_uv(p[0], p[1]))
    z0, z1 = RECESS_Z, -WALL_DEPTH
    depth = z0 - z1
    for x in (-ARCH_R1, ARCH_R1):
        m.quad((x, pass_h, z0), (x, pass_h, z1), (x, 0, z1), (x, 0, z0),
               planar(STONE, 0, 0), planar(STONE, depth, 0),
               planar(STONE, depth, pass_h), planar(STONE, 0, pass_h))
    m.quad((-ARCH_R1, pass_h, z0), (ARCH_R1, pass_h, z0), (ARCH_R1, pass_h, z1),
           (-ARCH_R1, pass_h, z1), planar(STONE, 0, 0), planar(STONE, 2 * ARCH_R1, 0),
           planar(STONE, 2 * ARCH_R1, depth), planar(STONE, 0, depth))

    # Fundo da parede (em volta da passagem retangular), lados e topo.
    for (x0, x1, y0, y1) in ((-WALL_HALF, -ARCH_R1, 0, WALL_TOP), (ARCH_R1, WALL_HALF, 0, WALL_TOP),
                             (-ARCH_R1, ARCH_R1, pass_h, WALL_TOP)):
        m.quad((x0, y1, z1), (x1, y1, z1), (x1, y0, z1), (x0, y0, z1),
               stone_uv(x0, y1), stone_uv(x1, y1), stone_uv(x1, y0), stone_uv(x0, y0))
    for x in (-WALL_HALF, WALL_HALF):
        m.quad((x, WALL_TOP, 0), (x, WALL_TOP, -WALL_DEPTH), (x, 0, -WALL_DEPTH), (x, 0, 0),
               planar(STONE, 0, 0), planar(STONE, WALL_DEPTH, 0),
               planar(STONE, WALL_DEPTH, WALL_TOP), planar(STONE, 0, WALL_TOP))
    m.quad((-WALL_HALF, WALL_TOP, 0), (WALL_HALF, WALL_TOP, 0),
           (WALL_HALF, WALL_TOP, -WALL_DEPTH), (-WALL_HALF, WALL_TOP, -WALL_DEPTH),
           planar(STONE, 0, 0), planar(STONE, 2 * WALL_HALF, 0),
           planar(STONE, 2 * WALL_HALF, WALL_DEPTH), planar(STONE, 0, WALL_DEPTH))
    return m


def door_half_width(y):
    if y <= ARCH_CY:
        return DOOR_R
    dy = y - ARCH_CY
    return math.sqrt(max(DOOR_R * DOOR_R - dy * dy, 0.0))


def build_door():
    m = Mesh("Door")
    outline = arch_outline(DOOR_R, ARCH_CY)
    apex = ARCH_CY + DOOR_R
    z_front, z_back = DOOR_Z, DOOR_Z - DOOR_T

    def wood_uv(x, y):
        return planar(WOOD, (x + DOOR_R) / (2 * DOOR_R) * 4.0, (apex - y) / apex * 4.0)

    # Folha: frente e verso em leque (contorno convexo) e borda extrudada.
    pivot = outline[0]
    for z in (z_front, z_back):
        for i in range(1, len(outline) - 1):
            a, b = outline[i], outline[i + 1]
            m.tri((pivot[0], pivot[1], z), (a[0], a[1], z), (b[0], b[1], z),
                  wood_uv(pivot[0], pivot[1]), wood_uv(a[0], a[1]), wood_uv(b[0], b[1]))
    soffit(m, outline, z_front, z_back, METAL_PLAIN, PX_PER_UNIT)
    # Base da porta
    m.quad((-DOOR_R, 0, z_front), (DOOR_R, 0, z_front), (DOOR_R, 0, z_back), (-DOOR_R, 0, z_back),
           planar(METAL_PLAIN, 0, 0), planar(METAL_PLAIN, 2 * DOOR_R, 0),
           planar(METAL_PLAIN, 2 * DOOR_R, DOOR_T), planar(METAL_PLAIN, 0, DOOR_T))

    # Bandas de ferro rebitadas (rebite a cada 0,5 unidade: 64 px / 128 px por unidade).
    band_h, band_t = 0.14, 0.035
    for y in (0.38, 1.18, 1.98, 2.72):
        hw = door_half_width(y + band_h) - 0.05
        box(m, -hw, hw, y, y + band_h, z_front, z_front + band_t, METAL_RIVET, "strip")

    # Guarnição de ferro acompanhando a borda da porta.
    trim_w, trim_t = 0.07, 0.02
    inner = [(*offset_point(x, y, ARCH_CY, -trim_w), s) for (x, y, s) in outline]
    ring(m, inner, outline, z_front + trim_t,
         lambda p, i: planar(METAL_PLAIN, p[2] * 0.3, 0.05),
         lambda p, i: planar(METAL_PLAIN, p[2] * 0.3, 0.05 + trim_w))

    # Dobradiças: três tiras à esquerda sobre as tábuas, com o nó na ombreira.
    hinge_h, hinge_len, hinge_t = 0.11, 0.72, 0.03
    for y in (0.62, 1.52, 2.42):
        box(m, -DOOR_R + 0.02, -DOOR_R + 0.02 + hinge_len, y, y + hinge_h,
            z_front + band_t, z_front + band_t + hinge_t, METAL_RIVET, "strip")
        box(m, -DOOR_R - 0.015, -DOOR_R + 0.05, y - 0.03, y + hinge_h + 0.03,
            z_front - 0.03, z_front + band_t + hinge_t + 0.01, METAL_PLAIN)

    # Argola: placa quadrada e anel (caixa fina) à direita, na altura da mão.
    hx = DOOR_R - 0.32
    box(m, hx - 0.07, hx + 0.07, 1.45, 1.59, z_front, z_front + 0.03, METAL_PLAIN)
    box(m, hx - 0.03, hx + 0.03, 1.22, 1.52, z_front + 0.03, z_front + 0.08, METAL_PLAIN)
    box(m, hx - 0.09, hx + 0.09, 1.22, 1.28, z_front + 0.03, z_front + 0.08, METAL_PLAIN)
    return m


# ----------------------------------------------------------------------------- GLB

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
        "asset": {"version": "2.0", "generator": "PONTIFICE tools/make_door.py"},
        "scene": 0,
        "scenes": [{"nodes": list(range(len(nodes)))}],
        "nodes": nodes,
        "meshes": gl_meshes,
        "materials": [{"name": "DoorAtlas", "doubleSided": True,
                       "pbrMetallicRoughness": {"baseColorTexture": {"index": 0},
                                                "metallicFactor": 0.1, "roughnessFactor": 0.9}}],
        "textures": [{"source": 0, "sampler": 0}],
        "samplers": [{"magFilter": 9729, "minFilter": 9987, "wrapS": 33071, "wrapT": 33071}],
        "images": [{"bufferView": v_img, "mimeType": "image/png", "name": "DoorAtlas"}],
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
    wall, door = build_wall(), build_door()
    print("parede: %d triangulos; porta: %d triangulos" % (len(wall.idx) // 3, len(door.idx) // 3))
    print("pintando atlas %dx%d" % (ATLAS, ATLAS))
    atlas = build_atlas()
    png_bytes = write_png(OUT_ATLAS, atlas.w, atlas.h, atlas.px)
    print("atlas salvo em", OUT_ATLAS, len(png_bytes), "bytes")
    write_glb(OUT_GLB, [wall, door], png_bytes)
    print("gravado", OUT_GLB, os.path.getsize(OUT_GLB), "bytes")


if __name__ == "__main__":
    main()
