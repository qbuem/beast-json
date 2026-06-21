#!/usr/bin/env bash
# Build the qbuem-json WebAssembly module (ES module: qbuem_json.mjs + .wasm).
# Requires the Emscripten SDK (emcc/em++) on PATH — see https://emscripten.org.
#
#   ./build.sh            # → dist/qbuem_json.mjs, dist/qbuem_json.wasm
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo="$(cd "$here/../.." && pwd)"
out="$here/dist"
mkdir -p "$out"

em++ -std=c++20 -O3 \
  -I "$repo/include" \
  "$here/qbuem_wasm.cpp" \
  -o "$out/qbuem_json.mjs" \
  -lembind \
  -sMODULARIZE=1 \
  -sEXPORT_ES6=1 \
  -sEXPORT_NAME=createQbuemJson \
  -sENVIRONMENT=web,node \
  -sALLOW_MEMORY_GROWTH=1 \
  -fexceptions

echo "Built: $out/qbuem_json.mjs  +  $out/qbuem_json.wasm"
