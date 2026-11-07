#!/usr/bin/env bash
# Builds on macOS: ./build_mac.sh [debug|release] [package]
set -euo pipefail
cd "$(dirname "$0")"

if [[ "$(uname -s)" != Darwin ]]; then
  echo "build_mac.sh only runs on macOS" >&2
  exit 1
fi
preset="macos-$(printf '%s' "${1:-debug}" | tr '[:upper:]' '[:lower:]')"

cmake --preset "$preset"
cmake --build --preset "$preset"
if [[ "${2:-}" == "package" ]]; then
  cpack --preset "$preset"
fi
