// tests/gs12.cpp: a geometry shader between vertex and pixel shader (SM 6.0).
#define RS "RootFlags(0)"
struct V { float4 pos : SV_Position; nointerpolation float id : ID; };
struct G { float4 pos : SV_Position; nointerpolation float val : VAL; nointerpolation uint prim : PRIM; };
[RootSignature(RS)]
V vs_main(uint vid : SV_VertexID)
{
    // Two triangles covering the target, the second drawn over the first; vertex ids 1 to 6.
    float2 p[3] = { float2(-1, -1), float2(-1, 3), float2(3, -1) };
    V v; v.pos = float4(p[vid % 3], 0, 1); v.id = vid + 1;
    return v;
}
[RootSignature(RS)] [maxvertexcount(3)]
void gs_main(triangle V v[3], uint prim : SV_PrimitiveID, inout TriangleStream<G> stream)
{
    for (uint i = 0; i < 3; i++) {
        G g; g.pos = v[i].pos; g.val = v[0].id + 10 * v[1].id + 100 * v[2].id; g.prim = prim;
        stream.Append(g);
    }
}
[RootSignature(RS)]
float4 ps_main(G g) : SV_Target
{
    return float4(g.val, g.prim, 0, 1);
}
