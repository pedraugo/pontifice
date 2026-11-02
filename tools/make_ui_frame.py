#!/usr/bin/env python3
"""Gera src/sprites/UiFrameButton.png, UiFrameWindow.png e UiFrameButtonDanger.png a partir de
src/art/ui.jpg: a placa de pedra com moldura usada como fundo de botões (Button, como na
arte), de janelas, painéis e tooltips (Window: mais escura e cinza, puxada para metal) e de
botões de ação destrutiva, como Sair e confirmar abandono (Danger: mesma moldura de metal e
estrelas, pedra em vermelho escuro). Ver scripts/frame/Frame.*.

1. Converte o JPG para PNG com `sips` (macOS) e lê os pixels (só biblioteca padrão).
2. Fundo branco fora da placa arredondada vira transparente (preenchimento a partir das
   bordas pelos pixels quase brancos, com borda suavizada).
3. Recorta a imagem à caixa da placa (sem a margem transparente).
4. Centro para TILE: a região central (entre os cantos de CORNER px) é refeita em espelho
   2x2 a partir do quadrante superior esquerdo. Com isso os 9 tiles IGUAIS (terços) em que
   o jogo divide a folha repetem sem emenda: o terço do meio é simétrico em torno do centro,
   então a última coluna/linha de um tile é igual à primeira do seguinte.

O jogo (scripts/frame) divide a folha em 3x3 tiles iguais: tl t tr / ml m mr / bl b br, e
monta cada botão ou painel com tl + N vezes t + tr na linha de cima, bl + N vezes b + br na
de baixo e, se alto, linhas ml + N vezes m + mr no meio.

Variante Danger: dentro da borda interna da moldura, cada pixel de pedra (escuro e saturado)
vira vermelho escuro com a mesma luminância relativa, mantendo veios e textura; o metal
(claro e pouco saturado: moldura, filete interno e estrelas) fica como está.

Uso: python3 tools/make_ui_frame.py [--size N]   (N: lado final, múltiplo de 3; padrão 768)
     python3 tools/make_ui_frame.py --danger-only (só a Danger, a partir do UiFrameButton.png)
"""

import os
import struct
import subprocess
import sys
import tempfile
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "src", "art", "ui.jpg")
OUT_BUTTON = os.path.join(ROOT, "src", "sprites", "UiFrameButton.png")
OUT_WINDOW = os.path.join(ROOT, "src", "sprites", "UiFrameWindow.png")
OUT_DANGER = os.path.join(ROOT, "src", "sprites", "UiFrameButtonDanger.png")
# Variante Window: dessatura (mistura com a luminância) e escurece, com leve tom de aço frio.
WINDOW_DESATURATE = 0.75
WINDOW_DARKEN = 0.68
WINDOW_TINT = (0.96, 0.98, 1.04)
# Variante Danger: cor da pedra = luminância vezes DANGER_TINT. Só dentro de DANGER_INSET
# (fração do lado: borda interna da moldura). Peso da troca: pixels escuros (luminância até
# DANGER_LUM_FULL, nada a partir de DANGER_LUM_NONE) e saturados (saturação a partir de
# DANGER_SAT_FULL, nada abaixo de DANGER_SAT_NONE), o que deixa o metal intacto.
DANGER_TINT = (1.55, 0.30, 0.28)
DANGER_INSET = 41.0 / 768.0
DANGER_LUM_FULL, DANGER_LUM_NONE = 70.0, 115.0
DANGER_SAT_NONE, DANGER_SAT_FULL = 0.15, 0.35
CORNER_FRACTION = 130.0 / 888.0  # cantos com as estrelas, em fração do lado da placa
WHITE = 232                      # mínimo de r, g, b para contar como fundo branco

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from normalmap import read_png_bytes  # noqa: E402
from make_torch import write_png_rgba  # noqa: E402


def load_rgb(path):
    with tempfile.TemporaryDirectory() as tmp:
        png = os.path.join(tmp, "ui.png")
        subprocess.run(["sips", "-s", "format", "png", path, "--out", png], check=True,
                       stdout=subprocess.DEVNULL)
        with open(png, "rb") as f:
            return read_png_bytes(f.read())


def remove_background(w, h, rgb):
    """Alpha por pixel: 0 no fundo branco ligado às bordas, suavizado na borda da placa."""
    seen = bytearray(w * h)
    stack = []

    def bright(i):
        return min(rgb[i * 3], rgb[i * 3 + 1], rgb[i * 3 + 2]) >= WHITE

    def push(x, y):
        if 0 <= x < w and 0 <= y < h:
            i = y * w + x
            if not seen[i] and bright(i):
                seen[i] = 1
                stack.append(i)

    for x in range(w):
        push(x, 0)
        push(x, h - 1)
    for y in range(h):
        push(0, y)
        push(w - 1, y)
    while stack:
        i = stack.pop()
        x, y = i % w, i // w
        push(x + 1, y)
        push(x - 1, y)
        push(x, y + 1)
        push(x, y - 1)

    alpha = bytearray([255]) * (w * h)
    for y in range(h):
        for x in range(w):
            i = y * w + x
            if seen[i]:
                alpha[i] = 0
                continue
            edge = (x == 0 or seen[i - 1]) or (x == w - 1 or seen[i + 1]) or \
                   (y == 0 or seen[i - w]) or (y == h - 1 or seen[i + w])
            if edge:
                lum = (rgb[i * 3] + rgb[i * 3 + 1] + rgb[i * 3 + 2]) // 3
                alpha[i] = max(60, min(255, 255 - (lum - 150) * 2))
    return alpha


def crop_to_plate(w, h, rgb, alpha):
    xs = [x for y in range(h) for x in range(w) if alpha[y * w + x] > 40]
    ys = [y for y in range(h) for x in range(w) if alpha[y * w + x] > 40]
    x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)
    cw, ch = x1 - x0 + 1, y1 - y0 + 1
    out = bytearray(cw * ch * 4)
    for y in range(ch):
        for x in range(cw):
            i = (y + y0) * w + (x + x0)
            o = (y * cw + x) * 4
            out[o:o + 3] = rgb[i * 3:i * 3 + 3]
            out[o + 3] = alpha[i]
    return cw, ch, out


def resize_rgba(w, h, rgba, size):
    """Reduz por média de área para size x size (a placa é quadrada)."""
    out = bytearray(size * size * 4)
    for y in range(size):
        sy0, sy1 = y * h // size, max((y + 1) * h // size, y * h // size + 1)
        for x in range(size):
            sx0, sx1 = x * w // size, max((x + 1) * w // size, x * w // size + 1)
            acc = [0, 0, 0, 0]
            n = 0
            for sy in range(sy0, sy1):
                row = sy * w
                for sx in range(sx0, sx1):
                    i = (row + sx) * 4
                    for c in range(4):
                        acc[c] += rgba[i + c]
                    n += 1
            o = (y * size + x) * 4
            for c in range(4):
                out[o + c] = acc[c] // n
    return out


def mirror_center(size, rgba):
    """Centro (entre os cantos) em espelho 2x2 do quadrante superior esquerdo: tile sem emenda."""
    c = int(round(CORNER_FRACTION * size))
    span = size - 2 * c
    half = span // 2
    src = [bytes(rgba[((c + y) * size + c + x) * 4:((c + y) * size + c + x) * 4 + 4])
           for y in range(half) for x in range(half)]
    for y in range(span):
        sy = y if y < half else span - 1 - y
        sy = min(sy, half - 1)
        for x in range(span):
            sx = x if x < half else span - 1 - x
            sx = min(sx, half - 1)
            o = ((c + y) * size + c + x) * 4
            rgba[o:o + 4] = src[sy * half + sx]


def window_variant(rgba):
    out = bytearray(rgba)
    for i in range(0, len(out), 4):
        r, g, b = out[i], out[i + 1], out[i + 2]
        lum = 0.299 * r + 0.587 * g + 0.114 * b
        for c, (v, tint) in enumerate(zip((r, g, b), WINDOW_TINT)):
            mixed = v + (lum - v) * WINDOW_DESATURATE
            out[i + c] = max(0, min(255, int(mixed * WINDOW_DARKEN * tint + 0.5)))
    return out


def smoothstep(e0, e1, x):
    t = max(0.0, min(1.0, (x - e0) / (e1 - e0)))
    return t * t * (3.0 - 2.0 * t)


def danger_variant(size, rgba):
    out = bytearray(rgba)
    inset = DANGER_INSET * size
    for y in range(size):
        for x in range(size):
            # Fora da borda interna (moldura de metal): intacto; 1 px de transição.
            edge = min(x - inset, y - inset, size - 1 - inset - x, size - 1 - inset - y)
            region = max(0.0, min(1.0, edge + 1.0))
            if region <= 0.0:
                continue
            i = (y * size + x) * 4
            r, g, b = out[i], out[i + 1], out[i + 2]
            lum = 0.299 * r + 0.587 * g + 0.114 * b
            top = max(r, g, b)
            sat = (top - min(r, g, b)) / top if top > 0 else 0.0
            weight = region * (1.0 - smoothstep(DANGER_LUM_FULL, DANGER_LUM_NONE, lum)) * \
                smoothstep(DANGER_SAT_NONE, DANGER_SAT_FULL, sat)
            if weight <= 0.0:
                continue
            for c, tint in enumerate(DANGER_TINT):
                red = min(255.0, lum * tint)
                out[i + c] = max(0, min(255, int(out[i + c] + (red - out[i + c]) * weight + 0.5)))
    return out


def main():
    if "--danger-only" in sys.argv:
        with open(OUT_BUTTON, "rb") as f:
            w, h, rgba = read_png_bytes(f.read(), keep_alpha=True)
        if w != h:
            sys.exit("UiFrameButton.png precisa ser quadrado")
        write_png_rgba(OUT_DANGER, w, h, danger_variant(w, rgba))
        print(f"gravado {OUT_DANGER} ({w}x{h})")
        return
    size = 768
    if "--size" in sys.argv:
        size = int(sys.argv[sys.argv.index("--size") + 1])
    if size % 3 != 0:
        sys.exit("--size precisa ser multiplo de 3 (9 tiles iguais)")
    w, h, rgb = load_rgb(SRC)
    alpha = remove_background(w, h, rgb)
    cw, ch, rgba = crop_to_plate(w, h, rgb, alpha)
    print(f"placa {cw}x{ch} de {w}x{h}")
    rgba = resize_rgba(cw, ch, rgba, size)
    mirror_center(size, rgba)
    write_png_rgba(OUT_BUTTON, size, size, rgba)
    write_png_rgba(OUT_WINDOW, size, size, window_variant(rgba))
    write_png_rgba(OUT_DANGER, size, size, danger_variant(size, rgba))
    print(f"gravados {OUT_BUTTON}, {OUT_WINDOW} e {OUT_DANGER} ({size}x{size}, tiles de "
          f"{size // 3} px)")


if __name__ == "__main__":
    main()
