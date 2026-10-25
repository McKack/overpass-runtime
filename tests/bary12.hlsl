// tests/bary12.cpp: SV_Barycentrics and GetAttributeAtVertex (SM 6.1).
#define RS "RootFlags(0)"
struct V { float4 pos : SV_Position; nointerpolation float id : ID; float lin : LIN; };
[RootSignature(RS)]
V vs_main(uint vid : SV_VertexID)
{
    // A triangle covering the target: v0 bottom left, v1 above it, v2 to its right.
    float2 p[3] = { float2(-1, -1), float2(-1, 3), float2(3, -1) };
    V v; v.pos = float4(p[vid], 0, 1); v.id = vid + 1; v.lin = vid + 1;
    return v;
}
[RootSignature(RS)]
float4 ps_main(V v, float3 b : SV_Barycentrics) : SV_Target
{
    float3 at = float3(GetAttributeAtVertex(v.id, 0), GetAttributeAtVertex(v.id, 1), GetAttributeAtVertex(v.id, 2));
    // x, y: weights of v0 and v1; z: interpolated value minus the barycentric blend (0); w: 1000 * flat + vertex ids.
    return float4(b.x, b.y, v.lin - dot(b, at), 1000 * v.id + at.x + 10 * at.y + 100 * at.z);
}
