// tests/atomic12.cpp: 64-bit atomics (SM 6.6) on a raw buffer and on groupshared memory, 16 groups of 64 threads.
// Built twice: with -DSPLIT only the operations that need no lock (add/or/xor, unsigned max without a result),
// without it everything, so all 64-bit atomics in the shader go through the lock words. With -DHEAP as well, the
// buffer is reached through ResourceDescriptorHeap[0] instead of a root UAV.
#ifdef HEAP
#define RS "RootFlags(CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED)"
#else
#define RS "UAV(u0)"
RWByteAddressBuffer b : register(u0);
#endif
groupshared uint64_t g_add, g_xor, g_cas;
groupshared int64_t g_max;

[RootSignature(RS)] [numthreads(64, 1, 1)]
void main(uint i : SV_DispatchThreadID, uint l : SV_GroupIndex, uint grp : SV_GroupID) {
#ifdef HEAP
  RWByteAddressBuffer b = ResourceDescriptorHeap[0];
#endif
  if (l == 0) { g_add = 0; g_xor = 0; g_cas = 0; g_max = -1000; }
  GroupMemoryBarrierWithGroupSync();
  b.InterlockedAdd64(0, 0x100000001ull);               // slot 0: 1024 * 0x100000001
  b.InterlockedAdd64(8, 1);                             // slot 1: starts at 0xffffff00, carries into the high half
  b.InterlockedOr64(16, 1ull << (i & 63));              // slot 2: all ones
  b.InterlockedMax64(24, (uint64_t)i * 0x100000001ull); // slot 3: 1023 * 0x100000001
  InterlockedAdd(g_add, 1ull << 31);
  InterlockedXor(g_xor, 1ull << (32 + (l & 31)));
#ifndef SPLIT
  int64_t unused;
  b.InterlockedMin64(32, -(int64_t)i, unused);          // slot 4: -1023
  uint64_t seen, prev;
  [loop] for (;;) {                                     // slot 5: a CAS-loop counter, 1024
    seen = b.Load<uint64_t>(40);
    b.InterlockedCompareExchange64(40, seen, seen + 1, prev);
    if (prev == seen) break;
  }
  uint64_t ticket;
  b.InterlockedAdd64(48, 1, ticket);                    // slot 6: 1024; every ticket 0..1023 handed out once
  b.Store<uint64_t>(128 + (uint)ticket * 8, i + 1);         // slots 16..1039
  uint64_t xprev;
  b.InterlockedExchange64(56, i + 0x100000000ull, xprev); // slot 7: some i + 2^32
  [loop] for (;;) {
    uint64_t gs = g_cas, gp;
    InterlockedCompareExchange(g_cas, gs, gs + 1, gp);
    if (gp == gs) break;
  }
  InterlockedMax(g_max, (int64_t)l - 100);
#endif
  GroupMemoryBarrierWithGroupSync();
  if (grp == 0 && l == 0) {
    b.Store<uint64_t>(64, g_add);                       // slot 8: 64 << 31
    b.Store<uint64_t>(72, g_xor);                       // slot 9: 0 (each bit flipped twice)
    b.Store<uint64_t>(80, g_cas);                       // slot 10: 64
    b.Store<int64_t>(88, g_max);                        // slot 11: -37
  }
}
