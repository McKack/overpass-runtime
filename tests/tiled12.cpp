/* OverPass Runtime: D3D12 tiled resources (tier 2) and feature level 12_0, end to end.
 * Reserved textures and buffers get their tiles from a heap at chosen offsets; data written through one
 * resource must show up in another resource mapped to the same heap tile. */
#include <windows.h>
#include <d3d12.h>
#include <stdio.h>
#include <string.h>
#include <vector>

static int failures;
#define CHECK(cond, ...) do { printf("%s ", (cond) ? "ok  " : "FAIL"); printf(__VA_ARGS__); printf("\n"); if (!(cond)) failures++; } while (0)

static ID3D12Device *dev;
static ID3D12CommandQueue *queue;
static ID3D12CommandAllocator *alloc;
static ID3D12GraphicsCommandList *list;
static ID3D12Fence *fence;
static UINT64 fence_value;

static void run_and_wait()
{
    list->Close();
    queue->ExecuteCommandLists(1, (ID3D12CommandList *const *)&list);
    queue->Signal(fence, ++fence_value);
    HANDLE ev = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    fence->SetEventOnCompletion(fence_value, ev);
    WaitForSingleObject(ev, 10000);
    CloseHandle(ev);
    alloc->Reset();
    list->Reset(alloc, nullptr);
}

static ID3D12Resource *buffer(D3D12_HEAP_TYPE type, UINT64 size, D3D12_RESOURCE_STATES state)
{
    D3D12_HEAP_PROPERTIES heap = { type };
    D3D12_RESOURCE_DESC desc = { D3D12_RESOURCE_DIMENSION_BUFFER, 0, size, 1, 1, 1, DXGI_FORMAT_UNKNOWN, {1, 0}, D3D12_TEXTURE_LAYOUT_ROW_MAJOR };
    ID3D12Resource *r = nullptr;
    dev->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, state, nullptr, IID_PPV_ARGS(&r));
    return r;
}

static ID3D12Resource *reserved_texture(UINT size, UINT16 mips)
{
    D3D12_RESOURCE_DESC desc = { D3D12_RESOURCE_DIMENSION_TEXTURE2D, 0, size, size, 1, mips, DXGI_FORMAT_R8G8B8A8_UNORM, {1, 0},
                                 D3D12_TEXTURE_LAYOUT_64KB_UNDEFINED_SWIZZLE };
    ID3D12Resource *r = nullptr;
    HRESULT hr = dev->CreateReservedResource(&desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&r));
    if (FAILED(hr)) printf("info CreateReservedResource hr=%#lx\n", hr);
    return r;
}

/* Copies a 128x128 RGBA8 tile between a texture and a buffer laid out for it. */
static void copy_tile(ID3D12Resource *tex, UINT tile_x, ID3D12Resource *buf, bool to_texture)
{
    D3D12_TEXTURE_COPY_LOCATION t = { tex, D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX }, b = { buf, D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT };
    t.SubresourceIndex = 0;
    b.PlacedFootprint.Footprint = { DXGI_FORMAT_R8G8B8A8_UNORM, 128, 128, 1, 512 };
    D3D12_BOX box = { tile_x * 128, 0, 0, tile_x * 128 + 128, 128, 1 }, src_box = { 0, 0, 0, 128, 128, 1 };
    if (to_texture) list->CopyTextureRegion(&t, tile_x * 128, 0, 0, &b, &src_box);
    else list->CopyTextureRegion(&b, 0, 0, 0, &t, &box);
}

static UINT32 first_texel(ID3D12Resource *readback)
{
    UINT32 *p = nullptr, v;
    readback->Map(0, nullptr, (void **)&p);
    v = p[128 * 64 + 64];
    readback->Unmap(0, nullptr);
    return v;
}

int main()
{
    HRESULT hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&dev));
    CHECK(SUCCEEDED(hr), "d3d12: device at feature level 12_0, hr=%#lx", hr);
    if (!dev) return 1;

    D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0, D3D_FEATURE_LEVEL_11_1 };
    D3D12_FEATURE_DATA_FEATURE_LEVELS fl = { 4, levels };
    D3D12_FEATURE_DATA_D3D12_OPTIONS o = {};
    dev->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &fl, sizeof(fl));
    dev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &o, sizeof(o));
    CHECK(fl.MaxSupportedFeatureLevel == D3D_FEATURE_LEVEL_12_0 && o.TiledResourcesTier == D3D12_TILED_RESOURCES_TIER_2 &&
          o.ResourceBindingTier >= D3D12_RESOURCE_BINDING_TIER_2 && o.TypedUAVLoadAdditionalFormats,
          "d3d12: max feature level %#x, tiled resources tier %d, binding tier %d, typed UAV loads %d",
          fl.MaxSupportedFeatureLevel, o.TiledResourcesTier, o.ResourceBindingTier, o.TypedUAVLoadAdditionalFormats);
    if (o.TiledResourcesTier < D3D12_TILED_RESOURCES_TIER_2) return 1;

    D3D12_COMMAND_QUEUE_DESC qd = { D3D12_COMMAND_LIST_TYPE_DIRECT };
    dev->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue));
    dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc));
    dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc, nullptr, IID_PPV_ARGS(&list));
    dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));

    /* Tiling of a full-chain 1024x1024 RGBA8 texture: 64 + 16 + 4 + 1 standard tiles, then the packed mips. */
    auto tex = reserved_texture(1024, 11);
    CHECK(tex != nullptr, "d3d12: reserved 1024x1024 RGBA8 texture with 11 mips");
    if (!tex) return 1;
    UINT total = 0, sub_count = 11;
    D3D12_PACKED_MIP_INFO packed = {};
    D3D12_TILE_SHAPE shape = {};
    D3D12_SUBRESOURCE_TILING subs[11] = {};
    dev->GetResourceTiling(tex, &total, &packed, &shape, &sub_count, 0, subs);
    CHECK(shape.WidthInTexels == 128 && shape.HeightInTexels == 128 && shape.DepthInTexels == 1, "tiling: tile shape %ux%ux%u (128x128x1 expected)",
          shape.WidthInTexels, shape.HeightInTexels, shape.DepthInTexels);
    CHECK(packed.NumStandardMips == 4 && packed.NumPackedMips == 7 && subs[0].WidthInTiles == 8 && subs[3].WidthInTiles == 1 &&
          subs[4].StartTileIndexInOverallResource == D3D12_PACKED_TILE && packed.StartTileIndexInOverallResource == 85 &&
          total == 85 + packed.NumTilesForPackedMips,
          "tiling: %u tiles, %u standard + %u packed mips in %u tiles from tile %u", total, packed.NumStandardMips,
          packed.NumPackedMips, packed.NumTilesForPackedMips, packed.StartTileIndexInOverallResource);

    /* A heap of 16 tiles; map tile (0,0) of mip 0 to heap tile 3 and tile (1,0) to heap tile 7. */
    D3D12_HEAP_DESC hd = { 16 * 65536, { D3D12_HEAP_TYPE_DEFAULT }, 65536, D3D12_HEAP_FLAG_DENY_BUFFERS | D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES };
    ID3D12Heap *heap = nullptr;
    dev->CreateHeap(&hd, IID_PPV_ARGS(&heap));
    D3D12_TILED_RESOURCE_COORDINATE coords[2] = { {0, 0, 0, 0}, {1, 0, 0, 0} };
    D3D12_TILE_REGION_SIZE one = { 1, FALSE, 0, 0, 0 }, sizes[2] = { one, one };
    UINT offsets[2] = { 3, 7 }, counts[2] = { 1, 1 };
    queue->UpdateTileMappings(tex, 2, coords, sizes, heap, 2, nullptr, offsets, counts, D3D12_TILE_MAPPING_FLAG_NONE);

    /* Fill the two tiles with different colours through the texture, then read them back. */
    auto upload_red = buffer(D3D12_HEAP_TYPE_UPLOAD, 65536, D3D12_RESOURCE_STATE_GENERIC_READ);
    auto upload_green = buffer(D3D12_HEAP_TYPE_UPLOAD, 65536, D3D12_RESOURCE_STATE_GENERIC_READ);
    auto readback = buffer(D3D12_HEAP_TYPE_READBACK, 65536, D3D12_RESOURCE_STATE_COPY_DEST);
    UINT32 *p;
    upload_red->Map(0, nullptr, (void **)&p); for (int i = 0; i < 16384; i++) p[i] = 0xff0000ff; upload_red->Unmap(0, nullptr);
    upload_green->Map(0, nullptr, (void **)&p); for (int i = 0; i < 16384; i++) p[i] = 0xff00ff00; upload_green->Unmap(0, nullptr);
    copy_tile(tex, 0, upload_red, true);
    copy_tile(tex, 1, upload_green, true);
    run_and_wait();
    copy_tile(tex, 1, readback, false);
    run_and_wait();
    UINT32 v = first_texel(readback);
    CHECK(v == 0xff00ff00, "mapping: mapped tile holds what was written (%08x)", v);

    /* A second reserved texture whose tile (0,0) uses heap tile 3 sees the first texture's red tile. */
    auto alias = reserved_texture(1024, 1);
    D3D12_TILED_RESOURCE_COORDINATE origin = { 0, 0, 0, 0 };
    UINT three = 3, one_tile = 1;
    queue->UpdateTileMappings(alias, 1, &origin, &one, heap, 1, nullptr, &three, &one_tile, D3D12_TILE_MAPPING_FLAG_NONE);
    copy_tile(alias, 0, readback, false);
    run_and_wait();
    v = first_texel(readback);
    CHECK(v == 0xff0000ff, "aliasing: another resource mapped to the same heap tile reads it (%08x)", v);

    /* CopyTileMappings: the alias's tile (1,0) takes the first texture's tile (1,0), the green one. */
    D3D12_TILED_RESOURCE_COORDINATE second = { 1, 0, 0, 0 };
    queue->CopyTileMappings(alias, &second, tex, &second, &one, D3D12_TILE_MAPPING_FLAG_NONE);
    copy_tile(alias, 1, readback, false);
    run_and_wait();
    v = first_texel(readback);
    CHECK(v == 0xff00ff00, "copy mappings: copied tile reads the source's memory (%08x)", v);

    /* Ordinary placed resources still work in heaps that can back tiles. */
    D3D12_RESOURCE_DESC pdesc = { D3D12_RESOURCE_DIMENSION_TEXTURE2D, 0, 128, 128, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, {1, 0} };
    ID3D12Resource *placed = nullptr;
    hr = dev->CreatePlacedResource(heap, 8 * 65536, &pdesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&placed));
    if (placed)
    {
        copy_tile(placed, 0, upload_green, true);
        run_and_wait();
        copy_tile(placed, 0, readback, false);
        run_and_wait();
    }
    v = placed ? first_texel(readback) : 0;
    CHECK(SUCCEEDED(hr) && v == 0xff00ff00, "placed: texture placed in the same heap, hr=%#lx (%08x)", hr, v);

    /* Reserved buffer: tile 2 mapped to heap tile 5, written and read back. */
    D3D12_HEAP_DESC bhd = { 8 * 65536, { D3D12_HEAP_TYPE_DEFAULT }, 65536, D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS };
    ID3D12Heap *bheap = nullptr;
    dev->CreateHeap(&bhd, IID_PPV_ARGS(&bheap));
    D3D12_RESOURCE_DESC bdesc = { D3D12_RESOURCE_DIMENSION_BUFFER, 0, 4 * 65536, 1, 1, 1, DXGI_FORMAT_UNKNOWN, {1, 0}, D3D12_TEXTURE_LAYOUT_ROW_MAJOR };
    ID3D12Resource *rbuf = nullptr;
    hr = dev->CreateReservedResource(&bdesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&rbuf));
    UINT btotal = 0;
    if (rbuf) dev->GetResourceTiling(rbuf, &btotal, nullptr, nullptr, nullptr, 0, nullptr);
    CHECK(SUCCEEDED(hr) && rbuf && btotal == 4, "d3d12: reserved 256 KB buffer, %u tiles", btotal);
    if (rbuf)
    {
        D3D12_TILED_RESOURCE_COORDINATE t2 = { 2, 0, 0, 0 };
        UINT five = 5;
        queue->UpdateTileMappings(rbuf, 1, &t2, &one, bheap, 1, nullptr, &five, &one_tile, D3D12_TILE_MAPPING_FLAG_NONE);
        list->CopyBufferRegion(rbuf, 2 * 65536, upload_green, 0, 65536);
        run_and_wait();
        list->CopyBufferRegion(readback, 0, rbuf, 2 * 65536, 65536);
        run_and_wait();
        readback->Map(0, nullptr, (void **)&p);
        v = p[1000];
        readback->Unmap(0, nullptr);
        CHECK(v == 0xff00ff00, "buffer: mapped tile holds what was written (%08x)", v);
    }

    printf("%d failure(s)\n", failures);
    return failures;
}
