#!/usr/bin/env python3
"""Gera um normal map a partir da textura de cor embutida num GLB e o grava no mesmo GLB
como `normalTexture` do material (o jogo lê em Model::Parse e aplica em Gpu::Draw).

Altura = luminância da cor (pedra, madeira e metal pintados ficam com relevo plausível),
suavizada com caixa 3x3; normal por Sobel, convenção OpenGL (verde = para cima na imagem),
a mesma dos normal maps das cartas. Atlas maiores que MAX_SIZE são reduzidos (o relevo não
precisa de mais resolução que isso). Só biblioteca padrão.

Uso: python3 tools/normalmap.py src/models/Arena.glb [--strength 2.0]
Também é chamado no fim de texture_arena.py, make_door.py e make_torch.py.
"""

import json
import os
import struct
import sys
import zlib

MAX_SIZE = 1024


# ----------------------------------------------------------------------------- PNG

def read_png_bytes(data):
    """PNG 8 bits (cinza, cinza+alfa, RGB, RGBA), sem interlace. Devolve (w, h, bytes RGB)."""
    assert data[:8] == b"\x89PNG\r\n\x1a\n", "nao e PNG"
    pos = 8
    idat = []
    width = height = 0
    color_type = 0
    while pos < len(data):
        length, ctype = struct.unpack_from(">I4s", data, pos)
        chunk = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if ctype == b"IHDR":
            width, height, bit_depth, color_type = struct.unpack_from(">IIBB", chunk, 0)
            assert bit_depth == 8 and chunk[12] == 0, "PNG precisa ser 8 bits sem interlace"
        elif ctype == b"IDAT":
            idat.append(chunk)
        elif ctype == b"IEND":
            break
    channels = {0: 1, 2: 3, 4: 2, 6: 4}[color_type]
    raw = zlib.decompress(b"".join(idat))
    stride = width * channels
    prev = bytearray(stride)
    src = 0
    rows = []
    for _ in range(height):
        f = raw[src]
        src += 1
        line = bytearray(raw[src:src + stride])
        src += stride
        if f == 1:
            for i in range(channels, stride):
                line[i] = (line[i] + line[i - channels]) & 255
        elif f == 2:
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 255
        elif f == 3:
            for i in range(stride):
                left = line[i - channels] if i >= channels else 0
                line[i] = (line[i] + ((left + prev[i]) >> 1)) & 255
        elif f == 4:
            for i in range(stride):
                a = line[i - channels] if i >= channels else 0
                b = prev[i]
                c = prev[i - channels] if i >= channels else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pred = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[i] = (line[i] + pred) & 255
        rows.append(line)
        prev = line
    rgb = bytearray(width * height * 3)
    o = 0
    for line in rows:
        if channels == 3:
            rgb[o:o + stride] = line
            o += stride
        else:
            for x in range(width):
                i = x * channels
                if channels >= 3:
                    rgb[o:o + 3] = line[i:i + 3]
                else:
                    rgb[o] = rgb[o + 1] = rgb[o + 2] = line[i]
                o += 3
    return width, height, rgb


def write_png_bytes(width, height, rgb):
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
    return png


# ----------------------------------------------------------------------------- normal map

def luminance_rows(width, height, rgb, factor):
    """Altura 0..255 por pixel, já reduzida por `factor` (média de blocos)."""
    w, h = width // factor, height // factor
    rows = []
    for y in range(h):
        row = [0] * w
        for x in range(w):
            acc = 0
            for dy in range(factor):
                base = ((y * factor + dy) * width + x * factor) * 3
                for dx in range(factor):
                    i = base + dx * 3
                    acc += 77 * rgb[i] + 150 * rgb[i + 1] + 29 * rgb[i + 2]
            row[x] = acc // (256 * factor * factor)
        rows.append(row)
    return w, h, rows


def blur3(w, h, rows):
    out = []
    for y in range(h):
        y0, y1 = max(y - 1, 0), min(y + 1, h - 1)
        a, b, c = rows[y0], rows[y], rows[y1]
        row = [0] * w
        for x in range(w):
            x0, x1 = max(x - 1, 0), min(x + 1, w - 1)
            row[x] = (a[x0] + a[x] + a[x1] + b[x0] + b[x] + b[x1] + c[x0] + c[x] + c[x1]) // 9
        out.append(row)
    return out


def normal_map(w, h, rows, strength):
    """Sobel sobre a altura; normal em espaço tangente, OpenGL (verde = cima na imagem)."""
    out = bytearray(w * h * 3)
    o = 0
    k = strength / 255.0
    for y in range(h):
        y0, y1 = max(y - 1, 0), min(y + 1, h - 1)
        a, b, c = rows[y0], rows[y], rows[y1]
        for x in range(w):
            x0, x1 = max(x - 1, 0), min(x + 1, w - 1)
            dx = (a[x1] - a[x0]) + 2 * (b[x1] - b[x0]) + (c[x1] - c[x0])
            dy = (c[x0] - a[x0]) + 2 * (c[x] - a[x]) + (c[x1] - a[x1])
            nx, ny, nz = -dx * k, dy * k, 1.0
            inv = 1.0 / (nx * nx + ny * ny + 1.0) ** 0.5
            out[o] = int((nx * inv * 0.5 + 0.5) * 255)
            out[o + 1] = int((ny * inv * 0.5 + 0.5) * 255)
            out[o + 2] = int((nz * inv * 0.5 + 0.5) * 255)
            o += 3
    return out


def make_normal_png(color_png_bytes, strength):
    width, height, rgb = read_png_bytes(color_png_bytes)
    factor = 1
    while max(width, height) // factor > MAX_SIZE:
        factor *= 2
    w, h, rows = luminance_rows(width, height, rgb, factor)
    rows = blur3(w, h, rows)
    return w, h, write_png_bytes(w, h, normal_map(w, h, rows, strength))


# ----------------------------------------------------------------------------- GLB

def pad4(b, fill=b"\x00"):
    return b + fill * ((4 - len(b) % 4) % 4)


def read_glb(path):
    data = open(path, "rb").read()
    magic, version, _ = struct.unpack_from("<4sII", data, 0)
    assert magic == b"glTF" and version == 2
    json_len, _ = struct.unpack_from("<I4s", data, 12)
    gltf = json.loads(data[20:20 + json_len])
    bin_off = 20 + json_len
    bin_len, bin_type = struct.unpack_from("<I4s", data, bin_off)
    assert bin_type == b"BIN\x00"
    return gltf, bytearray(data[bin_off + 8:bin_off + 8 + bin_len])


def write_glb(path, gltf, blob):
    json_bytes = pad4(json.dumps(gltf, separators=(",", ":")).encode("utf-8"), b" ")
    bin_bytes = bytes(pad4(blob))
    total = 12 + 8 + len(json_bytes) + 8 + len(bin_bytes)
    with open(path, "wb") as f:
        f.write(struct.pack("<4sII", b"glTF", 2, total))
        f.write(struct.pack("<I4s", len(json_bytes), b"JSON"))
        f.write(json_bytes)
        f.write(struct.pack("<I4s", len(bin_bytes), b"BIN\x00"))
        f.write(bin_bytes)


def strip_previous(gltf, blob):
    """Remove normal maps gerados antes (imagens com nome terminado em "Normal", suas texturas
    e bufferViews) e compacta o BIN, para a regeneração não acumular lixo no arquivo."""
    for material in gltf.get("materials", []):
        material.pop("normalTexture", None)
    images = gltf.get("images", [])
    keep_img = [i for i, im in enumerate(images) if not (im.get("name") or "").endswith("Normal")]
    img_map = {old: new for new, old in enumerate(keep_img)}
    textures = gltf.get("textures", [])
    keep_tex = [i for i, t in enumerate(textures) if t.get("source") in img_map]
    tex_map = {old: new for new, old in enumerate(keep_tex)}
    gltf["images"] = [dict(images[i], source=None) and images[i] for i in keep_img]
    gltf["textures"] = [dict(textures[i], source=img_map[textures[i]["source"]]) for i in keep_tex]
    for material in gltf.get("materials", []):
        pbr = material.get("pbrMetallicRoughness", {})
        base = pbr.get("baseColorTexture")
        if base is not None and base["index"] in tex_map:
            base["index"] = tex_map[base["index"]]
    # bufferViews referenciados por acessores e imagens restantes.
    used = []
    for acc in gltf.get("accessors", []):
        if acc["bufferView"] not in used:
            used.append(acc["bufferView"])
    for im in gltf["images"]:
        if im["bufferView"] not in used:
            used.append(im["bufferView"])
    views = gltf["bufferViews"]
    new_blob = bytearray()
    new_views = []
    view_map = {}
    for old in used:
        v = views[old]
        off = v.get("byteOffset", 0)
        start = len(new_blob)
        new_blob.extend(pad4(bytes(blob[off:off + v["byteLength"]])))
        nv = dict(v)
        nv["byteOffset"] = start
        new_views.append(nv)
        view_map[old] = len(new_views) - 1
    for acc in gltf.get("accessors", []):
        acc["bufferView"] = view_map[acc["bufferView"]]
    for im in gltf["images"]:
        im["bufferView"] = view_map[im["bufferView"]]
    gltf["bufferViews"] = new_views
    return new_blob


def add_to_glb(path, strength=2.0, out_png=None):
    gltf, blob = read_glb(path)
    blob = strip_previous(gltf, blob)
    done = {}
    for material in gltf.get("materials", []):
        pbr = material.get("pbrMetallicRoughness", {})
        base = pbr.get("baseColorTexture")
        if base is None:
            continue
        tex_index = base["index"]
        if tex_index not in done:
            texture = gltf["textures"][tex_index]
            image = gltf["images"][texture["source"]]
            view = gltf["bufferViews"][image["bufferView"]]
            off = view.get("byteOffset", 0)
            color_png = bytes(blob[off:off + view["byteLength"]])
            w, h, normal_png = make_normal_png(color_png, strength)
            if out_png:
                open(out_png, "wb").write(normal_png)
            if len(blob) % 4:
                blob = bytearray(pad4(bytes(blob)))
            start = len(blob)
            blob.extend(pad4(normal_png))
            gltf["bufferViews"].append({"buffer": 0, "byteOffset": start,
                                        "byteLength": len(normal_png)})
            gltf.setdefault("images", []).append({"bufferView": len(gltf["bufferViews"]) - 1,
                                                  "mimeType": "image/png",
                                                  "name": (image.get("name") or "atlas") + "Normal"})
            gltf["textures"].append({"source": len(gltf["images"]) - 1,
                                     "sampler": texture.get("sampler", 0)})
            done[tex_index] = len(gltf["textures"]) - 1
            print("normal map %dx%d gerado para %s" % (w, h, image.get("name") or path))
        material["normalTexture"] = {"index": done[tex_index]}
    gltf["buffers"][0]["byteLength"] = len(pad4(bytes(blob)))
    write_glb(path, gltf, blob)
    print("gravado", path, os.path.getsize(path), "bytes")


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 1
    strength = 2.0
    paths = []
    i = 1
    while i < len(argv):
        if argv[i] == "--strength":
            strength = float(argv[i + 1])
            i += 2
        else:
            paths.append(argv[i])
            i += 1
    for path in paths:
        add_to_glb(path, strength)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
