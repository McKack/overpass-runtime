float4 main(float3 b : SV_Barycentrics, nointerpolation float4 c : COLOR) : SV_Target { return float4(b, 1) * GetAttributeAtVertex(c, 1); }
