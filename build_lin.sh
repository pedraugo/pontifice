#!/usr/bin/env bash
# Builds on Linux: ./build_lin.sh [debug|release] [package]
set -euo pipefail
cd "$(dirname "$0")"

if [[ "$(uname -s)" != Linux ]]; then
  echo "build_lin.sh only runs on Linux" >&2
  exit 1
fi
preset="linux-$(printf '%s' "${1:-debug}" | tr '[:upper:]' '[:lower:]')"

cmake --preset "$preset"
cmake --build --preset "$preset"
if [[ "${2:-}" == "package" ]]; then
  cpack --preset "$preset"
fi
