#!/usr/bin/env bash

set -euo pipefail

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
source_root=$(cd "$script_dir/../.." && pwd)
cd "$source_root"

fail() {
    echo "Local macOS build failed: $*" >&2
    exit 1
}

command -v git >/dev/null 2>&1 || fail "Git is required"
command -v brew >/dev/null 2>&1 || fail "Homebrew is required"
command -v cmake >/dev/null 2>&1 || fail "CMake is required (brew install cmake)"
command -v ninja >/dev/null 2>&1 || fail "Ninja is required (brew install ninja)"
xcode-select -p >/dev/null 2>&1 || fail "Xcode Command Line Tools are required (xcode-select --install)"

java_home=$(/usr/libexec/java_home -v 17 2>/dev/null) || fail "JDK 17 is required (brew install --cask temurin@17)"
export JAVA_HOME=$java_home

formulae=(qt libarchive tomlplusplus cmark qrencode extra-cmake-modules)
prefixes=()
for formula in "${formulae[@]}"; do
    prefix=$(brew --prefix "$formula" 2>/dev/null) || fail "Missing Homebrew dependency: $formula"
    prefixes+=("$prefix")
done
if [[ -n ${CMAKE_PREFIX_PATH:-} ]]; then
    prefixes+=("$CMAKE_PREFIX_PATH")
fi
export CMAKE_PREFIX_PATH=$(IFS=';'; echo "${prefixes[*]}")

deployment_target=${MACOSX_DEPLOYMENT_TARGET:-$(sw_vers -productVersion | awk -F. '{print $1 ".0"}')}
architecture=$(uname -m)
build_dir=${BUILD_DIR:-build-macos}
install_dir=${INSTALL_DIR:-install-macos}
output_dir=${OUTPUT_DIR:-dist-macos}

for directory in "$build_dir" "$install_dir" "$output_dir"; do
    case "$directory" in
        ''|/|.|..|/*|../*|*/../*|*/..) fail "Working directories must be relative paths inside the repository: $directory" ;;
    esac
done

git submodule update --init --recursive
cmake -E rm -rf "$build_dir" "$install_dir" "$output_dir"

cmake -S . -B "$build_dir" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="$deployment_target" \
    -DLauncher_BUILD_RELEASE=ON \
    -DLauncher_BUILD_PLATFORM="macOS-$architecture"

cmake --build "$build_dir" --parallel
ctest --test-dir "$build_dir" --output-on-failure

version=$(cmake -N -LA "$build_dir" | sed -n 's/^Launcher_VERSION:STRING=//p')
[[ $version =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || fail "Could not read Launcher_VERSION"

ARTIFACT_ARCH=$architecture \
EXPECTED_ARCHS=$architecture \
EXPECTED_MIN_MACOS=$deployment_target \
    "$script_dir/package.sh" "$build_dir" "$install_dir" "$output_dir" "$version"

standalone_app="$output_dir/PollyMC.app"
cmake -E rm -rf "$standalone_app"
ditto --rsrc --extattr --noqtn --noacl "$install_dir/PollyMC.app" "$standalone_app"

source_branch=$(git symbolic-ref --short -q HEAD 2>/dev/null || true)
EXPECTED_VERSION=$version \
EXPECTED_ARCHS=$architecture \
EXPECTED_MIN_MACOS=$deployment_target \
SOURCE_ROOT=$source_root \
SOURCE_BRANCH=$source_branch \
    "$script_dir/verify_bundle.sh" "$standalone_app"

dmg_path="$output_dir/PollyMC-Continued-${version}-macOS-${architecture}.dmg"
app_icon="$standalone_app/Contents/Resources/PollyMC.icns"
[[ -f $dmg_path ]] || fail "DMG was not created: $dmg_path"
[[ -s $app_icon ]] || fail "Application icon is missing: $app_icon"

rez=$(xcrun --find Rez) || fail "Rez is required from Xcode Command Line Tools"
setfile=$(xcrun --find SetFile) || fail "SetFile is required from Xcode Command Line Tools"
getfileinfo=$(xcrun --find GetFileInfo) || fail "GetFileInfo is required from Xcode Command Line Tools"
icon_resource=$(mktemp "${TMPDIR:-/tmp}/pollymc-dmg-icon.XXXXXX")
trap 'cmake -E rm -f "$icon_resource"' EXIT
escaped_icon=${app_icon//\\/\\\\}
escaped_icon=${escaped_icon//\"/\\\"}
printf "read 'icns' (-16455) \"%s\";\n" "$escaped_icon" > "$icon_resource"
"$rez" -append "$icon_resource" -o "$dmg_path"
"$setfile" -a C "$dmg_path"
touch "$dmg_path"

attributes=$("$getfileinfo" -a "$dmg_path")
[[ $attributes == *C* ]] || fail "The DMG file custom icon flag was not set"
[[ -s "$dmg_path/..namedfork/rsrc" ]] || fail "The DMG file icon resource is missing"

echo
echo "Created local macOS artifacts in $source_root/$output_dir:"
echo "  PollyMC.app"
echo "  $(basename "$dmg_path")"
echo "  PollyMC-Continued-${version}-macOS-${architecture}.zip"
echo "  PollyMC-Continued-${version}-macOS-${architecture}.sha256"
echo
echo "The DMG file icon is a macOS resource fork and is intended for local builds."
