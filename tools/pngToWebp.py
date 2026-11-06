#!/usr/bin/env python3
"""Converts the PNGs in a directory to lossless WebP, in place, and deletes each converted PNG.

Uses `cwebp` (brew install webp) in lossless mode with `-exact` (preserves the RGB of transparent
pixels, important for normal maps) and maximum compression effort (`-z 9`): the decoded pixels
come out identical to the PNG. Conversions run in parallel. The PNG is only deleted after the
.webp is written successfully; if the conversion fails, the PNG stays.

Usage: python3 tools/pngToWebp.py src/sprites [--no-recursive] [--keep] [-j 8]
"""

import argparse
import concurrent.futures
import os
import pathlib
import shutil
import subprocess
import sys


def convert(png: pathlib.Path, keep: bool) -> tuple[pathlib.Path, bool, str]:
    webp = png.with_suffix(".webp")
    result = subprocess.run(
        ["cwebp", "-quiet", "-lossless", "-exact", "-z", "9", str(png), "-o", str(webp)],
        capture_output=True,
    )
    if result.returncode != 0 or not webp.is_file() or webp.stat().st_size == 0:
        webp.unlink(missing_ok=True)
        detail = result.stderr.decode(errors="replace").strip()
        return png, False, detail or f"cwebp exited with {result.returncode}"
    before, after = png.stat().st_size, webp.stat().st_size
    if not keep:
        png.unlink()
    return png, True, f"{before / 1048576:.2f} MB -> {webp.name}: {after / 1048576:.2f} MB"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("directory", type=pathlib.Path, help="directory with the .png files")
    parser.add_argument("--no-recursive", action="store_true", help="do not descend into subdirectories")
    parser.add_argument("--keep", action="store_true", help="do not delete the converted PNGs")
    parser.add_argument("-j", "--jobs", type=int, default=os.cpu_count() or 1,
                        help="parallel conversions (default: number of CPUs)")
    args = parser.parse_args()

    if shutil.which("cwebp") is None:
        sys.exit("cwebp not found: brew install webp")
    if not args.directory.is_dir():
        sys.exit(f"{args.directory} is not a directory")
    pattern = "*.png" if args.no_recursive else "**/*.png"
    pngs = sorted(p for p in args.directory.glob(pattern) if p.is_file())
    if not pngs:
        sys.exit(f"no .png in {args.directory}")

    total_before = sum(p.stat().st_size for p in pngs)
    done = 0
    total_after = 0
    with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, args.jobs)) as pool:
        for png, ok, detail in pool.map(lambda p: convert(p, args.keep), pngs):
            if ok:
                done += 1
                total_after += png.with_suffix(".webp").stat().st_size
                print(f"{png.name}: {detail}")
            else:
                print(f"FAILED {png}: {detail}", file=sys.stderr)
    print(f"{done}/{len(pngs)} converted: {total_before / 1048576:.1f} MB -> "
          f"{total_after / 1048576:.1f} MB")
    if done != len(pngs):
        sys.exit(1)


if __name__ == "__main__":
    main()
