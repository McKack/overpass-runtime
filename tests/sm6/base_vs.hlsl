cbuffer C : register(b0) { float4x4 mvp; };
struct O { float4 p : SV_Position; float2 uv : TEXCOORD0; };
O main(float3 p : POSITION, float2 uv : TEXCOORD0, uint vid : SV_VertexID) { O o; o.p = mul(mvp, float4(p,1)); o.uv = uv + vid*0.001; return o; }
