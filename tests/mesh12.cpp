/* OverPass Runtime: mesh and amplification shaders (SM 6.5), end to end. Draws tests/mesh12.hlsl (see there) with
 * DispatchMesh inside a render pass and checks the pixels. Takes ms_basic.cso, as_main.cso, ms_amp.cso and
 * ps_main.cso (DXC: ms_6_5, as_6_5, ms_6_5, ps_6_5 with -E of the same name). */
#include <windows.h>
#include <d3d12.h>
#include <stdio.h>
#include <vector>

static int failures;
#define CHECK(cond, ...) do { printf("%s ", (cond) ? "ok  " : "FAIL"); printf(__VA_ARGS__); printf("\n"); if (!(cond)) failures++; } while (0)

static std::vector<char> load(const char *path)
{
    std::vector<char> data;
    if (FILE *f = fopen(path, "rb")) { fseek(f, 0, SEEK_END); data.resize(ftell(f)); fseek(f, 0, SEEK_SET); fread(data.data(), 1, data.size(), f); fclose(f); }
    return data;
}

template <typename T, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE Type> struct alignas(void *) Sub {
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE type = Type;
    T data;
};

struct MeshStream {
    Sub<ID3D12RootSignature *, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE> rs;
    Sub<D3D12_SHADER_BYTECODE, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS> as;
    Sub<D3D12_SHADER_BYTECODE, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS> ms;
    Sub<D3D12_SHADER_BYTECODE, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS> ps;
    Sub<D3D12_RT_FORMAT_ARRAY, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS> rtv;
    Sub<D3D12_RASTERIZER_DESC, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER> raster;
    Sub<D3D12_DEPTH_STENCIL_DESC, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL> depth;
};

int main(int argc, char **argv)
{
    auto ms_basic = load("ms_basic.cso"), as_main = load("as_main.cso"), ms_amp = load("ms_amp.cso"), ps_main = load("ps_main.cso");
    CHECK(!ms_basic.empty() && !as_main.empty() && !ms_amp.empty() && !ps_main.empty(), "shaders loaded");
    if (failures) return 1;

    ID3D12Device2 *dev = nullptr;
    D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&dev));
    D3D12_FEATURE_DATA_D3D12_OPTIONS7 o7 = {};
    dev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &o7, sizeof(o7));
    CHECK(o7.MeshShaderTier >= D3D12_MESH_SHADER_TIER_1, "mesh shader tier %d", o7.MeshShaderTier);

    ID3D12RootSignature *rs = nullptr;
    HRESULT hr = dev->CreateRootSignature(0, ps_main.data(), ps_main.size(), IID_PPV_ARGS(&rs));
    CHECK(SUCCEEDED(hr), "root signature hr=%#lx", hr);

    MeshStream stream = {};
    stream.rs.data = rs;
    stream.ps.data = { ps_main.data(), ps_main.size() };
    stream.rtv.data.NumRenderTargets = 1;
    stream.rtv.data.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    stream.raster.data = { D3D12_FILL_MODE_SOLID, D3D12_CULL_MODE_NONE };
    stream.raster.data.DepthClipEnable = TRUE;
    stream.depth.data.DepthEnable = FALSE;
    ID3D12PipelineState *pso_basic = nullptr, *pso_amp = nullptr;
    stream.ms.data = { ms_basic.data(), ms_basic.size() };
    D3D12_PIPELINE_STATE_STREAM_DESC sd = { sizeof(stream), &stream };
    hr = dev->CreatePipelineState(&sd, IID_PPV_ARGS(&pso_basic));
    CHECK(SUCCEEDED(hr), "mesh pipeline hr=%#lx", hr);
    stream.as.data = { as_main.data(), as_main.size() };
    stream.ms.data = { ms_amp.data(), ms_amp.size() };
    hr = dev->CreatePipelineState(&sd, IID_PPV_ARGS(&pso_amp));
    CHECK(SUCCEEDED(hr), "amplification + mesh pipeline hr=%#lx", hr);
    if (failures) return 1;

    D3D12_COMMAND_QUEUE_DESC qd = { D3D12_COMMAND_LIST_TYPE_DIRECT };
    ID3D12CommandQueue *queue; ID3D12CommandAllocator *alloc; ID3D12GraphicsCommandList *list0; ID3D12Fence *fence;
    dev->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue));
    dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc));
    dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc, nullptr, IID_PPV_ARGS(&list0));
    dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    ID3D12GraphicsCommandList6 *list = nullptr;
    hr = list0->QueryInterface(IID_PPV_ARGS(&list));
    CHECK(SUCCEEDED(hr), "ID3D12GraphicsCommandList6 hr=%#lx", hr);
    if (failures) return 1;

    D3D12_HEAP_PROPERTIES def = { D3D12_HEAP_TYPE_DEFAULT }, rb = { D3D12_HEAP_TYPE_READBACK };
    D3D12_RESOURCE_DESC td = { D3D12_RESOURCE_DIMENSION_TEXTURE2D, 0, 64, 64, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, {1, 0} };
    td.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    D3D12_RESOURCE_DESC bd = { D3D12_RESOURCE_DIMENSION_BUFFER, 0, 256 * 64, 1, 1, 1, DXGI_FORMAT_UNKNOWN, {1, 0}, D3D12_TEXTURE_LAYOUT_ROW_MAJOR };
    ID3D12Resource *rt, *readback;
    dev->CreateCommittedResource(&def, D3D12_HEAP_FLAG_NONE, &td, D3D12_RESOURCE_STATE_RENDER_TARGET, nullptr, IID_PPV_ARGS(&rt));
    dev->CreateCommittedResource(&rb, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback));
    D3D12_DESCRIPTOR_HEAP_DESC hd = { D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1 };
    ID3D12DescriptorHeap *rtv_heap;
    dev->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&rtv_heap));
    auto rtv = rtv_heap->GetCPUDescriptorHandleForHeapStart();
    dev->CreateRenderTargetView(rt, nullptr, rtv);

    D3D12_RENDER_PASS_RENDER_TARGET_DESC pass_rt = { rtv };
    pass_rt.BeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
    pass_rt.BeginningAccess.Clear.ClearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    pass_rt.EndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
    list->BeginRenderPass(1, &pass_rt, nullptr, D3D12_RENDER_PASS_FLAG_NONE);
    D3D12_VIEWPORT vp = { 0, 0, 64, 64, 0, 1 };
    D3D12_RECT sc = { 0, 0, 64, 64 };
    list->RSSetViewports(1, &vp);
    list->RSSetScissorRects(1, &sc);
    list->SetGraphicsRootSignature(rs);
    float scale = 1.0f;
    list->SetGraphicsRoot32BitConstants(0, 1, &scale, 0);
    list->SetPipelineState(pso_basic);
    list->DispatchMesh(1, 1, 1);
    list->SetPipelineState(pso_amp);
    list->DispatchMesh(1, 1, 1);
    list->EndRenderPass();

    D3D12_RESOURCE_BARRIER b = {}; b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition = { rt, 0, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE };
    list->ResourceBarrier(1, &b);
    D3D12_TEXTURE_COPY_LOCATION dst = { readback, D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT }, src = { rt, D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX };
    dst.PlacedFootprint.Footprint = { DXGI_FORMAT_R8G8B8A8_UNORM, 64, 64, 1, 256 };
    list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    list->Close();
    queue->ExecuteCommandLists(1, (ID3D12CommandList *const *)&list0);
    queue->Signal(fence, 1);
    HANDLE ev = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    fence->SetEventOnCompletion(1, ev);
    DWORD waited = WaitForSingleObject(ev, 10000);
    CHECK(waited == WAIT_OBJECT_0, "GPU work finished");
    if (waited != WAIT_OBJECT_0) return 1;

    UINT8 *px; readback->Map(0, nullptr, (void **)&px);
    auto at = [&](int x, int y) { return px + y * 256 + x * 4; };
    auto is = [&](int x, int y, int r, int g, int bl, int a0, int a1) {
        UINT8 *p = at(x, y);
        return p[0] == r && p[1] == g && p[2] == bl && (p[3] == a0 || p[3] == a1);
    };
    auto show = [&](int x, int y) { static char s[32]; UINT8 *p = at(x, y); sprintf(s, "%d %d %d %d", p[0], p[1], p[2], p[3]); return s; };
    CHECK(is(16, 16, 255, 0, 0, 0, 1), "top left: red, primitive 0 or 1: %s", show(16, 16));
    CHECK(is(48, 16, 0, 255, 0, 2, 3), "top right: green, primitive 2 or 3: %s", show(48, 16));
    CHECK(is(16, 48, 255, 255, 0, 10, 10), "bottom left: yellow from the payload, mesh group 0: %s", show(16, 48));
    CHECK(is(48, 48, 0, 255, 255, 11, 11), "bottom right: cyan from the payload, mesh group 1: %s", show(48, 48));
    int blue = 0;
    for (int y = 0; y < 64; y++) for (int x = 0; x < 64; x++) blue += at(x, y)[2] == 255 && at(x, y)[1] == 0;
    CHECK(blue == 0, "culled primitive left no blue pixels (%d)", blue);
    printf("%d failure(s)\n", failures);
    return failures;
}
