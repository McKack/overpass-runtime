/* OverPass Runtime: SV_Barycentrics and GetAttributeAtVertex, end to end. Draws one triangle into an 8x8 float
 * target and checks the bottom-left pixel: the weights, each vertex's own attribute value, and the provoking
 * vertex for flat interpolation. Takes bary12_vs.cso and bary12_ps.cso (tests/bary12.hlsl compiled with DXC,
 * -E vs_main -T vs_6_1 and -E ps_main -T ps_6_1). */
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

int main(int argc, char **argv)
{
    auto vs = load(argc > 2 ? argv[1] : "bary12_vs.cso"), ps = load(argc > 2 ? argv[2] : "bary12_ps.cso");
    CHECK(!vs.empty() && !ps.empty(), "shaders loaded");
    if (vs.empty() || ps.empty()) return 1;

    ID3D12Device *dev = nullptr;
    D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&dev));
    ID3D12RootSignature *rs = nullptr;
    HRESULT hr = dev->CreateRootSignature(0, ps.data(), ps.size(), IID_PPV_ARGS(&rs));
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pd = {};
    pd.pRootSignature = rs;
    pd.VS = { vs.data(), vs.size() };
    pd.PS = { ps.data(), ps.size() };
    pd.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    pd.SampleMask = ~0u;
    pd.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pd.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pd.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pd.NumRenderTargets = 1;
    pd.RTVFormats[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;
    pd.SampleDesc.Count = 1;
    ID3D12PipelineState *pso = nullptr;
    if (SUCCEEDED(hr)) hr = dev->CreateGraphicsPipelineState(&pd, IID_PPV_ARGS(&pso));
    CHECK(SUCCEEDED(hr), "root signature and pipeline hr=%#lx", hr);
    if (FAILED(hr)) return 1;

    D3D12_COMMAND_QUEUE_DESC qd = { D3D12_COMMAND_LIST_TYPE_DIRECT };
    ID3D12CommandQueue *queue; ID3D12CommandAllocator *alloc; ID3D12GraphicsCommandList *list; ID3D12Fence *fence;
    dev->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue));
    dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc));
    dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc, nullptr, IID_PPV_ARGS(&list));
    dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));

    D3D12_HEAP_PROPERTIES def = { D3D12_HEAP_TYPE_DEFAULT }, rb = { D3D12_HEAP_TYPE_READBACK };
    D3D12_RESOURCE_DESC td = { D3D12_RESOURCE_DIMENSION_TEXTURE2D, 0, 8, 8, 1, 1, DXGI_FORMAT_R32G32B32A32_FLOAT, {1, 0} };
    td.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    D3D12_RESOURCE_DESC bd = { D3D12_RESOURCE_DIMENSION_BUFFER, 0, 8 * 256, 1, 1, 1, DXGI_FORMAT_UNKNOWN, {1, 0}, D3D12_TEXTURE_LAYOUT_ROW_MAJOR };
    ID3D12Resource *rt, *readback;
    dev->CreateCommittedResource(&def, D3D12_HEAP_FLAG_NONE, &td, D3D12_RESOURCE_STATE_RENDER_TARGET, nullptr, IID_PPV_ARGS(&rt));
    dev->CreateCommittedResource(&rb, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback));
    D3D12_DESCRIPTOR_HEAP_DESC hd = { D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1 };
    ID3D12DescriptorHeap *rtvs;
    dev->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&rtvs));
    auto rtv = rtvs->GetCPUDescriptorHandleForHeapStart();
    dev->CreateRenderTargetView(rt, nullptr, rtv);

    list->SetGraphicsRootSignature(rs);
    list->SetPipelineState(pso);
    list->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    D3D12_VIEWPORT vp = { 0, 0, 8, 8, 0, 1 };
    D3D12_RECT sc = { 0, 0, 8, 8 };
    list->RSSetViewports(1, &vp);
    list->RSSetScissorRects(1, &sc);
    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    list->DrawInstanced(3, 1, 0, 0);
    D3D12_RESOURCE_BARRIER b = {}; b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition = { rt, 0, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE };
    list->ResourceBarrier(1, &b);
    D3D12_TEXTURE_COPY_LOCATION dst = { readback, D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT }, src = { rt, D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX };
    dst.PlacedFootprint.Footprint = { DXGI_FORMAT_R32G32B32A32_FLOAT, 8, 8, 1, 256 };
    list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    list->Close();
    queue->ExecuteCommandLists(1, (ID3D12CommandList *const *)&list);
    queue->Signal(fence, 1);
    HANDLE ev = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    fence->SetEventOnCompletion(1, ev);
    DWORD waited = WaitForSingleObject(ev, 10000);
    CHECK(waited == WAIT_OBJECT_0, "GPU work finished");

    /* Bottom-left pixel (0, 7): centre at NDC (-0.875, -0.875), so weights v0 0.9375, v1 0.03125, v2 0.03125. */
    char *base; readback->Map(0, nullptr, (void **)&base);
    float *v = (float *)(base + 7 * 256);
    auto close_to = [](float a, float b) { return a > b - 0.001f && a < b + 0.001f; };
    CHECK(close_to(v[0], 0.9375f) && close_to(v[1], 0.03125f), "SV_Barycentrics weights %.5f %.5f (0.93750 0.03125 expected)", v[0], v[1]);
    CHECK(close_to(v[2], 0), "interpolated value equals the barycentric blend of the vertex values (difference %.5f)", v[2]);
    CHECK(close_to(v[3], 1321), "flat value and GetAttributeAtVertex 0, 1, 2: %.1f (1321 expected: provoking vertex 1, then 1, 2, 3)", v[3]);
    printf("%d failure(s)\n", failures);
    return failures;
}
