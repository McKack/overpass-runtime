struct O { float4 p : SV_Position; float2 uv : TEXCOORD0; nointerpolation float4 c : COLOR; };
O main(float3 p : POSITION, float2 uv : TEXCOORD0) { O o; o.p = float4(p, 1); o.uv = uv; o.c = float4(uv, 0, 1); return o; }
