cbuffer C : register(b0) { uint texIndex; uint bufIndex; uint smpIndex; };
[numthreads(8,8,1)] void main(uint2 i : SV_DispatchThreadID) { Texture2D<float4> t = ResourceDescriptorHeap[texIndex]; RWTexture2D<float4> u = ResourceDescriptorHeap[bufIndex]; SamplerState s = SamplerDescriptorHeap[smpIndex]; u[i] = t.SampleLevel(s, i / 64.0, 0); }
