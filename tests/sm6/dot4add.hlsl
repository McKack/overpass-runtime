RWStructuredBuffer<uint> o : register(u0); RWStructuredBuffer<float> f : register(u1);
[numthreads(64,1,1)] void main(uint i : SV_DispatchThreadID) { o[i] = dot4add_u8packed(o[i], 0x01020304u, 5u); f[i] = dot2add(half2(1,2), half2(3,4), f[i]); }
