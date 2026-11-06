#!/usr/bin/env python3
"""Converts the MP3s in a directory to Ogg Vorbis, in place, and deletes each converted MP3.

Each track is decoded to 24-bit WAV with macOS `afconvert` (or `ffmpeg`, if available)
and encoded with `oggenc` (brew install vorbis-tools) at VBR quality -q 3 (~110 kbps,
stereo). The MP3 is only deleted after the .ogg is written successfully; if the conversion
fails, the MP3 stays and the script moves on to the next one.

Usage: python3 tools/mp3ToOgg.py src/audios/music [-q 3] [--no-recursive] [--keep]
"""

import argparse
import pathlib
import shutil
import subprocess
import sys
import tempfile


def decode_to_wav(mp3: pathlib.Path, wav: pathlib.Path) -> None:
    if shutil.which("afconvert"):
        cmd = ["afconvert", "-f", "WAVE", "-d", "LEI24", str(mp3), str(wav)]
    elif shutil.which("ffmpeg"):
        cmd = ["ffmpeg", "-loglevel", "error", "-y", "-i", str(mp3), "-c:a", "pcm_s24le", str(wav)]
    else:
        sys.exit("afconvert and ffmpeg not found: install one of them (brew install ffmpeg)")
    subprocess.run(cmd, check=True, capture_output=True)


def encode_to_ogg(wav: pathlib.Path, ogg: pathlib.Path, quality: float) -> None:
    if shutil.which("oggenc") is None:
        sys.exit("oggenc not found: brew install vorbis-tools")
    subprocess.run(["oggenc", "-Q", "-q", str(quality), str(wav), "-o", str(ogg)],
                   check=True, capture_output=True)


def convert(mp3: pathlib.Path, quality: float, keep: bool, tmp_dir: pathlib.Path) -> bool:
    ogg = mp3.with_suffix(".ogg")
    wav = tmp_dir / (mp3.stem + ".wav")
    try:
        decode_to_wav(mp3, wav)
        encode_to_ogg(wav, ogg, quality)
    except subprocess.CalledProcessError as error:
        detail = error.stderr.decode(errors="replace").strip() if error.stderr else ""
        print(f"FAILED {mp3}: {detail or error}", file=sys.stderr)
        ogg.unlink(missing_ok=True)
        return False
    finally:
        wav.unlink(missing_ok=True)
    before, after = mp3.stat().st_size, ogg.stat().st_size
    if not keep:
        mp3.unlink()
    print(f"{mp3.name}: {before / 1048576:.2f} MB -> {ogg.name}: {after / 1048576:.2f} MB")
    return True


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("directory", type=pathlib.Path, help="directory with the .mp3 files")
    parser.add_argument("-q", "--quality", type=float, default=3.0,
                        help="oggenc VBR quality, -1 to 10 (default 3, ~110 kbps)")
    parser.add_argument("--no-recursive", action="store_true", help="do not descend into subdirectories")
    parser.add_argument("--keep", action="store_true", help="do not delete the converted MP3s")
    args = parser.parse_args()

    if not args.directory.is_dir():
        sys.exit(f"{args.directory} is not a directory")
    pattern = "*.mp3" if args.no_recursive else "**/*.mp3"
    mp3s = sorted(p for p in args.directory.glob(pattern) if p.is_file())
    if not mp3s:
        sys.exit(f"no .mp3 in {args.directory}")

    total_before = sum(p.stat().st_size for p in mp3s)
    done = 0
    total_after = 0
    with tempfile.TemporaryDirectory() as tmp:
        tmp_dir = pathlib.Path(tmp)
        for mp3 in mp3s:
            if convert(mp3, args.quality, args.keep, tmp_dir):
                done += 1
                total_after += mp3.with_suffix(".ogg").stat().st_size
    print(f"{done}/{len(mp3s)} converted: {total_before / 1048576:.1f} MB -> "
          f"{total_after / 1048576:.1f} MB")
    if done != len(mp3s):
        sys.exit(1)


if __name__ == "__main__":
    main()
