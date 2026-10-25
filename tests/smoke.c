/* OverPass Runtime smoke test: loads each DXMT component the way a game would and reports what it gets. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <stdio.h>
#define __NVAPI_EMPTY_SAL
#include "nvapi.h"

typedef void *(__cdecl *nvapi_qi_fn)(unsigned int);
typedef int (__cdecl *nvapi_init_fn)(void);
typedef int (__cdecl *nvapi_enum_fn)(void **, unsigned long *);
typedef int (__cdecl *nvapi_name_fn)(void *, char *);

static int failures;
#define CHECK(cond, ...) do { printf("%s ", (cond) ? "ok  " : "FAIL"); printf(__VA_ARGS__); printf("\n"); if (!(cond)) failures++; } while (0)

int main(void)
{
    HMODULE m; HRESULT hr;

    /* DXGI adapter */
    IDXGIFactory1 *factory = NULL; IDXGIAdapter1 *adapter = NULL; DXGI_ADAPTER_DESC1 ad = {0};
    HRESULT (WINAPI *create_factory)(REFIID, void **) = (void *)GetProcAddress(LoadLibraryA("dxgi.dll"), "CreateDXGIFactory1");
    hr = create_factory ? create_factory(&IID_IDXGIFactory1, (void **)&factory) : E_FAIL;
    if (SUCCEEDED(hr)) hr = IDXGIFactory1_EnumAdapters1(factory, 0, &adapter);
    if (SUCCEEDED(hr)) IDXGIAdapter1_GetDesc1(adapter, &ad);
    CHECK(SUCCEEDED(hr), "dxgi: adapter \"%ls\" vendor %04x device %04x, %llu MB", ad.Description, ad.VendorId, ad.DeviceId,
          (unsigned long long)ad.DedicatedVideoMemory >> 20);

    /* D3D11 */
    ID3D11Device *dev11 = NULL; D3D_FEATURE_LEVEL fl11 = 0;
    PFN_D3D11_CREATE_DEVICE create11 = (void *)GetProcAddress(LoadLibraryA("d3d11.dll"), "D3D11CreateDevice");
    hr = create11 ? create11(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &dev11, &fl11, NULL) : E_FAIL;
    CHECK(SUCCEEDED(hr), "d3d11: device hr=%#lx feature level %#x", hr, fl11);
    if (dev11)
    {
        /* ANGLE (Chromium, Qt WebEngine) only renders into a shared texture whose DXGI usage says it can. */
        D3D11_TEXTURE2D_DESC td = { 64, 64, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, {1, 0}, D3D11_USAGE_DEFAULT,
                                    D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, 0,
                                    D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE };
        ID3D11Texture2D *tex = NULL; IDXGIResource *res = NULL; DXGI_USAGE usage = 0;
        hr = ID3D11Device_CreateTexture2D(dev11, &td, NULL, &tex);
        if (SUCCEEDED(hr)) hr = ID3D11Texture2D_QueryInterface(tex, &IID_IDXGIResource, (void **)&res);
        if (SUCCEEDED(hr)) hr = IDXGIResource_GetUsage(res, &usage);
        CHECK(SUCCEEDED(hr) && usage == (DXGI_USAGE_SHADER_INPUT | DXGI_USAGE_RENDER_TARGET_OUTPUT),
              "d3d11: shared render target's DXGI usage %#x (%#x expected)", usage, DXGI_USAGE_SHADER_INPUT | DXGI_USAGE_RENDER_TARGET_OUTPUT);
        if (res) IDXGIResource_Release(res);
        if (tex) ID3D11Texture2D_Release(tex);
    }

    /* D3D12 */
    ID3D12Device *dev12 = NULL; ID3D12CommandQueue *queue = NULL;
    PFN_D3D12_CREATE_DEVICE create12 = (void *)GetProcAddress(LoadLibraryA("d3d12.dll"), "D3D12CreateDevice");
    hr = create12 ? create12(NULL, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void **)&dev12) : E_FAIL;
    CHECK(SUCCEEDED(hr), "d3d12: device hr=%#lx", hr);
    if (dev12)
    {
        D3D12_FEATURE_DATA_SHADER_MODEL sm = { (D3D_SHADER_MODEL)0x68 };
        D3D12_FEATURE_DATA_D3D12_OPTIONS o0 = {0};
        D3D12_FEATURE_DATA_D3D12_OPTIONS5 o5 = {0};
        D3D12_FEATURE_DATA_D3D12_OPTIONS7 o7 = {0};
        D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
        D3D12_FEATURE_DATA_FEATURE_LEVELS fls = { ARRAYSIZE(levels), levels, 0 };
        /* Like games do: ask for the newest model, step down while the device calls the request unknown. */
        while (sm.HighestShaderModel > D3D_SHADER_MODEL_5_1 &&
               ID3D12Device_CheckFeatureSupport(dev12, D3D12_FEATURE_SHADER_MODEL, &sm, sizeof(sm)) == E_INVALIDARG)
            sm.HighestShaderModel = sm.HighestShaderModel == 0x60 ? D3D_SHADER_MODEL_5_1 : sm.HighestShaderModel - 1;
        ID3D12Device_CheckFeatureSupport(dev12, D3D12_FEATURE_FEATURE_LEVELS, &fls, sizeof(fls));
        ID3D12Device_CheckFeatureSupport(dev12, D3D12_FEATURE_D3D12_OPTIONS, &o0, sizeof(o0));
        ID3D12Device_CheckFeatureSupport(dev12, D3D12_FEATURE_D3D12_OPTIONS5, &o5, sizeof(o5));
        ID3D12Device_CheckFeatureSupport(dev12, D3D12_FEATURE_D3D12_OPTIONS7, &o7, sizeof(o7));
        printf("info d3d12: max feature level %#x, shader model %#x, raytracing tier %d, mesh shader tier %d\n",
               fls.MaxSupportedFeatureLevel, sm.HighestShaderModel, o5.RaytracingTier, o7.MeshShaderTier);
        printf("info d3d12: tiled resources tier %d, resource binding tier %d, conservative raster tier %d, ROVs %d\n",
               o0.TiledResourcesTier, o0.ResourceBindingTier, o0.ConservativeRasterizationTier, o0.ROVsSupported);

        D3D12_COMMAND_QUEUE_DESC qd = { D3D12_COMMAND_LIST_TYPE_DIRECT };
        hr = ID3D12Device_CreateCommandQueue(dev12, &qd, &IID_ID3D12CommandQueue, (void **)&queue);
        CHECK(SUCCEEDED(hr), "d3d12: command queue hr=%#lx", hr);
    }

    /* NVAPI */
    m = LoadLibraryA("nvapi64.dll");
    nvapi_qi_fn qi = m ? (void *)GetProcAddress(m, "nvapi_QueryInterface") : NULL;
    CHECK(qi != NULL, "nvapi64: nvapi_QueryInterface %s", qi ? "found" : "missing");
    if (qi)
    {
        nvapi_init_fn init = qi(0x0150e828);          /* NvAPI_Initialize */
        int st = init ? init() : -999;
        printf("info nvapi: NvAPI_Initialize = %d (0 = OK; -6 = no NVIDIA GPU reported)\n", st);
        nvapi_enum_fn enum_gpus = qi(0xe5ac921f);     /* NvAPI_EnumPhysicalGPUs */
        nvapi_name_fn name = qi(0xceee8e9f);          /* NvAPI_GPU_GetFullName */
        void *gpus[64]; unsigned long n = 0; char gpu_name[64] = "";
        if (!st && enum_gpus && !enum_gpus(gpus, &n) && n && name) name(gpus[0], gpu_name);
        printf("info nvapi: %lu GPU(s), first \"%s\"\n", n, gpu_name);

        /* Functions OverPass Runtime adds to DXMT's NVAPI. */
        static const unsigned int added[] = {
            0x6a16d3a0, 0xbb939ee5, 0x8c02c4d0, 0x3dfacec8, 0xac2dfeb5, 0x43d867c0, 0x85a6c2a0, 0x8d025b77,
            0xe24ead45, 0x5c607a27, 0x03d6e8cb, 0x13c98f73, 0x0694d52e, 0xdad9cff8, 0x375dbd6b, 0xfcbc7e14,
            0x577dd202, 0x73bf8338, 0x7e4a9a0b, 0xeee566b2, 0xda8466a0, 0xcc176068, 0x17093206, 0x4347a9de,
            0xd3b24d2d, 0xccfffc10, 0xadd604d1, 0xaea3fa32, 0x6533ea3e, 0x5380ad1a, 0xc33baeb1, 0xafd1b02c,
            0x2ddfb66e, 0xe3640a56, 0x35c29134, 0x22a78b05, 0xee1370cf, 0x9ea74659,
        };
        unsigned found = 0;
        for (unsigned i = 0; i < ARRAYSIZE(added); i++) found += qi(added[i]) != NULL;
        CHECK(found == ARRAYSIZE(added), "nvapi: %u of %u added functions present", found, (unsigned)ARRAYSIZE(added));

        /* DRS: games open a session and read their profile before anything else. */
        NvDRSSessionHandle session = NULL; NvDRSProfileHandle profile = NULL;
        NVDRS_SETTING setting = { NVDRS_SETTING_VER1 };
        int s1 = ((NvAPI_Status (__cdecl *)(NvDRSSessionHandle *))qi(0x0694d52e))(&session);
        int s2 = ((NvAPI_Status (__cdecl *)(NvDRSSessionHandle))qi(0x375dbd6b))(session);
        int s3 = ((NvAPI_Status (__cdecl *)(NvDRSSessionHandle, NvDRSProfileHandle *))qi(0xda8466a0))(session, &profile);
        int s4 = ((NvAPI_Status (__cdecl *)(NvDRSSessionHandle, NvDRSProfileHandle, NvU32, NVDRS_SETTING *))qi(0x73bf8338))(session, profile, 0x10e41df3, &setting);
        int s5 = ((NvAPI_Status (__cdecl *)(NvDRSSessionHandle))qi(0xdad9cff8))(session);
        CHECK(!s1 && session && !s2 && !s3 && profile && s4 == NVAPI_SETTING_NOT_FOUND && !s5,
              "nvapi: DRS session %d, load %d, base profile %d, setting %d (-160 = not found), destroy %d", s1, s2, s3, s4, s5);

        /* GPU queries, using the handles NvAPI_SYS_GetPhysicalGPUs returns. */
        NV_PHYSICAL_GPUS phys = { NV_PHYSICAL_GPUS_VER1 };
        int g1 = ((NvAPI_Status (__cdecl *)(NV_PHYSICAL_GPUS *))qi(0xd3b24d2d))(&phys);
        CHECK(!g1 && phys.gpuHandleCount >= 1, "nvapi: SYS_GetPhysicalGPUs %d, %u GPU(s)", g1, phys.gpuHandleCount);
        if (!g1 && phys.gpuHandleCount)
        {
            NvPhysicalGpuHandle gpu = phys.gpuHandleData[0].hPhysicalGpu, back = NULL;
            NvLogicalGpuHandle logical = NULL;
            NvU32 dev_id = 0, subsys = 0, rev = 0, ext = 0, gpu_id = 0;
            NV_GPU_TYPE type = 0; NV_GPU_INFO info = { NV_GPU_INFO_VER2 };
            int p = ((NvAPI_Status (__cdecl *)(NvPhysicalGpuHandle, NvU32 *, NvU32 *, NvU32 *, NvU32 *))qi(0x2ddfb66e))(gpu, &dev_id, &subsys, &rev, &ext);
            CHECK(!p && (dev_id & 0xffff) == ad.VendorId && (dev_id >> 16) == ad.DeviceId,
                  "nvapi: GPU_GetPCIIdentifiers %d, device id %08x (DXGI vendor %04x device %04x)", p, dev_id, ad.VendorId, ad.DeviceId);
            int t = ((NvAPI_Status (__cdecl *)(NvPhysicalGpuHandle, NV_GPU_TYPE *))qi(0xc33baeb1))(gpu, &type);
            int in = ((NvAPI_Status (__cdecl *)(NvPhysicalGpuHandle, NV_GPU_INFO *))qi(0xafd1b02c))(gpu, &info);
            CHECK(!t && type == NV_SYSTEM_TYPE_DGPU && !in, "nvapi: GPU_GetGPUType %d type %d, GPU_GetGPUInfo %d, %u RT / %u tensor cores",
                  t, type, in, info.rayTracingCores, info.tensorCores);
            int i1 = ((NvAPI_Status (__cdecl *)(NvPhysicalGpuHandle, NvU32 *))qi(0x6533ea3e))(gpu, &gpu_id);
            int i2 = ((NvAPI_Status (__cdecl *)(NvU32, NvPhysicalGpuHandle *))qi(0x5380ad1a))(gpu_id, &back);
            int l1 = ((NvAPI_Status (__cdecl *)(NvPhysicalGpuHandle, NvLogicalGpuHandle *))qi(0xadd604d1))(gpu, &logical);
            CHECK(!i1 && !i2 && back == gpu && !l1 && logical, "nvapi: GPU ID %08x round trip %d/%d, logical GPU %d", gpu_id, i1, i2, l1);
            int bad = ((NvAPI_Status (__cdecl *)(NvPhysicalGpuHandle, NV_GPU_TYPE *))qi(0xc33baeb1))((NvPhysicalGpuHandle)(ULONG_PTR)1, &type);
            CHECK(bad == NVAPI_EXPECTED_PHYSICAL_GPU_HANDLE, "nvapi: bad GPU handle gives %d (-101 expected)", bad);
        }

        /* Displays: handle -> name -> handle, and the GPU behind the primary display. */
        NvDisplayHandle disp = NULL, disp2 = NULL; NvAPI_ShortString disp_name = ""; NvLogicalGpuHandle disp_gpu = NULL;
        NvU32 primary = 0; NvPhysicalGpuHandle primary_gpu = NULL;
        int d1 = ((NvAPI_Status (__cdecl *)(NvU32, NvDisplayHandle *))qi(0x9abdd40d))(0, &disp);
        int d2 = ((NvAPI_Status (__cdecl *)(NvDisplayHandle, char *))qi(0x22a78b05))(disp, disp_name);
        int d3 = ((NvAPI_Status (__cdecl *)(const char *, NvDisplayHandle *))qi(0x35c29134))(disp_name, &disp2);
        int d4 = ((NvAPI_Status (__cdecl *)(NvDisplayHandle, NvLogicalGpuHandle *))qi(0xee1370cf))(disp, &disp_gpu);
        int d5 = ((NvAPI_Status (__cdecl *)(NvU32 *))qi(0x1e9d8a31))(&primary);
        int d6 = ((NvAPI_Status (__cdecl *)(NvU32, NvPhysicalGpuHandle *))qi(0x9ea74659))(primary, &primary_gpu);
        CHECK(!d1 && !d2 && !d3 && disp2 == disp && !d4 && !d5 && !d6 && primary_gpu,
              "nvapi: display \"%s\" %d/%d/%d, its GPU %d, primary display %#x %d, its GPU %d", disp_name, d1, d2, d3, d4, primary, d5, d6);

        /* D3D11 device creation through NVAPI. */
        ID3D11Device *nvdev11 = NULL; int nvfl = -1; /* NVAPI_DEVICE_FEATURE_LEVEL, C++ only in nvapi.h */
        int c11 = ((NvAPI_Status (__cdecl *)(IDXGIAdapter *, D3D_DRIVER_TYPE, HMODULE, UINT, const D3D_FEATURE_LEVEL *, UINT, UINT,
                                              ID3D11Device **, D3D_FEATURE_LEVEL *, ID3D11DeviceContext **, int *))qi(0x6a16d3a0))(
            NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &nvdev11, NULL, NULL, &nvfl);
        CHECK(!c11 && nvdev11 && nvfl == 3 /* 11_0 */, "nvapi: D3D11_CreateDevice %d, NVAPI level %d", c11, nvfl);
        if (nvdev11) ID3D11Device_Release(nvdev11);

        /* D3D12: answers for what DXMT does not have. */
        if (dev12 && queue)
        {
            bool shfl = true; int reorder = -1, omm = -1;
            int e1 = ((NvAPI_Status (__cdecl *)(ID3D12Device *, NvU32, bool *))qi(0x3dfacec8))(dev12, 1 /* NV_EXTN_OP_SHFL */, &shfl);
            int e2 = ((NvAPI_Status (__cdecl *)(IUnknown *, NvU32, NvU32))qi(0xac2dfeb5))((IUnknown *)dev12, 127, 0);
            int r1 = ((NvAPI_Status (__cdecl *)(ID3D12Device *, int, void *, size_t))qi(0x85a6c2a0))(dev12, 0, &reorder, sizeof(reorder));
            int r2 = ((NvAPI_Status (__cdecl *)(ID3D12Device *, int, void *, size_t))qi(0x85a6c2a0))(dev12, 1, &omm, sizeof(omm));
            int q1 = ((NvAPI_Status (__cdecl *)(ID3D12CommandQueue *, int))qi(0x03d6e8cb))(queue, 0);
            CHECK(!e1 && !shfl && e2 == NVAPI_NO_IMPLEMENTATION && !r1 && !reorder && !r2 && !omm && !q1,
                  "nvapi: D3D12 shader extension op %d (supported %d), slot space %d (-3 = none), RT caps %d/%d reorder %d omm %d, out-of-band queue %d",
                  e1, shfl, e2, r1, r2, reorder, omm, q1);
        }
    }

    /* NGX */
    m = LoadLibraryA("nvngx.dll");
    FARPROC ngx11 = m ? GetProcAddress(m, "NVSDK_NGX_D3D11_Init") : NULL;
    FARPROC ngx12 = m ? GetProcAddress(m, "NVSDK_NGX_D3D12_Init") : NULL;
    CHECK(ngx11 != NULL, "nvngx: NVSDK_NGX_D3D11_Init %s", ngx11 ? "found" : "missing");
    printf("info nvngx: NVSDK_NGX_D3D12_Init %s\n", ngx12 ? "found" : "missing");

    printf("%d failure(s)\n", failures);
    return failures;
}
