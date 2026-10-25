RWStructuredBuffer<uint> o : register(u0);
[numthreads(64,1,1)] void main(uint i : SV_DispatchThreadID) { uint4 m = WaveMatch(o[i] & 3); o[i] = WaveMultiPrefixSum(o[i], m) + countbits(m.x); }
