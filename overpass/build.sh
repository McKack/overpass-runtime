#!/bin/bash
# Builds OverPass Runtime (DXMT main with D3D12, NVAPI and NGX on) on a Mac with Xcode, its Metal toolchain
# (xcodebuild -downloadComponent MetalToolchain), Homebrew's mingw-w64, cmake, ninja and meson.
# Toolchains are built or downloaded once into $TOOLCHAINS. Output: $PREFIX/{x86_64-windows,x86_64-unix}.
set -euo pipefail

SRC="$(cd "$(dirname "$0")/.." && pwd)"
TOOLCHAINS="${TOOLCHAINS:-$SRC/../toolchains}"
BUILD="${BUILD:-$SRC/../build/x64}"
PREFIX="${PREFIX:-$SRC/../install}"
LLVM_VERSION=llvmorg-15.0.7
WINE_URL=https://github.com/3Shain/wine/releases/download/v8.16-3shain/wine.tar.gz  # what DXMT's CI builds against
export DEVELOPER_DIR="${DEVELOPER_DIR:-/Applications/Xcode.app/Contents/Developer}"

mkdir -p "$TOOLCHAINS"
TOOLCHAINS="$(cd "$TOOLCHAINS" && pwd)"

# LLVM 15, x86_64, libraries only (airconv links it). About 4 minutes on an M4.
if [ ! -f "$TOOLCHAINS/llvm-darwin/lib/libLLVMCore.a" ]; then
    [ -d "$TOOLCHAINS/llvm-project" ] ||
        git clone --depth 1 --branch "$LLVM_VERSION" https://github.com/llvm/llvm-project.git "$TOOLCHAINS/llvm-project"
    cmake -B "$TOOLCHAINS/llvm-darwin-build" -S "$TOOLCHAINS/llvm-project/llvm" -G Ninja \
        -DCMAKE_INSTALL_PREFIX="$TOOLCHAINS/llvm-darwin" -DCMAKE_OSX_ARCHITECTURES=x86_64 \
        -DLLVM_HOST_TRIPLE=x86_64-apple-darwin -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_ASSERTIONS=On \
        -DLLVM_ENABLE_ZSTD=Off -DLLVM_TARGETS_TO_BUILD="" -DLLVM_BUILD_TOOLS=Off -DLLVM_INCLUDE_BENCHMARKS=Off \
        -DLLVM_INCLUDE_TESTS=Off -DLLVM_INCLUDE_EXAMPLES=Off -DLLVM_VERSION_PRINTER_SHOW_HOST_TARGET_INFO=Off \
        -DCMAKE_CXX_FLAGS="-D_LIBCPP_KEEP_TRANSITIVE_INCLUDES_LLVM23" \
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5  # LLVM 15's CMake files predate CMake 4
    cmake --build "$TOOLCHAINS/llvm-darwin-build"
    cmake --install "$TOOLCHAINS/llvm-darwin-build"
fi

# Wine headers and winebuild.
if [ ! -x "$TOOLCHAINS/wine/bin/winebuild" ]; then
    mkdir -p "$TOOLCHAINS/wine"
    curl -fsSL "$WINE_URL" | tar -xz -C "$TOOLCHAINS/wine"
fi

cd "$SRC/dxmt"
# OverPass's changes to DXMT (overpass/PATCHES.md), applied once to the pinned checkout.
for patch in "$SRC"/overpass/patches/*.patch; do
    if git apply --reverse --check "$patch" 2>/dev/null; then
        continue                                  # applied
    elif git apply --check "$patch" 2>/dev/null; then
        git apply "$patch"
    else
        echo "warning: $(basename "$patch") is neither applied nor applicable; assuming it's applied and edited further" >&2
    fi
done
if [ ! -f "$BUILD/build.ninja" ]; then
    meson setup --cross-file build-win64.txt --buildtype release --strip --prefix "$PREFIX" \
        -Denable_d3d12=true -Denable_nvapi=true -Denable_nvngx=true \
        -Dnative_llvm_path="$TOOLCHAINS/llvm-darwin" -Dwine_install_path="$TOOLCHAINS/wine" "$BUILD"
fi
meson compile -C "$BUILD"
meson install -C "$BUILD" --quiet
echo "Installed to $PREFIX"
