# overpass-runtime

Free and open-source DirectX → Metal for running Windows games on macOS, with no dependency on Apple's
Game Porting Toolkit:

- Direct3D 10, 11 and 12, and DXGI, translated to Metal;
- NVIDIA DLSS calls served by Apple's MetalFX upscaler;
- NVAPI, for games that look for an NVIDIA GPU.
- Direct3D 9 (32- and 64-bit games) through [mtld3d](https://github.com/athei/mtld3d) by Alexander Theißen,
  an unchanged release pinned in `overpass/mtld3d.pin` (zlib License), in the package's `mtld3d/` folder.

overpass-runtime is built on [DXMT](https://github.com/3Shain/dxmt) by Feifan He and contributors. It's
DXMT's main branch with its optional D3D12, NVAPI and NGX parts enabled, plus OverPass's own additions.
It's built for, and tested with, [overpass-wine](https://github.com/McKack/overpass-wine), and works with
other CrossOver-derived Wine builds.

## Who it's for

- **OverPass users:** OverPass downloads it from the Components manager. It isn't bundled, so the app
  stays small.
- **Developers:** download a release and ship your Windows build on the Mac inside a Wine wrapper, using
  only open-source parts.
- **Other Wine-based projects:** use it under the license below. Keep the copyright notices and credit
  DXMT and overpass-runtime.

## Status

Early. It builds on a Mac and passes its tests (`tests/run-all.sh`) under overpass-wine:

- **D3D10/11 and DXGI:** DXMT's, unchanged, for 64-bit and 32-bit games. D3D12, NVAPI and DLSS are 64-bit only.
- **D3D12:** feature level 12_0 on macOS 26.4 or later (tiled resources tier 2 through Metal placement
  sparse resources; OverPass's addition), 11_1 before that. Shader model 6.8: 26 of the 27 shader model
  6.x features in `tests/sm6` convert (wave ops, 16-bit types, SM 6.6 resource handles and
  `ResourceDescriptorHeap`/`SamplerDescriptorHeap`, 64-bit atomics on buffers and groupshared memory,
  barycentrics, mesh and amplification shaders, and the 6.7/6.8 sampling ops). Geometry shaders (DXMT's). The one that doesn't,
  writable MSAA textures (6.7, optional), can't be done on Metal and isn't reported. Mesh shader tier 1.
  Not yet: 64-bit atomics on typed (`R64_UINT`) textures, raytracing, variable rate shading.
- **DLSS → MetalFX:** Super Resolution in D3D11 and D3D12 games (D3D12 is OverPass's addition). Not yet:
  ray reconstruction and frame generation.
- **NVAPI:** DXMT's functions plus 36 more (D3D12, driver profile sessions, D3D11 device creation,
  GPU and display queries). Active with `DXMT_ENABLE_NVEXT=1`.

Releases: see the GitHub releases page. No CI yet.

## Building

On a Mac with Xcode and its Metal toolchain (`xcodebuild -downloadComponent MetalToolchain`), plus
Homebrew's `mingw-w64`, `cmake`, `ninja` and `meson`:

    git submodule update --init --recursive
    overpass/build.sh

The first run builds LLVM 15 (about 4 minutes on an M4) and downloads Wine headers. The result goes
to `../install`.

## Layout

| Path | What |
|---|---|
| `dxmt/` | DXMT, pinned as a git submodule |
| `overpass/` | `build.sh`, and OverPass's patches to DXMT in `patches/`, each described in `PATCHES.md` |
| `tests/` | Smoke test, runtime tests for D3D12 features (`*12.cpp` with their shaders), and `sm6/`, the shader model 6.x coverage corpus |

## License

LGPL-2.1-or-later, the same as DXMT's main branch and Wine. Some added files carry code adapted from
MIT-licensed projects; their notices are kept at the top of those files.
