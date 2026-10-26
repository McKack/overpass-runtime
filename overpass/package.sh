#!/bin/bash
# Packs an install made by build.sh into overpass-runtime-<version>.tar.xz.
#   package.sh <version> <install dir> <output dir>
set -euo pipefail

VERSION=$1 INSTALL=$2 OUT=$3
REPO=$(cd "$(dirname "$0")/.." && pwd)
NAME=overpass-runtime-$VERSION
STAGE=$(mktemp -d)/$NAME
PIN=$(git -C "$REPO/dxmt" rev-parse --short HEAD)
PATCHES=$(cd "$REPO/overpass/patches" && ls *.patch)

mkdir -p "$STAGE/x86_64-windows" "$STAGE/x86_64-unix" "$OUT"
cp "$INSTALL"/x86_64-windows/*.dll "$STAGE/x86_64-windows/"
cp "$INSTALL"/x86_64-unix/winemetal.so "$STAGE/x86_64-unix/"
cp "$REPO/LICENSE" "$STAGE/LICENSE"
cp "$REPO/dxmt/LICENSE.OLD" "$STAGE/LICENSE.DXMT-OLD"

{
    echo "OverPass Runtime $VERSION"
    echo
    echo "Built from DXMT (https://github.com/3Shain/dxmt):"
    echo
    sed -n '1,/^$/p' "$REPO/dxmt/LICENSE"
    echo "Licensed under the GNU Lesser General Public License 2.1 or later (LICENSE)."
    echo "Older DXMT code under the MIT License: see LICENSE.DXMT-OLD."
    echo
    echo "nvapi64.dll contains functions adapted from dxvk-nvapi (https://github.com/jp7677/dxvk-nvapi):"
    echo
    sed -n '/MIT License/,/SOFTWARE\.$/p' "$REPO/dxmt/src/nvapi/nvapi_ported.cpp" | sed 's/^ \* \{0,1\}//'
} > "$STAGE/NOTICE"

cat > "$STAGE/README.txt" <<EOF
OverPass Runtime $VERSION
Direct3D 10/11/12, NVAPI and DLSS (through MetalFX) for Windows games under Wine on macOS.

Source:     https://github.com/McKack/overpass-runtime (tag v$VERSION)
DXMT pin:   $PIN
Patches:
$(echo "$PATCHES" | sed 's/^/  /')

Requirements: macOS on Apple silicon, OverPass Wine or another Wine that DXMT supports.
64-bit games only. Tiled resources need macOS 26.4 or later.

Install, either:
  - OverPass Wine: set OVERPASS_DLL_PATH_PREPEND=/path/to/$NAME for the game, or
  - copy x86_64-windows/*.dll into <prefix>/drive_c/windows/system32 and x86_64-unix/winemetal.so
    into Wine's lib/wine/x86_64-unix, then set dxgi, d3d11, d3d10core, d3d12 to native.
NVAPI and DLSS are off unless DXMT_ENABLE_NVEXT=1 is set.
EOF

tar -C "$(dirname "$STAGE")" -cJf "$OUT/$NAME.tar.xz" "$NAME"
(cd "$OUT" && shasum -a 256 "$NAME.tar.xz" > "$NAME.tar.xz.sha256")
echo "$OUT/$NAME.tar.xz"
