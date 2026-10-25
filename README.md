# OverPass Runtime

Free and open-source DirectX → Metal for running Windows games on macOS, with no dependency on Apple's
Game Porting Toolkit:

- Direct3D 10, 11 and 12, and DXGI, translated to Metal;
- NVIDIA DLSS calls served by Apple's MetalFX upscaler;
- NVAPI, for games that look for an NVIDIA GPU.

OverPass Runtime is built on [DXMT](https://github.com/3Shain/dxmt) by Feifan He and contributors. It's
DXMT's main branch with its optional D3D12, NVAPI and NGX parts enabled, plus OverPass's own additions.
It's built for, and tested with, [OverPass Wine](https://github.com/McKack/overpass-wine), and works with
other CrossOver-derived Wine builds.

## Who it's for

- **OverPass users:** OverPass downloads it from the Components manager. It isn't bundled, so the app
  stays small.
- **Developers:** download a release and ship your Windows build on the Mac inside a Wine wrapper, using
  only open-source parts.
- **Other Wine-based projects:** use it under the license below. Keep the copyright notices and credit
  DXMT and OverPass Runtime.

## Status

Early. It builds on a Mac and passes its smoke test (`tests/smoke.c`) under OverPass Wine:

- **D3D10/11 and DXGI:** DXMT's, unchanged.
- **D3D12:** a device is created with feature level 11_1 and shader model 6.0. Games that need 12_0 or
  newer don't run yet.
- **DLSS → MetalFX:** Super Resolution in D3D11 and D3D12 games (D3D12 is OverPass's addition). Not yet:
  ray reconstruction and frame generation.
- **NVAPI:** DXMT's functions plus 36 more (D3D12, driver profile sessions, D3D11 device creation,
  GPU and display queries). Active with `DXMT_ENABLE_NVEXT=1`.

No releases yet, and no CI.

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
| `tests/smoke.c` | Loads each part the way a game does and checks what it reports |
| `.github/workflows/` | CI build, smoke test and release, as in overpass-wine (planned) |

## License

LGPL-2.1-or-later, the same as DXMT's main branch and Wine. Some added files carry code adapted from
MIT-licensed projects; their notices are kept at the top of those files.
