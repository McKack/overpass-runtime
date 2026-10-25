RWStructuredBuffer<uint> o : register(u0);
[numthreads(64,1,1)] void main(uint i : SV_DispatchThreadID) { uint v = o[i]; uint r = WavePrefixSum(v) + WaveActiveMin(v) + WaveActiveMax(v) + WaveReadLaneAt(v, 3) + WaveActiveBitOr(v) + WavePrefixCountBits(v & 1); if (WaveActiveAnyTrue(v == 0) && !WaveActiveAllTrue(v == 7)) r += WaveIsFirstLane(); o[i] = r; }
