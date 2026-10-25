Texture2D t : register(t0); SamplerState s : register(s0); RWTexture2D<float4> o : register(u0);
[numthreads(8,8,1)] void main(uint2 i : SV_DispatchThreadID) { float2 uv = i / 64.0; o[i] = t.Sample(s, uv) + ddx(uv.x); }
