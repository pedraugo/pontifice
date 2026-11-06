#!/usr/bin/env python3
"""Generates src/icons/icon.ico (Windows) from src/assets/icon.png.

The .ico embeds PNGs at sizes 16 to 256 (format accepted since Windows Vista). Resizing
uses macOS `sips`; on other systems, generate the PNGs externally and point to them with --sizes-dir.

Usage: python3 tools/makeIcons.py
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
        sys.exit("sips not found: generate the resized PNGs and use --sizes-dir")
    subprocess.run(
        ["sips", "-s", "format", "png", "-z", str(size), str(size), str(src), "--out", str(out)],
        check=True,
        capture_output=True,
    )
    return out


def write_ico(pngs: list[tuple[int, bytes]], out: pathlib.Path) -> None:
    header = struct.pack("<HHH", 0, 1, len(pngs))
    offset = len(header) + 16 * len(pngs)
    entries = b""
    data = b""
    for size, png in pngs:
        dim = 0 if size >= 256 else size
        entries += struct.pack("<BBBBHHII", dim, dim, 0, 0, 1, 32, len(png), offset)
        data += png
        offset += len(png)
    out.write_bytes(header + entries + data)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--sizes-dir", type=pathlib.Path,
                        help="directory with already resized icon_<size>.png files")
    args = parser.parse_args()

    with tempfile.TemporaryDirectory() as tmp:
        tmp_dir = pathlib.Path(tmp)
        pngs = []
        for size in SIZES:
            path = (args.sizes_dir / f"icon_{size}.png") if args.sizes_dir else resize(SOURCE, size, tmp_dir)
            pngs.append((size, path.read_bytes()))
        ICO.parent.mkdir(parents=True, exist_ok=True)
        write_ico(pngs, ICO)
    print(f"{ICO.relative_to(ROOT)}: {ICO.stat().st_size} bytes, sizes {SIZES}")


if __name__ == "__main__":
    main()
