struct V { float4 p : SV_Position; };
[outputtopology("triangle")] [numthreads(1,1,1)]
void main(out vertices V v[3], out indices uint3 t[1]) { SetMeshOutputCounts(3, 1); v[0].p = float4(0,0,0,1); v[1].p = float4(1,0,0,1); v[2].p = float4(0,1,0,1); t[0] = uint3(0,1,2); }
