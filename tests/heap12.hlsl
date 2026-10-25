// tests/heap12.cpp: resources reached only through the descriptor heaps (SM 6.6).
#define RS "RootFlags(CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED | SAMPLER_HEAP_DIRECTLY_INDEXED), RootConstants(num32BitConstants=3, b0)"
struct Indices { uint tex; uint buf; uint smp; };
ConstantBuffer<Indices> c : register(b0);
[RootSignature(RS)] [numthreads(1, 1, 1)]
void main() {
  Texture2D<float4> t = ResourceDescriptorHeap[c.tex];
  RWStructuredBuffer<float4> o = ResourceDescriptorHeap[c.buf];
  SamplerState s = SamplerDescriptorHeap[c.smp];
  o[0] = t.SampleLevel(s, float2(0.5, 0.5), 0);
  o[1] = t.Load(int3(0, 0, 0));
}
