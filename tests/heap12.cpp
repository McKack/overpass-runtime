/* OverPass Runtime: SM 6.6 descriptor heap indexing, end to end. A compute shader finds a texture, an output
 * buffer and a sampler only through ResourceDescriptorHeap / SamplerDescriptorHeap, at heap positions given in
 * root constants, and copies a texel out. Takes heap12.cso (tests/heap12.hlsl compiled with DXC, cs_6_6). */
#include <windows.h>
#include <d3d12.h>
#include <stdio.h>
#include <vector>

static int failures;
#define CHECK(cond, ...) do { printf("%s ", (cond) ? "ok  " : "FAIL"); printf(__VA_ARGS__); printf("\n"); if (!(cond)) failures++; } while (0)

int main(int argc, char **argv)
{
    std::vector<char> cso;
    if (FILE *f = fopen(argc > 1 ? argv[1] : "heap12.cso", "rb")) { fseek(f, 0, SEEK_END); cso.resize(ftell(f)); fseek(f, 0, SEEK_SET); fread(cso.data(), 1, cso.size(), f); fclose(f); }
    CHECK(!cso.empty(), "shader loaded");
    if (cso.empty()) return 1;

    ID3D12Device *dev = nullptr;
    D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&dev));
    ID3D12RootSignature *rs = nullptr;
    HRESULT hr = dev->CreateRootSignature(0, cso.data(), cso.size(), IID_PPV_ARGS(&rs));
    D3D12_COMPUTE_PIPELINE_STATE_DESC pd = {};
    pd.pRootSignature = rs;
    pd.CS = { cso.data(), cso.size() };
    ID3D12PipelineState *pso = nullptr;
    if (SUCCEEDED(hr)) hr = dev->CreateComputePipelineState(&pd, IID_PPV_ARGS(&pso));
    CHECK(SUCCEEDED(hr), "root signature and pipeline hr=%#lx", hr);
    if (FAILED(hr)) return 1;

    D3D12_COMMAND_QUEUE_DESC qd = { D3D12_COMMAND_LIST_TYPE_DIRECT };
    ID3D12CommandQueue *queue; ID3D12CommandAllocator *alloc; ID3D12GraphicsCommandList *list; ID3D12Fence *fence;
    dev->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue));
    dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc));
    dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc, nullptr, IID_PPV_ARGS(&list));
    dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));

    /* A 1x1 RGBA8 texture holding (64, 128, 192, 255), filled from an upload buffer. */
    D3D12_HEAP_PROPERTIES def = { D3D12_HEAP_TYPE_DEFAULT }, up = { D3D12_HEAP_TYPE_UPLOAD }, rb = { D3D12_HEAP_TYPE_READBACK };
    D3D12_RESOURCE_DESC td = { D3D12_RESOURCE_DIMENSION_TEXTURE2D, 0, 1, 1, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, {1, 0} };
    D3D12_RESOURCE_DESC bd = { D3D12_RESOURCE_DIMENSION_BUFFER, 0, 256, 1, 1, 1, DXGI_FORMAT_UNKNOWN, {1, 0}, D3D12_TEXTURE_LAYOUT_ROW_MAJOR };
    D3D12_RESOURCE_DESC ud = bd; ud.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    ID3D12Resource *tex, *upload, *out, *readback;
    dev->CreateCommittedResource(&def, D3D12_HEAP_FLAG_NONE, &td, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&tex));
    dev->CreateCommittedResource(&up, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&upload));
    dev->CreateCommittedResource(&def, D3D12_HEAP_FLAG_NONE, &ud, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&out));
    dev->CreateCommittedResource(&rb, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback));
    UINT32 *p; upload->Map(0, nullptr, (void **)&p); p[0] = 0xffc08040; upload->Unmap(0, nullptr);
    D3D12_TEXTURE_COPY_LOCATION dst = { tex, D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX }, src = { upload, D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT };
    src.PlacedFootprint.Footprint = { DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 1, 256 };
    list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    D3D12_RESOURCE_BARRIER b = {}; b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition = { tex, 0, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE };
    list->ResourceBarrier(1, &b);

    /* Shader-visible heaps; the resources sit at positions 5 (texture) and 9 (buffer), the sampler at 3. */
    D3D12_DESCRIPTOR_HEAP_DESC hd = { D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 16, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE };
    D3D12_DESCRIPTOR_HEAP_DESC sd = { D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 8, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE };
    ID3D12DescriptorHeap *heap, *sheap;
    dev->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&heap));
    dev->CreateDescriptorHeap(&sd, IID_PPV_ARGS(&sheap));
    UINT inc = dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV), sinc = dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    auto at = [&](ID3D12DescriptorHeap *h, UINT i, UINT step) { auto c = h->GetCPUDescriptorHandleForHeapStart(); c.ptr += i * step; return c; };
    dev->CreateShaderResourceView(tex, nullptr, at(heap, 5, inc));
    D3D12_UNORDERED_ACCESS_VIEW_DESC uav = { DXGI_FORMAT_UNKNOWN, D3D12_UAV_DIMENSION_BUFFER };
    uav.Buffer = { 0, 2, 16, 0, D3D12_BUFFER_UAV_FLAG_NONE };
    dev->CreateUnorderedAccessView(out, nullptr, &uav, at(heap, 9, inc));
    D3D12_SAMPLER_DESC smp = { D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP };
    smp.MaxLOD = D3D12_FLOAT32_MAX;
    dev->CreateSampler(&smp, at(sheap, 3, sinc));

    ID3D12DescriptorHeap *heaps[] = { heap, sheap };
    list->SetDescriptorHeaps(2, heaps);
    list->SetComputeRootSignature(rs);
    list->SetPipelineState(pso);
    UINT indices[3] = { 5, 9, 3 };
    list->SetComputeRoot32BitConstants(0, 3, indices, 0);
    list->Dispatch(1, 1, 1);
    b.Transition = { out, 0, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE };
    list->ResourceBarrier(1, &b);
    list->CopyBufferRegion(readback, 0, out, 0, 32);
    list->Close();
    queue->ExecuteCommandLists(1, (ID3D12CommandList *const *)&list);
    queue->Signal(fence, 1);
    HANDLE ev = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    fence->SetEventOnCompletion(1, ev);
    DWORD waited = WaitForSingleObject(ev, 10000);
    CHECK(waited == WAIT_OBJECT_0, "GPU work finished");

    float *v; readback->Map(0, nullptr, (void **)&v);
    auto close_to = [](float a, float b) { return a > b - 0.01f && a < b + 0.01f; };
    bool sampled = close_to(v[0], 64 / 255.f) && close_to(v[1], 128 / 255.f) && close_to(v[2], 192 / 255.f) && close_to(v[3], 1);
    bool loaded = close_to(v[4], 64 / 255.f) && close_to(v[5], 128 / 255.f) && close_to(v[6], 192 / 255.f) && close_to(v[7], 1);
    CHECK(sampled, "heap texture + heap sampler: SampleLevel gives %.3f %.3f %.3f %.3f (0.251 0.502 0.753 1 expected)", v[0], v[1], v[2], v[3]);
    CHECK(loaded, "heap texture: Load gives %.3f %.3f %.3f %.3f", v[4], v[5], v[6], v[7]);
    printf("%d failure(s)\n", failures);
    return failures;
}
