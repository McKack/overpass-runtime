#!/bin/bash
# Builds and runs every OverPass Runtime test under OverPass Wine. The work dir (default ../../testenv, outside
# the repo) keeps DXC, the test programs and a Wine prefix between runs.
#   tests/run-all.sh [work dir]
# WINE (default: OverPass's current Wine) and INSTALL (default: ../../install, from overpass/build.sh) can be set.
set -uo pipefail

TESTS="$(cd "$(dirname "$0")" && pwd)"
SRC="$(cd "$TESTS/.." && pwd)"
WORK="${1:-$SRC/../testenv}"
INSTALL="${INSTALL:-$SRC/../install}"
WINE="${WINE:-$HOME/Library/Application Support/OverPass/Wine/current/bin/wine}"
DXC_TAG=v1.9.2609
mkdir -p "$WORK" && WORK="$(cd "$WORK" && pwd)" && INSTALL="$(cd "$INSTALL" && pwd)"
BIN="$WORK/smoke" && mkdir -p "$BIN"

# DXC, Microsoft's DXIL compiler (Windows build, runs under Wine).
if [ ! -f "$WORK/dxc/bin/x64/dxc.exe" ]; then
    mkdir -p "$WORK/dxc-download" "$WORK/dxc"
    gh release download "$DXC_TAG" -R microsoft/DirectXShaderCompiler -p 'dxc_*.zip' -D "$WORK/dxc-download" --clobber
    unzip -q -o "$WORK"/dxc-download/dxc_*.zip -d "$WORK/dxc"
fi

# Test programs.
CXX=(x86_64-w64-mingw32-g++ -O1 -static -I"$SRC/dxmt/src/nvngx" -I"$SRC/dxmt/external/nvapi")
x86_64-w64-mingw32-gcc -O1 -static -I"$SRC/dxmt/external/nvapi" "$TESTS/smoke.c" -o "$BIN/smoke.exe" \
    -ld3d11 -ld3d12 -ldxgi -luuid || exit 1
for t in tiled12 dlss12 heap12 bary12 atomic12 mesh12 gs12 query12 sm6pso; do
    "${CXX[@]}" "$TESTS/$t.cpp" -o "$BIN/$t.exe" -ld3d12 -ldxgi -luuid || exit 1
done

# Wine prefix with the runtime's extra DLLs in system32, as OverPass puts them in a bottle.
export WINEPREFIX="$BIN/prefix" WINEDEBUG=-all DXMT_ENABLE_NVEXT=1 OVERPASS_DLL_PATH_PREPEND="$INSTALL"
LIB="$(dirname "$(dirname "$WINE")")/lib/wine"
export WINEDLLPATH="$LIB/x86_64-windows:$LIB"
[ -d "$WINEPREFIX/drive_c/windows/system32" ] || "$WINE" wineboot -i >/dev/null 2>&1
"$(dirname "$WINE")/wineserver" -w
for dll in "$INSTALL"/x86_64-windows/*.dll; do
    name="$(basename "$dll" | tr A-Z a-z)"
    [ -f "$WINEPREFIX/drive_c/windows/system32/$name" ] || cp "$dll" "$WINEPREFIX/drive_c/windows/system32/$name"
done

DXC=("$WINE" "$WORK/dxc/bin/x64/dxc.exe" -nologo)
cd "$BIN"
compile() { "${DXC[@]}" "$TESTS/$1" -T "$2" -E "$3" -Fo "$4" "${@:5}" >/dev/null 2>&1 || echo "DXC failed: $*"; }
compile heap12.hlsl cs_6_6 main heap12.cso
compile bary12.hlsl vs_6_1 vs_main bary12_vs.cso
compile bary12.hlsl ps_6_1 ps_main bary12_ps.cso
compile atomic12.hlsl cs_6_6 main atomic12_split.cso -DSPLIT
compile atomic12.hlsl cs_6_6 main atomic12.cso
compile atomic12.hlsl cs_6_6 main atomic12_heap.cso -DHEAP
compile mesh12.hlsl ms_6_5 ms_basic ms_basic.cso
compile mesh12.hlsl as_6_5 as_main as_main.cso
compile mesh12.hlsl ms_6_5 ms_amp ms_amp.cso
compile mesh12.hlsl ps_6_5 ps_main ps_main.cso
compile gs12.hlsl vs_6_0 vs_main gs12_vs.cso
compile gs12.hlsl gs_6_0 gs_main gs12_gs.cso
compile gs12.hlsl ps_6_0 ps_main gs12_ps.cso
compile query12.hlsl vs_6_0 vs_main query12_vs.cso
compile query12.hlsl ps_6_0 ps_main query12_ps.cso

FAILED=()
for run in smoke tiled12 dlss12 "dlss12 hires-mv" heap12 bary12 atomic12 mesh12 gs12 query12; do
    echo "=== $run"
    set -- $run
    "$WINE" "$1.exe" "${@:2}" 2>&1 | tr -d '\r' | grep -v "^ok  " ; status=${PIPESTATUS[0]}
    [ "$status" = 0 ] || FAILED+=("$run ($status)")
done
echo "=== sm6 corpus"
rm -rf "$WORK/smcorpus" && mkdir -p "$WORK/smcorpus/src" "$WORK/smcorpus/dxil" "$WORK/smcorpus/log"
cp "$TESTS"/sm6/*.hlsl "$WORK/smcorpus/src/" && cp "$TESTS/sm6/features.txt" "$WORK/smcorpus/list.txt"
python3 "$TESTS/sm6/run.py" "$WORK" "$WORK/smcorpus" | tee "$WORK/sm6-results.txt" | tail -4

[ ${#FAILED[@]} = 0 ] && echo "All runtime tests passed." || { echo "Failed: ${FAILED[*]}"; exit 1; }
