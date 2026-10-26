/* OverPass Runtime: 64-bit atomics (SM 6.6), end to end. Runs tests/atomic12.hlsl compiled with DXC (cs_6_6) once
 * as atomic12_split.cso (-DSPLIT: the lock-free paths), as atomic12.cso (everything, through the lock words) and
 * as atomic12_heap.cso (-DHEAP: the same with the buffer from ResourceDescriptorHeap), 16 groups of 64 threads, and
 * checks the buffer. */
#include <windows.h>
#include <d3d12.h>
#include <stdio.h>
#include <vector>

static int failures;
#define CHECK(cond, ...) do { printf("%s ", (cond) ? "ok  " : "FAIL"); printf(__VA_ARGS__); printf("\n"); if (!(cond)) failures++; } while (0)

static const UINT kSize = 128 + 1024 * 8;

static bool run(ID3D12Device *dev, const char *path, bool split, bool heap = false)
{
    std::vector<char> cso;
    if (FILE *f = fopen(path, "rb")) { fseek(f, 0, SEEK_END); cso.resize(ftell(f)); fseek(f, 0, SEEK_SET); fread(cso.data(), 1, cso.size(), f); fclose(f); }
    CHECK(!cso.empty(), "%s loaded", path);
    if (cso.empty()) return false;
    ID3D12RootSignature *rs = nullptr;
    HRESULT hr = dev->CreateRootSignature(0, cso.data(), cso.size(), IID_PPV_ARGS(&rs));
    D3D12_COMPUTE_PIPELINE_STATE_DESC pd = {};
    pd.pRootSignature = rs;
    pd.CS = { cso.data(), cso.size() };
    ID3D12PipelineState *pso = nullptr;
    if (SUCCEEDED(hr)) hr = dev->CreateComputePipelineState(&pd, IID_PPV_ARGS(&pso));
    CHECK(SUCCEEDED(hr), "root signature and pipeline hr=%#lx", hr);
    if (FAILED(hr)) return false;

    D3D12_COMMAND_QUEUE_DESC qd = { D3D12_COMMAND_LIST_TYPE_DIRECT };
    ID3D12CommandQueue *queue; ID3D12CommandAllocator *alloc; ID3D12GraphicsCommandList *list; ID3D12Fence *fence;
    dev->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue));
    dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc));
    dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc, nullptr, IID_PPV_ARGS(&list));
    dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));

    D3D12_HEAP_PROPERTIES def = { D3D12_HEAP_TYPE_DEFAULT }, up = { D3D12_HEAP_TYPE_UPLOAD }, rb = { D3D12_HEAP_TYPE_READBACK };
    D3D12_RESOURCE_DESC bd = { D3D12_RESOURCE_DIMENSION_BUFFER, 0, kSize, 1, 1, 1, DXGI_FORMAT_UNKNOWN, {1, 0}, D3D12_TEXTURE_LAYOUT_ROW_MAJOR };
    D3D12_RESOURCE_DESC ud = bd; ud.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    ID3D12Resource *upload, *buf, *readback;
    dev->CreateCommittedResource(&up, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&upload));
    dev->CreateCommittedResource(&def, D3D12_HEAP_FLAG_NONE, &ud, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&buf));
    dev->CreateCommittedResource(&rb, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback));
    UINT64 *init; upload->Map(0, nullptr, (void **)&init);
    memset(init, 0, kSize);
    init[1] = 0xffffff00;
    upload->Unmap(0, nullptr);
    list->CopyBufferRegion(buf, 0, upload, 0, kSize);
    D3D12_RESOURCE_BARRIER b = {}; b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition = { buf, 0, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS };
    list->ResourceBarrier(1, &b);
    list->SetComputeRootSignature(rs);
    list->SetPipelineState(pso);
    if (heap) {
        D3D12_DESCRIPTOR_HEAP_DESC hd = { D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE };
        ID3D12DescriptorHeap *dh;
        dev->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&dh));
        D3D12_UNORDERED_ACCESS_VIEW_DESC uav = { DXGI_FORMAT_R32_TYPELESS, D3D12_UAV_DIMENSION_BUFFER };
        uav.Buffer = { 0, kSize / 4, 0, 0, D3D12_BUFFER_UAV_FLAG_RAW };
        dev->CreateUnorderedAccessView(buf, nullptr, &uav, dh->GetCPUDescriptorHandleForHeapStart());
        list->SetDescriptorHeaps(1, &dh);
    } else {
        list->SetComputeRootUnorderedAccessView(0, buf->GetGPUVirtualAddress());
    }
    list->Dispatch(16, 1, 1);
    b.Transition = { buf, 0, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE };
    list->ResourceBarrier(1, &b);
    list->CopyBufferRegion(readback, 0, buf, 0, kSize);
    list->Close();
    queue->ExecuteCommandLists(1, (ID3D12CommandList *const *)&list);
    queue->Signal(fence, 1);
    HANDLE ev = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    fence->SetEventOnCompletion(1, ev);
    DWORD waited = WaitForSingleObject(ev, 10000);
    CHECK(waited == WAIT_OBJECT_0, "GPU work finished");
    if (waited != WAIT_OBJECT_0) return false;

    UINT64 *v; readback->Map(0, nullptr, (void **)&v);
    CHECK(v[0] == 1024 * 0x100000001ull, "add, both halves: %#llx", v[0]);
    CHECK(v[1] == 0xffffff00ull + 1024, "add with carry into the high half: %#llx (0x100000300 expected)", v[1]);
    CHECK(v[2] == ~0ull, "or: %#llx", v[2]);
    CHECK(v[3] == 1023 * 0x100000001ull, "unsigned max: %#llx", v[3]);
    CHECK(v[8] == 64ull << 31, "groupshared add: %#llx", v[8]);
    CHECK(v[9] == 0, "groupshared xor: %#llx", v[9]);
    if (!split) {
        CHECK((INT64)v[4] == -1023, "signed min: %lld", (INT64)v[4]);
        CHECK(v[5] == 1024, "compare-exchange loop counter: %llu", v[5]);
        CHECK(v[6] == 1024, "add with result: %llu", v[6]);
        UINT tickets = 0;
        for (UINT t = 0; t < 1024; t++) tickets += v[16 + t] != 0;
        CHECK(tickets == 1024, "add results unique: %u of 1024 tickets", tickets);
        CHECK(v[7] >> 32 == 1 && (v[7] & 0xffffffff) < 1024, "exchange: %#llx", v[7]);
        CHECK(v[10] == 64, "groupshared compare-exchange loop counter: %llu", v[10]);
        CHECK((INT64)v[11] == -37, "groupshared signed max: %lld", (INT64)v[11]);
    }
    return true;
}

int main(int argc, char **argv)
{
    ID3D12Device *dev = nullptr;
    D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&dev));
    printf("-- lock-free paths\n");
    run(dev, argc > 2 ? argv[2] : "atomic12_split.cso", true);
    printf("-- everything through the lock words\n");
    run(dev, argc > 1 ? argv[1] : "atomic12.cso", false);
    printf("-- the same through the descriptor heap\n");
    run(dev, argc > 3 ? argv[3] : "atomic12_heap.cso", false, true);
    printf("%d failure(s)\n", failures);
    return failures;
}
