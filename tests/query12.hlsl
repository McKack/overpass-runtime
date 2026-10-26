// tests/query12.cpp: a rectangle at a given depth, so occlusion queries have known sample counts.
#define RS "RootConstants(num32BitConstants=5, b0)"
struct Rect { float4 bounds; float z; };
ConstantBuffer<Rect> rect : register(b0);
[RootSignature(RS)]
float4 vs_main(uint vid : SV_VertexID) : SV_Position
{
    // Two triangles covering bounds (x0, y0, x1, y1 in clip space).
    uint corner[6] = { 0, 1, 2, 2, 1, 3 };
    uint c = corner[vid];
    return float4(c & 1 ? rect.bounds.z : rect.bounds.x, c & 2 ? rect.bounds.w : rect.bounds.y, rect.z, 1);
}
[RootSignature(RS)]
float4 ps_main() : SV_Target
{
    return float4(1, 1, 1, 1);
}
