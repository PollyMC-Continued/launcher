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

# Pinned: ONNX Runtime 1.22.0 has wheels for cp310-cp313 only, and runners can have newer Python.
PYVER="${ORT_PYVER:-312}"
TMP=$(mktemp -d)

# The PyPI wheel has no C++ headers. Use a committed copy if there is one; otherwise
# take them from the official Microsoft.ML.OnnxRuntime NuGet package (nuget.org, not GitHub).
if [ ! -f "$HEADERS/onnxruntime_cxx_api.h" ]; then
  echo "No committed headers in $HEADERS; fetching them from NuGet."
  NUPKG="$TMP/ort.nupkg"
  curl -fsSL --retry 3 -o "$NUPKG" \
    "https://api.nuget.org/v3-flatcontainer/microsoft.ml.onnxruntime/${VERSION}/microsoft.ml.onnxruntime.${VERSION}.nupkg"
  HEADERS="$TMP/include"
  mkdir -p "$HEADERS"
  "$PY" - "$(native "$NUPKG")" "$(native "$HEADERS")" <<'PYEOF'
import sys, zipfile
z = zipfile.ZipFile(sys.argv[1])
out = sys.argv[2]
prefix = "build/native/include/"
n = 0
for info in z.infolist():
    if info.filename.startswith(prefix) and not info.is_dir():
        info.filename = info.filename[len(prefix):]
        z.extract(info, out)
        n += 1
print(f"extracted {n} header files")
if n == 0:
    sys.exit("no headers found in the NuGet package")
PYEOF
fi

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

# macOS wheels ship only libonnxruntime.<version>.dylib; CMake's find_library wants libonnxruntime.dylib.
DYLIB=$(find "$ROOT/lib" -maxdepth 1 -type f -name 'libonnxruntime.*.dylib' | head -n1 || true)
if [ -n "$DYLIB" ]; then
  ln -sfn "$(basename "$DYLIB")" "$ROOT/lib/libonnxruntime.dylib"
fi

test -n "$(find "$ROOT/include" -name onnxruntime_cxx_api.h | head -n1)"
test -n "$(find "$ROOT/lib" -maxdepth 1 \( -name 'libonnxruntime*' -o -name 'onnxruntime*.dll' \) | head -n1)"
rm -rf "$TMP"

echo "ORT_ROOT=$ROOT"
if [ -n "${GITHUB_ENV:-}" ]; then echo "ORT_ROOT=$ROOT" >> "$GITHUB_ENV"; fi