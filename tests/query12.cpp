/* OverPass Runtime: D3D12 occlusion and timestamp queries. Draws rectangles with a depth test into a 64x64
 * target and checks the sample counts: a query that sees nothing, overlapping queries, a query that spans two
 * render passes, binary results, results resolved again from a later command list; then that timestamps are
 * ordered and on the same clock as GetClockCalibration. Takes query12_vs.cso and query12_ps.cso
 * (tests/query12.hlsl compiled with DXC, -E vs_main -T vs_6_0 and -E ps_main -T ps_6_0). */
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

static ID3D12GraphicsCommandList *list;

static void draw(float x0, float y0, float x1, float y1, float z)
{
    float c[5] = { x0, y0, x1, y1, z };
    list->SetGraphicsRoot32BitConstants(0, 5, c, 0);
    list->DrawInstanced(6, 1, 0, 0);
}

int main(int argc, char **argv)
{
    auto vs = load("query12_vs.cso"), ps = load("query12_ps.cso");
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
    pd.DepthStencilState.DepthEnable = TRUE;
    pd.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    pd.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    pd.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pd.NumRenderTargets = 1;
    pd.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pd.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    pd.SampleDesc.Count = 1;
    ID3D12PipelineState *pso = nullptr;
    if (SUCCEEDED(hr)) hr = dev->CreateGraphicsPipelineState(&pd, IID_PPV_ARGS(&pso));
    CHECK(SUCCEEDED(hr), "root signature and pipeline hr=%#lx", hr);
    if (FAILED(hr)) return 1;

    D3D12_COMMAND_QUEUE_DESC qd = { D3D12_COMMAND_LIST_TYPE_DIRECT };
    ID3D12CommandQueue *queue; ID3D12CommandAllocator *alloc; ID3D12Fence *fence;
    dev->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue));
    dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc));
    dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc, nullptr, IID_PPV_ARGS(&list));
    dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));

    D3D12_QUERY_HEAP_DESC ohd = { D3D12_QUERY_HEAP_TYPE_OCCLUSION, 8 }, thd = { D3D12_QUERY_HEAP_TYPE_TIMESTAMP, 4 };
    ID3D12QueryHeap *occlusion = nullptr, *timestamps = nullptr;
    hr = dev->CreateQueryHeap(&ohd, IID_PPV_ARGS(&occlusion));
    if (SUCCEEDED(hr)) hr = dev->CreateQueryHeap(&thd, IID_PPV_ARGS(&timestamps));
    CHECK(SUCCEEDED(hr), "query heaps hr=%#lx", hr);
    if (FAILED(hr)) return 1;

    D3D12_HEAP_PROPERTIES def = { D3D12_HEAP_TYPE_DEFAULT }, rb = { D3D12_HEAP_TYPE_READBACK };
    D3D12_RESOURCE_DESC td = { D3D12_RESOURCE_DIMENSION_TEXTURE2D, 0, 64, 64, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, {1, 0} };
    td.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    D3D12_RESOURCE_DESC dd = td;
    dd.Format = DXGI_FORMAT_D32_FLOAT;
    dd.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    D3D12_RESOURCE_DESC bd = { D3D12_RESOURCE_DIMENSION_BUFFER, 0, 4096, 1, 1, 1, DXGI_FORMAT_UNKNOWN, {1, 0}, D3D12_TEXTURE_LAYOUT_ROW_MAJOR };
    D3D12_CLEAR_VALUE dc = { DXGI_FORMAT_D32_FLOAT };
    dc.DepthStencil.Depth = 1;
    ID3D12Resource *rt, *depth, *readback;
    dev->CreateCommittedResource(&def, D3D12_HEAP_FLAG_NONE, &td, D3D12_RESOURCE_STATE_RENDER_TARGET, nullptr, IID_PPV_ARGS(&rt));
    dev->CreateCommittedResource(&def, D3D12_HEAP_FLAG_NONE, &dd, D3D12_RESOURCE_STATE_DEPTH_WRITE, &dc, IID_PPV_ARGS(&depth));
    dev->CreateCommittedResource(&rb, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback));
    D3D12_DESCRIPTOR_HEAP_DESC hd = { D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1 }, dhd = { D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1 };
    ID3D12DescriptorHeap *rtvs, *dsvs;
    dev->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&rtvs));
    dev->CreateDescriptorHeap(&dhd, IID_PPV_ARGS(&dsvs));
    auto rtv = rtvs->GetCPUDescriptorHandleForHeapStart(), dsv = dsvs->GetCPUDescriptorHandleForHeapStart();
    dev->CreateRenderTargetView(rt, nullptr, rtv);
    dev->CreateDepthStencilView(depth, nullptr, dsv);

    UINT64 frequency = 0, gpu_before = 0, cpu_before = 0, gpu_after = 0, cpu_after = 0;
    queue->GetTimestampFrequency(&frequency);
    queue->GetClockCalibration(&gpu_before, &cpu_before);

    list->EndQuery(timestamps, D3D12_QUERY_TYPE_TIMESTAMP, 0);
    list->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1, 0, 0, nullptr);
    list->SetGraphicsRootSignature(rs);
    list->SetPipelineState(pso);
    list->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
    D3D12_VIEWPORT vp = { 0, 0, 64, 64, 0, 1 };
    D3D12_RECT sc = { 0, 0, 64, 64 };
    list->RSSetViewports(1, &vp);
    list->RSSetScissorRects(1, &sc);
    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 0: the left half (2048 samples). 1: the same half behind it (0). 2: nothing drawn (0).
    list->BeginQuery(occlusion, D3D12_QUERY_TYPE_OCCLUSION, 0);
    draw(-1, -1, 0, 1, 0.5f);
    list->EndQuery(occlusion, D3D12_QUERY_TYPE_OCCLUSION, 0);
    list->BeginQuery(occlusion, D3D12_QUERY_TYPE_OCCLUSION, 1);
    draw(-1, -1, 0, 1, 0.7f);
    list->EndQuery(occlusion, D3D12_QUERY_TYPE_OCCLUSION, 1);
    list->BeginQuery(occlusion, D3D12_QUERY_TYPE_OCCLUSION, 2);
    list->EndQuery(occlusion, D3D12_QUERY_TYPE_OCCLUSION, 2);
    // 3 and 4 overlap: 3 sees the top right quarter and the bottom right one (2048), 4 the bottom right quarter
    // and then the whole target in front (1024 + 4096).
    list->BeginQuery(occlusion, D3D12_QUERY_TYPE_OCCLUSION, 3);
    draw(0, 0, 1, 1, 0.5f);
    list->BeginQuery(occlusion, D3D12_QUERY_TYPE_OCCLUSION, 4);
    draw(0, -1, 1, 0, 0.5f);
    list->EndQuery(occlusion, D3D12_QUERY_TYPE_OCCLUSION, 3);
    draw(-1, -1, 1, 1, 0.1f);
    list->EndQuery(occlusion, D3D12_QUERY_TYPE_OCCLUSION, 4);
    // 5 spans two render passes (a timestamp ends the first): the whole target twice (8192).
    list->BeginQuery(occlusion, D3D12_QUERY_TYPE_OCCLUSION, 5);
    draw(-1, -1, 1, 1, 0.05f);
    list->EndQuery(timestamps, D3D12_QUERY_TYPE_TIMESTAMP, 1);
    draw(-1, -1, 1, 1, 0.02f);
    list->EndQuery(occlusion, D3D12_QUERY_TYPE_OCCLUSION, 5);
    // 6: a binary occlusion query on a quarter (1).
    list->BeginQuery(occlusion, D3D12_QUERY_TYPE_BINARY_OCCLUSION, 6);
    draw(-1, -1, 0, 0, 0.01f);
    list->EndQuery(occlusion, D3D12_QUERY_TYPE_BINARY_OCCLUSION, 6);
    list->EndQuery(timestamps, D3D12_QUERY_TYPE_TIMESTAMP, 2);

    list->ResolveQueryData(occlusion, D3D12_QUERY_TYPE_OCCLUSION, 0, 6, readback, 0);
    list->ResolveQueryData(occlusion, D3D12_QUERY_TYPE_BINARY_OCCLUSION, 0, 7, readback, 256);
    list->ResolveQueryData(timestamps, D3D12_QUERY_TYPE_TIMESTAMP, 0, 3, readback, 512);
    list->Close();
    queue->ExecuteCommandLists(1, (ID3D12CommandList *const *)&list);
    queue->Signal(fence, 1);
    HANDLE ev = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    fence->SetEventOnCompletion(1, ev);
    DWORD waited = WaitForSingleObject(ev, 10000);
    CHECK(waited == WAIT_OBJECT_0, "GPU work finished");
    queue->GetClockCalibration(&gpu_after, &cpu_after);

    // A later command list resolves the same queries again.
    alloc->Reset();
    list->Reset(alloc, nullptr);
    list->ResolveQueryData(occlusion, D3D12_QUERY_TYPE_OCCLUSION, 0, 6, readback, 1024);
    list->Close();
    queue->ExecuteCommandLists(1, (ID3D12CommandList *const *)&list);
    queue->Signal(fence, 2);
    fence->SetEventOnCompletion(2, ev);
    waited = WaitForSingleObject(ev, 10000);
    CHECK(waited == WAIT_OBJECT_0, "second resolve finished");

    UINT64 *r; readback->Map(0, nullptr, (void **)&r);
    const UINT64 expected[6] = { 2048, 0, 0, 2048, 5120, 8192 };
    for (int i = 0; i < 6; i++)
        CHECK(r[i] == expected[i], "occlusion query %d: %llu samples (%llu expected)", i, r[i], expected[i]);
    const UINT64 binary[7] = { 1, 0, 0, 1, 1, 1, 1 };
    for (int i = 0; i < 7; i++)
        CHECK(r[32 + i] == binary[i], "binary result %d: %llu (%llu expected)", i, r[32 + i], binary[i]);
    for (int i = 0; i < 6; i++)
        CHECK(r[128 + i] == expected[i], "resolved again later %d: %llu", i, r[128 + i]);

    UINT64 *t = r + 64;
    printf("     frequency %llu, calibration gpu %llu..%llu cpu %llu..%llu, timestamps %llu %llu %llu\n", frequency,
           gpu_before, gpu_after, cpu_before, cpu_after, t[0], t[1], t[2]);
    CHECK(t[0] <= t[1] && t[1] <= t[2], "timestamps in order");
    CHECK(t[0] >= gpu_before && t[2] <= gpu_after, "timestamps between the clock calibrations before and after");
    CHECK(frequency && gpu_after - gpu_before < frequency, "calibrations less than 1 s apart on the GPU clock");
    printf("%d failure(s)\n", failures);
    return failures;
}
