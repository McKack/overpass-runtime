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

Planning. Nothing is built yet.

## Layout (planned)

| Path | What |
|---|---|
| `dxmt/` | DXMT, pinned as a git submodule |
| `overpass/` | OverPass's patches and additions, with `PATCHES.md` describing each one |
| `.github/workflows/` | CI build, smoke test and release, as in overpass-wine |

## License

LGPL-2.1-or-later, the same as DXMT's main branch and Wine.
