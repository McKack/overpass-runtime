RWTexture2DMS<float4, 4> o : register(u0);
[numthreads(8,8,1)] void main(uint2 i : SV_DispatchThreadID) { o.sample[1][i] = float4(i, 0, 1); }
