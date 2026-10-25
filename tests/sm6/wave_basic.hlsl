RWStructuredBuffer<uint> o : register(u0);
[numthreads(64,1,1)] void main(uint i : SV_DispatchThreadID) { uint v = o[i]; uint s = WaveActiveSum(v); uint f = WaveReadLaneFirst(v); uint4 b = WaveActiveBallot(v > 3); o[i] = s + f + WaveGetLaneIndex() + countbits(b.x) + WaveGetLaneCount(); }
