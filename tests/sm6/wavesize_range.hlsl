RWStructuredBuffer<uint> o : register(u0);
[WaveSize(16, 64)] [numthreads(64,1,1)] void main(uint i : SV_DispatchThreadID) { o[i] = WaveActiveSum(1); }
