#!/usr/bin/env bash
# Builds an ONNX Runtime tree from the PyPI wheel instead of GitHub releases.
# Usage: fetch-onnxruntime.sh <version> <pip-platform> <root-dir> <headers-dir>
# Headers come from the repo (thirdparty/onnxruntime/include), because wheels don't ship them.
set -euo pipefail

VERSION="$1"
PLATFORM="$2"
ROOT="$3"
HEADERS="$4"
PY="${PYTHON:-python3}"

# Windows Python needs Windows paths; cygpath only exists under MSYS2.
native() { if command -v cygpath >/dev/null 2>&1; then cygpath -w "$1"; else printf '%s' "$1"; fi; }

PYVER=$("$PY" -c 'import sys; print(f"{sys.version_info[0]}{sys.version_info[1]}")')
TMP=$(mktemp -d)

# --only-binary: never build from a source distribution.
"$PY" -m pip download "onnxruntime==${VERSION}" --no-deps --only-binary=:all: \
  --platform "$PLATFORM" --python-version "$PYVER" -d "$(native "$TMP")"

WHL=$(find "$TMP" -maxdepth 1 -name 'onnxruntime-*.whl' | head -n1)
test -n "$WHL"
"$PY" -m zipfile -e "$(native "$WHL")" "$(native "$TMP/x")"

CAPI="$TMP/x/onnxruntime/capi"
rm -rf "$ROOT" && mkdir -p "$ROOT/lib" "$ROOT/include"
find "$CAPI" -maxdepth 1 \( -name 'libonnxruntime*' -o -name 'onnxruntime*.dll' \) -exec cp {} "$ROOT/lib/" \;
cp -r "$HEADERS"/. "$ROOT/include/"

# Linux wheels ship only the versioned .so; add the links the build expects.
SO=$(find "$ROOT/lib" -maxdepth 1 -name 'libonnxruntime.so.*' | head -n1 || true)
if [ -n "$SO" ]; then
  BASE=$(basename "$SO")
  ln -sfn "$BASE" "$ROOT/lib/libonnxruntime.so.1"
  ln -sfn "$BASE" "$ROOT/lib/libonnxruntime.so"
fi

test -n "$(find "$ROOT/include" -name onnxruntime_cxx_api.h | head -n1)"
test -n "$(find "$ROOT/lib" -maxdepth 1 \( -name 'libonnxruntime*' -o -name 'onnxruntime*.dll' \) | head -n1)"
rm -rf "$TMP"

echo "ORT_ROOT=$ROOT"
if [ -n "${GITHUB_ENV:-}" ]; then echo "ORT_ROOT=$ROOT" >> "$GITHUB_ENV"; fi
