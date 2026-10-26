// Mesh shader for amplification shaders in the corpus: takes their payload (struct P { uint x; }).
struct P { uint x; };
struct V { float4 p : SV_Position; };
[outputtopology("triangle")] [numthreads(1,1,1)]
void main(in payload P pl, out vertices V v[3], out indices uint3 t[1]) { SetMeshOutputCounts(3, 1); v[0].p = pl.x; v[1].p = 1; v[2].p = 2; t[0] = uint3(0,1,2); }
