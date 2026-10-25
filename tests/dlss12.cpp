/* OverPass Runtime: DLSS on D3D12, end to end. A 960x540 frame filled with one colour goes through NGX's
 * DLSS Super Resolution (MetalFX underneath) into a 1920x1080 output, which is read back and checked.
 * Run with DXMT_ENABLE_NVEXT=1. With the argument "hires-mv", motion vectors come at output size (no
 * DLSS MVLowRes flag), which runtimes have to shrink to render size first. */
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "nvngx.hpp" /* DXMT's NGX declarations: the parameter object's layout */

static int failures;
#define CHECK(cond, ...) do { printf("%s ", (cond) ? "ok  " : "FAIL"); printf(__VA_ARGS__); printf("\n"); if (!(cond)) failures++; } while (0)

template <typename T> static T ngx(HMODULE m, const char *name) { return reinterpret_cast<T>(GetProcAddress(m, name)); }

static ID3D12Resource *texture(ID3D12Device *dev, UINT w, UINT h, DXGI_FORMAT f, D3D12_RESOURCE_FLAGS flags,
                               D3D12_RESOURCE_STATES state, const D3D12_CLEAR_VALUE *clear)
{
    D3D12_HEAP_PROPERTIES heap = { D3D12_HEAP_TYPE_DEFAULT };
    D3D12_RESOURCE_DESC desc = { D3D12_RESOURCE_DIMENSION_TEXTURE2D, 0, w, h, 1, 1, f, {1, 0}, D3D12_TEXTURE_LAYOUT_UNKNOWN, flags };
    ID3D12Resource *r = nullptr;
    dev->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, state, clear, IID_PPV_ARGS(&r));
    return r;
}

static unsigned short half(float f)
{
    unsigned int x; memcpy(&x, &f, 4);
    int e = ((x >> 23) & 0xff) - 127 + 15;
    return (unsigned short)(((x >> 16) & 0x8000) | (e <= 0 ? 0 : e >= 31 ? 0x7c00 : (e << 10) | ((x >> 13) & 0x3ff)));
}

static float from_half(unsigned short h)
{
    int e = (h >> 10) & 0x1f; float m = (h & 0x3ff) / 1024.0f;
    float v = e == 0 ? m * powf(2, -14) : (1 + m) * powf(2, e - 15);
    return (h & 0x8000) ? -v : v;
}

int main(int argc, char **argv)
{
    const bool hires_mv = argc > 1 && !strcmp(argv[1], "hires-mv");
    printf("info motion vectors at %s size\n", hires_mv ? "output" : "render");
    ID3D12Device *dev = nullptr;
    HRESULT hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&dev));
    CHECK(SUCCEEDED(hr), "d3d12: device hr=%#lx", hr);
    if (!dev) return 1;

    D3D12_COMMAND_QUEUE_DESC qd = { D3D12_COMMAND_LIST_TYPE_DIRECT };
    ID3D12CommandQueue *queue; ID3D12CommandAllocator *alloc; ID3D12GraphicsCommandList *list; ID3D12Fence *fence;
    dev->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue));
    dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc));
    dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc, nullptr, IID_PPV_ARGS(&list));
    dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));

    const UINT W = 960, H = 540, OW = 1920, OH = 1080;
    const float colour[4] = { 0.25f, 0.5f, 0.75f, 1.0f };
    D3D12_CLEAR_VALUE cv_color = { DXGI_FORMAT_R16G16B16A16_FLOAT, { colour[0], colour[1], colour[2], colour[3] } };
    D3D12_CLEAR_VALUE cv_mv = { DXGI_FORMAT_R16G16_FLOAT, { 0, 0, 0, 0 } };
    D3D12_CLEAR_VALUE cv_depth = { DXGI_FORMAT_D32_FLOAT }; cv_depth.DepthStencil.Depth = 0.5f;
    auto color = texture(dev, W, H, DXGI_FORMAT_R16G16B16A16_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET, &cv_color);
    auto mv = texture(dev, hires_mv ? OW : W, hires_mv ? OH : H, DXGI_FORMAT_R16G16_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET, &cv_mv);
    auto depth = texture(dev, W, H, DXGI_FORMAT_D32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL, D3D12_RESOURCE_STATE_DEPTH_WRITE, &cv_depth);
    auto output = texture(dev, OW, OH, DXGI_FORMAT_R16G16B16A16_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr);
    CHECK(color && mv && depth && output, "d3d12: input, motion vector, depth and output textures");
    if (!(color && mv && depth && output)) return 1;

    /* Fill the inputs. */
    D3D12_DESCRIPTOR_HEAP_DESC rtv_desc = { D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2 }, dsv_desc = { D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1 };
    ID3D12DescriptorHeap *rtvs, *dsvs;
    dev->CreateDescriptorHeap(&rtv_desc, IID_PPV_ARGS(&rtvs));
    dev->CreateDescriptorHeap(&dsv_desc, IID_PPV_ARGS(&dsvs));
    auto rtv0 = rtvs->GetCPUDescriptorHandleForHeapStart(), rtv1 = rtv0;
    rtv1.ptr += dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    auto dsv = dsvs->GetCPUDescriptorHandleForHeapStart();
    dev->CreateRenderTargetView(color, nullptr, rtv0);
    dev->CreateRenderTargetView(mv, nullptr, rtv1);
    dev->CreateDepthStencilView(depth, nullptr, dsv);
    list->ClearRenderTargetView(rtv0, colour, 0, nullptr);
    const float zero[4] = {};
    list->ClearRenderTargetView(rtv1, zero, 0, nullptr);
    list->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 0.5f, 0, 0, nullptr);

    /* DLSS through NGX, as a game calls it. */
    HMODULE m = LoadLibraryA("nvngx.dll");
    CHECK(m != nullptr, "nvngx: loaded");
    if (!m) return 1;
    auto init = ngx<NVNGX_RESULT (*)(unsigned long long, const wchar_t *, ID3D12Device *, const void *, unsigned)>(m, "NVSDK_NGX_D3D12_Init");
    auto caps = ngx<NVNGX_RESULT (*)(NVNGXParameter **)>(m, "NVSDK_NGX_D3D12_GetCapabilityParameters");
    auto alloc_params = ngx<NVNGX_RESULT (*)(NVNGXParameter **)>(m, "NVSDK_NGX_D3D12_AllocateParameters");
    auto create = ngx<NVNGX_RESULT (*)(ID3D12GraphicsCommandList *, unsigned, NVNGXParameter *, unsigned **)>(m, "NVSDK_NGX_D3D12_CreateFeature");
    auto evaluate = ngx<NVNGX_RESULT (*)(ID3D12GraphicsCommandList *, const unsigned *, NVNGXParameter *, void *)>(m, "NVSDK_NGX_D3D12_EvaluateFeature");
    auto release = ngx<NVNGX_RESULT (*)(unsigned *)>(m, "NVSDK_NGX_D3D12_ReleaseFeature");
    CHECK(init && caps && alloc_params && create && evaluate && release, "nvngx: D3D12 entry points");
    if (!(init && caps && alloc_params && create && evaluate && release)) return 1;

    NVNGXParameter *cap = nullptr, *p = nullptr;
    unsigned available = 0;
    CHECK(init(1, L"C:\\", dev, nullptr, 0x14) == NVNGX_RESULT_OK && caps(&cap) == NVNGX_RESULT_OK &&
          cap->Get("SuperSampling.Available", &available) == NVNGX_RESULT_OK && available,
          "nvngx: init, DLSS available %u", available);

    alloc_params(&p);
    p->Set(NVNGX_Parameter_Width, W);
    p->Set(NVNGX_Parameter_Height, H);
    p->Set(NVNGX_Parameter_OutWidth, OW);
    p->Set(NVNGX_Parameter_OutHeight, OH);
    p->Set(NVNGX_Parameter_PerfQualityValue, (int)NVNGX_PERFQUALITY_MAXPERF);
    p->Set(NVNGX_Parameter_DLSS_Feature_Create_Flags, (int)((hires_mv ? 0 : NVNGX_DLSS_FLAG_MV_LOWRES) | NVNGX_DLSS_FLAG_AUTO_EXPOSURE));
    unsigned *handle = nullptr;
    NVNGX_RESULT r = create(list, NVNGX_FEATURE_SUPERSAMPLING, p, &handle);
    CHECK(r == NVNGX_RESULT_OK && handle, "nvngx: CreateFeature %#x", r);
    if (!handle) return 1;

    D3D12_RESOURCE_BARRIER to_read[3] = {};
    ID3D12Resource *inputs[3] = { color, mv, depth };
    D3D12_RESOURCE_STATES before[3] = { D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_DEPTH_WRITE };
    for (int i = 0; i < 3; i++)
    {
        to_read[i].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        to_read[i].Transition = { inputs[i], D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, before[i], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE };
    }
    list->ResourceBarrier(3, to_read);

    p->Set(NVNGX_Parameter_Color, (ID3D12Resource *)color);
    p->Set(NVNGX_Parameter_Output, (ID3D12Resource *)output);
    p->Set(NVNGX_Parameter_Depth, (ID3D12Resource *)depth);
    p->Set(NVNGX_Parameter_MotionVectors, (ID3D12Resource *)mv);
    p->Set(NVNGX_Parameter_DLSS_Render_Subrect_Dimensions_Width, W);
    p->Set(NVNGX_Parameter_DLSS_Render_Subrect_Dimensions_Height, H);
    p->Set(NVNGX_Parameter_Jitter_Offset_X, 0.0f);
    p->Set(NVNGX_Parameter_Jitter_Offset_Y, 0.0f);
    p->Set(NVNGX_Parameter_Reset, 1);
    r = evaluate(list, handle, p, nullptr);
    CHECK(r == NVNGX_RESULT_OK, "nvngx: EvaluateFeature %#x", r);

    /* Read back the output's centre row. */
    D3D12_RESOURCE_BARRIER to_copy = {};
    to_copy.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    to_copy.Transition = { output, 0, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE };
    list->ResourceBarrier(1, &to_copy);
    const UINT pitch = OW * 8;
    D3D12_HEAP_PROPERTIES rb_heap = { D3D12_HEAP_TYPE_READBACK };
    D3D12_RESOURCE_DESC rb_desc = { D3D12_RESOURCE_DIMENSION_BUFFER, 0, (UINT64)pitch * OH, 1, 1, 1, DXGI_FORMAT_UNKNOWN, {1, 0}, D3D12_TEXTURE_LAYOUT_ROW_MAJOR };
    ID3D12Resource *readback = nullptr;
    dev->CreateCommittedResource(&rb_heap, D3D12_HEAP_FLAG_NONE, &rb_desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback));
    D3D12_TEXTURE_COPY_LOCATION dst = { readback, D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT }, src = { output, D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX };
    dst.PlacedFootprint.Footprint = { DXGI_FORMAT_R16G16B16A16_FLOAT, OW, OH, 1, pitch };
    src.SubresourceIndex = 0;
    list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    list->Close();
    queue->ExecuteCommandLists(1, (ID3D12CommandList *const *)&list);
    queue->Signal(fence, 1);
    HANDLE ev = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    fence->SetEventOnCompletion(1, ev);
    DWORD waited = WaitForSingleObject(ev, 10000);
    CHECK(waited == WAIT_OBJECT_0, "d3d12: GPU work finished");

    unsigned short *px = nullptr;
    readback->Map(0, nullptr, (void **)&px);
    int matching = 0, samples = 0;
    float first[4] = {};
    for (UINT x = 0; x < OW; x += 64, samples++)
    {
        unsigned short *c = px + (size_t)(OH / 2) * (pitch / 2) + x * 4;
        float v[4] = { from_half(c[0]), from_half(c[1]), from_half(c[2]), from_half(c[3]) };
        if (!x) memcpy(first, v, sizeof(v));
        matching += fabsf(v[0] - colour[0]) < 0.02f && fabsf(v[1] - colour[1]) < 0.02f && fabsf(v[2] - colour[2]) < 0.02f;
    }
    CHECK(matching == samples, "dlss: %d of %d output samples hold the input colour (first %.3f %.3f %.3f, want %.3f %.3f %.3f)",
          matching, samples, first[0], first[1], first[2], colour[0], colour[1], colour[2]);
    (void)half;

    release(handle);
    printf("%d failure(s)\n", failures);
    return failures;
}
