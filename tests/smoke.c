/* OverPass Runtime smoke test: loads each DXMT component the way a game would and reports what it gets. */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <stdio.h>

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

    /* D3D12 */
    ID3D12Device *dev12 = NULL;
    PFN_D3D12_CREATE_DEVICE create12 = (void *)GetProcAddress(LoadLibraryA("d3d12.dll"), "D3D12CreateDevice");
    hr = create12 ? create12(NULL, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void **)&dev12) : E_FAIL;
    CHECK(SUCCEEDED(hr), "d3d12: device hr=%#lx", hr);
    if (dev12)
    {
        D3D12_FEATURE_DATA_SHADER_MODEL sm = { (D3D_SHADER_MODEL)0x68 };
        D3D12_FEATURE_DATA_D3D12_OPTIONS5 o5 = {0};
        D3D12_FEATURE_DATA_D3D12_OPTIONS7 o7 = {0};
        D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
        D3D12_FEATURE_DATA_FEATURE_LEVELS fls = { ARRAYSIZE(levels), levels, 0 };
        ID3D12Device_CheckFeatureSupport(dev12, D3D12_FEATURE_SHADER_MODEL, &sm, sizeof(sm));
        ID3D12Device_CheckFeatureSupport(dev12, D3D12_FEATURE_FEATURE_LEVELS, &fls, sizeof(fls));
        ID3D12Device_CheckFeatureSupport(dev12, D3D12_FEATURE_D3D12_OPTIONS5, &o5, sizeof(o5));
        ID3D12Device_CheckFeatureSupport(dev12, D3D12_FEATURE_D3D12_OPTIONS7, &o7, sizeof(o7));
        printf("info d3d12: max feature level %#x, shader model %#x, raytracing tier %d, mesh shader tier %d\n",
               fls.MaxSupportedFeatureLevel, sm.HighestShaderModel, o5.RaytracingTier, o7.MeshShaderTier);

        D3D12_COMMAND_QUEUE_DESC qd = { D3D12_COMMAND_LIST_TYPE_DIRECT };
        ID3D12CommandQueue *queue = NULL;
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
