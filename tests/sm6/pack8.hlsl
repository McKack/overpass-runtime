RWStructuredBuffer<uint> o : register(u0);
[numthreads(64,1,1)] void main(uint i : SV_DispatchThreadID) { uint8_t4_packed p = (uint8_t4_packed)o[i]; uint4 u = unpack_u8u32(p); o[i] = (uint)pack_clamp_u8(int4(u) - 1); }
