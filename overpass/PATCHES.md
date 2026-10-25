# Patches

OverPass Runtime is DXMT at the commit pinned in `dxmt/`, plus the patches in `patches/`.
`build.sh` applies them in order to the pinned checkout before building (a patch that is already applied is skipped).
To start over from a clean DXMT, run `git -C dxmt checkout -- . && git -C dxmt clean -fd`.

| Patch | What it does |
|---|---|
| `0001-nvapi-add-D3D12-DRS-D3D11-device-creation-and-GPU-di.patch` | Adds 36 NVAPI functions: D3D12 (shader extension and raytracing caps, Reflex queue calls, pipeline options), DRS sessions and profiles, `NvAPI_D3D11_CreateDevice(AndSwapChain)`, `NvAPI_D3D_RegisterDevice`, and GPU/display lookups (PCI IDs, GPU type and info, GPU IDs, logical/physical GPUs, display names). Behaviour adapted from dxvk-nvapi (MIT); the notice is kept in `src/nvapi/nvapi_ported.cpp`. |
| `0002-d3d11-report-DXGI-usage-from-bind-flags-and-CPU-acce.patch` | `IDXGIResource::GetUsage` reports what the resource can be used for (shader input, render target output, unordered access, CPU access) instead of 0. ANGLE checks it before rendering into a shared texture, so Chromium-based apps such as GOG Galaxy (Qt WebEngine) failed with "Unable to initialize SkSurface" and crashed. Verified: GOG Galaxy 2.1.10 shows its login window on DXMT. |
| `0003-d3d12-nvngx-DLSS-Super-Resolution-on-D3D12-through-M.patch` | DLSS Super Resolution for D3D12 games, served by MetalFX's temporal scaler: NGX's D3D12 entry points, and a command list extension (`IMTLD3D12CommandListExt`) that records the upscale into the game's command list. Motion vectors at output size are shrunk to render size by a compute pass first. Verified with `tests/dlss12.cpp` (both motion vector sizes): a single-colour frame upscales to an output of the same colour. |

Verify with `tests/smoke.c` and `tests/dlss12.cpp` (`dlss12.exe` and `dlss12.exe hires-mv`), run with `DXMT_ENABLE_NVEXT=1`.
