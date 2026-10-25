StructuredBuffer<float4> inb : register(t0); RWStructuredBuffer<float4> outb : register(u0); RWByteAddressBuffer counter : register(u1);
groupshared float4 sh[64];
[numthreads(64,1,1)] void main(uint i : SV_DispatchThreadID, uint g : SV_GroupIndex) { sh[g] = inb[i]; GroupMemoryBarrierWithGroupSync(); uint old; counter.InterlockedAdd(0, 1, old); outb[i] = sh[63 - g] + old; }
