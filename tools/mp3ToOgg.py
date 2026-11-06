#!/usr/bin/env python3
"""Converte os MP3 de uma pasta para Ogg Vorbis, no mesmo lugar, e apaga cada MP3 convertido.

Cada faixa é decodificada para WAV 24 bits com o `afconvert` do macOS (ou `ffmpeg`, se houver)
e codificada com `oggenc` (brew install vorbis-tools) em VBR de qualidade -q 3 (~110 kbps,
estéreo). O MP3 só é apagado depois de o .ogg ser gravado com sucesso; se a conversão falhar,
o MP3 fica e o script segue para o próximo.

Uso: python3 tools/mp3ToOgg.py src/audios/music [-q 3] [--no-recursive] [--keep]
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
        sys.exit("afconvert e ffmpeg nao encontrados: instale um deles (brew install ffmpeg)")
    subprocess.run(cmd, check=True, capture_output=True)


def encode_to_ogg(wav: pathlib.Path, ogg: pathlib.Path, quality: float) -> None:
    if shutil.which("oggenc") is None:
        sys.exit("oggenc nao encontrado: brew install vorbis-tools")
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
        print(f"FALHOU {mp3}: {detail or error}", file=sys.stderr)
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
    parser.add_argument("directory", type=pathlib.Path, help="pasta com os arquivos .mp3")
    parser.add_argument("-q", "--quality", type=float, default=3.0,
                        help="qualidade VBR do oggenc, -1 a 10 (padrao 3, ~110 kbps)")
    parser.add_argument("--no-recursive", action="store_true", help="nao entra nas subpastas")
    parser.add_argument("--keep", action="store_true", help="nao apaga os MP3 convertidos")
    args = parser.parse_args()

    if not args.directory.is_dir():
        sys.exit(f"{args.directory} nao e uma pasta")
    pattern = "*.mp3" if args.no_recursive else "**/*.mp3"
    mp3s = sorted(p for p in args.directory.glob(pattern) if p.is_file())
    if not mp3s:
        sys.exit(f"nenhum .mp3 em {args.directory}")

    total_before = sum(p.stat().st_size for p in mp3s)
    done = 0
    total_after = 0
    with tempfile.TemporaryDirectory() as tmp:
        tmp_dir = pathlib.Path(tmp)
        for mp3 in mp3s:
            if convert(mp3, args.quality, args.keep, tmp_dir):
                done += 1
                total_after += mp3.with_suffix(".ogg").stat().st_size
    print(f"{done}/{len(mp3s)} convertidos: {total_before / 1048576:.1f} MB -> "
          f"{total_after / 1048576:.1f} MB")
    if done != len(mp3s):
        sys.exit(1)


if __name__ == "__main__":
    main()
