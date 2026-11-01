#!/usr/bin/env python3
"""Gera src/icons/icon.ico (Windows) a partir de src/assets/icon.png.

O .ico embute PNGs nos tamanhos 16 a 256 (formato aceito desde o Windows Vista). O redimensionamento
usa o `sips` do macOS; em outros sistemas, gere os PNGs por fora e aponte com --sizes-dir.

Uso: python3 tools/make_icons.py
"""

import argparse
import pathlib
import shutil
import struct
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
SOURCE = ROOT / "src" / "assets" / "icon.png"
ICO = ROOT / "src" / "icons" / "icon.ico"
SIZES = (256, 128, 64, 48, 32, 16)


def resize(src: pathlib.Path, size: int, out_dir: pathlib.Path) -> pathlib.Path:
    out = out_dir / f"icon_{size}.png"
    if shutil.which("sips") is None:
        sys.exit("sips nao encontrado: gere os PNGs redimensionados e use --sizes-dir")
    subprocess.run(
        ["sips", "-s", "format", "png", "-z", str(size), str(size), str(src), "--out", str(out)],
        check=True,
        capture_output=True,
    )
    return out


def write_ico(pngs: list[tuple[int, bytes]], out: pathlib.Path) -> None:
    # Cabecalho ICONDIR + uma ICONDIRENTRY por imagem, depois os dados PNG em sequencia.
    header = struct.pack("<HHH", 0, 1, len(pngs))
    offset = len(header) + 16 * len(pngs)
    entries = b""
    data = b""
    for size, png in pngs:
        dim = 0 if size >= 256 else size  # 0 significa 256
        entries += struct.pack("<BBBBHHII", dim, dim, 0, 0, 1, 32, len(png), offset)
        data += png
        offset += len(png)
    out.write_bytes(header + entries + data)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--sizes-dir", type=pathlib.Path,
                        help="pasta com icon_<tamanho>.png ja redimensionados")
    args = parser.parse_args()

    with tempfile.TemporaryDirectory() as tmp:
        tmp_dir = pathlib.Path(tmp)
        pngs = []
        for size in SIZES:
            path = (args.sizes_dir / f"icon_{size}.png") if args.sizes_dir else resize(SOURCE, size, tmp_dir)
            pngs.append((size, path.read_bytes()))
        ICO.parent.mkdir(parents=True, exist_ok=True)
        write_ico(pngs, ICO)
    print(f"{ICO.relative_to(ROOT)}: {ICO.stat().st_size} bytes, tamanhos {SIZES}")


if __name__ == "__main__":
    main()
