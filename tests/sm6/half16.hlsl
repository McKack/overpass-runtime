RWStructuredBuffer<float16_t4> o : register(u0); RWStructuredBuffer<uint16_t> p : register(u1);
[numthreads(64,1,1)] void main(uint i : SV_DispatchThreadID) { float16_t4 v = o[i]; o[i] = v * (float16_t)0.5 + (float16_t)1; p[i] = (uint16_t)(p[i] * 3); }
