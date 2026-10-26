// tests/mesh12.cpp: mesh and amplification shaders (SM 6.5) drawing into a 64x64 target.
// ms_basic (no amplification shader): two quads in the top half, red left and green right, each primitive passing
// its index to the pixel shader as alpha, plus a full-screen blue triangle marked SV_CullPrimitive.
// as_main + ms_amp: the amplification shader puts two colours in the payload and launches two mesh groups, which
// draw yellow (left) and cyan (right) quads in the bottom half. The root constant `scale` (1.0) multiplies every
// colour in the mesh and amplification stages, so unbound root arguments would show as black.
#define RS "RootConstants(num32BitConstants=1, b0)"
cbuffer Constants : register(b0) { float scale; };

struct V { float4 pos : SV_Position; float3 color : COLOR0; };
struct P { uint id : PRIM_ID; bool cull : SV_CullPrimitive; };
struct Payload { float4 color[2]; };

static const float2 kQuad[4] = { float2(0, 0), float2(1, 0), float2(0, 1), float2(1, 1) };

[RootSignature(RS)] [outputtopology("triangle")] [numthreads(11, 1, 1)]
void ms_basic(uint tid : SV_GroupThreadID, out vertices V v[11], out indices uint3 tri[5], out primitives P prim[5]) {
  SetMeshOutputCounts(11, 5);
  if (tid < 8) {
    uint quad = tid / 4;
    float2 p = kQuad[tid % 4] + float2(quad ? 0 : -1, 0); // x in [-1,0] or [0,1], y in [0,1]
    v[tid].pos = float4(p, 0, 1);
    v[tid].color = (quad ? float3(0, 1, 0) : float3(1, 0, 0)) * scale;
  } else {
    float2 p = tid == 8 ? float2(-1, -1) : tid == 9 ? float2(3, -1) : float2(-1, 3);
    v[tid].pos = float4(p, 0, 1);
    v[tid].color = float3(0, 0, 1);
  }
  if (tid < 5) {
    uint b = (tid / 2) * 4;
    tri[tid] = tid == 4 ? uint3(8, 9, 10) : (tid & 1) ? uint3(b + 1, b + 3, b + 2) : uint3(b, b + 1, b + 2);
    prim[tid].id = tid;
    prim[tid].cull = tid == 4;
  }
}

groupshared Payload payload;

[RootSignature(RS)] [numthreads(2, 1, 1)]
void as_main(uint tid : SV_GroupThreadID) {
  payload.color[tid] = (tid ? float4(0, 1, 1, 1) : float4(1, 1, 0, 1)) * scale;
  DispatchMesh(2, 1, 1, payload);
}

[RootSignature(RS)] [outputtopology("triangle")] [numthreads(4, 1, 1)]
void ms_amp(uint tid : SV_GroupThreadID, uint gid : SV_GroupID, in payload Payload pl,
            out vertices V v[4], out indices uint3 tri[2], out primitives P prim[2]) {
  SetMeshOutputCounts(4, 2);
  float2 p = kQuad[tid] + float2(gid ? 0 : -1, -1); // bottom half
  v[tid].pos = float4(p, 0, 1);
  v[tid].color = pl.color[gid].rgb;
  if (tid < 2) {
    tri[tid] = tid ? uint3(1, 3, 2) : uint3(0, 1, 2);
    prim[tid].id = 10 + gid;
    prim[tid].cull = false;
  }
}

struct PSIn { float4 pos : SV_Position; float3 color : COLOR0; nointerpolation uint id : PRIM_ID; };

[RootSignature(RS)]
float4 ps_main(PSIn i) : SV_Target { return float4(i.color, i.id / 255.0); }
