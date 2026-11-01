#!/usr/bin/env bash
# Configura (na primeira vez) e compila o projeto em build/.
set -euo pipefail
cd "$(dirname "$0")"

BUILD_TYPE="${1:-Debug}"

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
cmake --build build
