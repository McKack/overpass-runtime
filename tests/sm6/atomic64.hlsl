RWByteAddressBuffer b : register(u0); groupshared uint64_t g;
[numthreads(64,1,1)] void main(uint i : SV_DispatchThreadID) { uint64_t old; b.InterlockedAdd64(0, (uint64_t)i, old); InterlockedMax(g, (uint64_t)i); b.Store<uint64_t>(8, old); }
