#!/usr/bin/env bash
# Compila e executa o jogo.
set -euo pipefail
cd "$(dirname "$0")"

./build.sh "${1:-Debug}"
exec ./build/pontifice
