RWStructuredBuffer<uint64_t> o : register(u0);
[numthreads(64,1,1)] void main(uint i : SV_DispatchThreadID) { uint64_t v = o[i]; o[i] = (v * 0x100000001ull) >> 7; }
