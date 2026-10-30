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

mkdir -p "$STAGE/x86_64-windows" "$STAGE/i386-windows" "$STAGE/x86_64-unix" "$OUT"
cp "$INSTALL"/x86_64-windows/*.dll "$STAGE/x86_64-windows/"
cp "$INSTALL"/i386-windows/*.dll "$STAGE/i386-windows/"
cp "$INSTALL"/x86_64-unix/winemetal.so "$STAGE/x86_64-unix/"
cp "$REPO/LICENSE" "$STAGE/LICENSE"
cp "$REPO/dxmt/LICENSE.OLD" "$STAGE/LICENSE.DXMT-OLD"

# mtld3d's release, unchanged: its builtin d3d9.dll + mtld3d.dll pair and the unix half, x86_64 Wine only.
# OverPass prepends mtld3d/ only for bottles that use it for Direct3D 9.
MTLD3D_VERSION=$(sed -n 's/^version=//p' "$REPO/overpass/mtld3d.pin")
MTLD3D_SHA=$(sed -n 's/^sha256=//p' "$REPO/overpass/mtld3d.pin")
MTLD3D=$(mktemp -d)
curl -fsSL -o "$MTLD3D/mtld3d.tar.xz" "https://github.com/athei/mtld3d/releases/download/$MTLD3D_VERSION/mtld3d.tar.xz"
echo "$MTLD3D_SHA  $MTLD3D/mtld3d.tar.xz" | shasum -a 256 -c -
tar -xJf "$MTLD3D/mtld3d.tar.xz" -C "$MTLD3D"
mkdir -p "$STAGE/mtld3d"
cp -R "$MTLD3D/wine/i386-windows" "$MTLD3D/wine/x86_64-windows" "$MTLD3D/wine/x86_64-unix" "$STAGE/mtld3d/"
cp "$MTLD3D/LICENSE" "$STAGE/mtld3d/LICENSE"
cp "$MTLD3D/mtld3d.conf" "$STAGE/mtld3d/mtld3d.conf"
echo "mtld3d $MTLD3D_VERSION (https://github.com/athei/mtld3d/releases/tag/$MTLD3D_VERSION), unchanged" > "$STAGE/mtld3d/VERSION"
rm -rf "$MTLD3D"

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
    echo
    echo "mtld3d/ is mtld3d $MTLD3D_VERSION by Alexander Theißen (https://github.com/athei/mtld3d), unchanged,"
    echo "under the zlib License: see mtld3d/LICENSE."
} > "$STAGE/NOTICE"

cat > "$STAGE/README.txt" <<EOF
OverPass Runtime $VERSION
Direct3D 10/11/12, NVAPI and DLSS (through MetalFX) for Windows games under Wine on macOS, and Direct3D 9
through mtld3d.

Source:     https://github.com/McKack/overpass-runtime (tag v$VERSION)
DXMT pin:   $PIN
mtld3d:     $MTLD3D_VERSION (unchanged release, in mtld3d/)
Patches:
$(echo "$PATCHES" | sed 's/^/  /')

Requirements: macOS on Apple silicon, OverPass Wine or another Wine that DXMT supports.
32-bit games get Direct3D 10/11; Direct3D 12, NVAPI and DLSS are 64-bit only. Tiled resources need macOS
26.4 or later.

Install, either:
  - OverPass Wine: set OVERPASS_DLL_PATH_PREPEND=/path/to/$NAME for the game, or
  - copy x86_64-windows/*.dll into <prefix>/drive_c/windows/system32, i386-windows/*.dll into its
    syswow64 and x86_64-unix/winemetal.so into Wine's lib/wine/x86_64-unix, then set dxgi, d3d11,
    d3d10core, d3d12 to native.
With OverPass Wine, winemetal.dll still needs a copy in system32 and syswow64: Wine only loads a builtin
that the prefix has a file for.
NVAPI and DLSS are off unless DXMT_ENABLE_NVEXT=1 is set.

Direct3D 9 (32- and 64-bit games): put mtld3d/ first in OVERPASS_DLL_PATH_PREPEND
(mtld3d/:<this folder>), and copy mtld3d/i386-windows/mtld3d.dll into the prefix's syswow64 and
mtld3d/x86_64-windows/mtld3d.dll into its system32. Settings: mtld3d/mtld3d.conf, or MTLD3D_CONFIG.
EOF

tar -C "$(dirname "$STAGE")" -cJf "$OUT/$NAME.tar.xz" "$NAME"
(cd "$OUT" && shasum -a 256 "$NAME.tar.xz" > "$NAME.tar.xz.sha256")
echo "$OUT/$NAME.tar.xz"
