# Patches

OverPass Runtime is DXMT at the commit pinned in `dxmt/`, plus the patches in `patches/`.
`build.sh` applies them in order to the pinned checkout before building (a patch that is already applied is skipped).
To start over from a clean DXMT, run `git -C dxmt checkout -- . && git -C dxmt clean -fd`.

| Patch | What it does |
|---|---|
| `0001-nvapi-add-D3D12-DRS-D3D11-device-creation-and-GPU-di.patch` | Adds 36 NVAPI functions: D3D12 (shader extension and raytracing caps, Reflex queue calls, pipeline options), DRS sessions and profiles, `NvAPI_D3D11_CreateDevice(AndSwapChain)`, `NvAPI_D3D_RegisterDevice`, and GPU/display lookups (PCI IDs, GPU type and info, GPU IDs, logical/physical GPUs, display names). Behaviour adapted from dxvk-nvapi (MIT); the notice is kept in `src/nvapi/nvapi_ported.cpp`. |

Verify with `tests/smoke.c` (run it with `DXMT_ENABLE_NVEXT=1` for the NVAPI checks).
