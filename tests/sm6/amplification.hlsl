struct P { uint x; }; groupshared P pl;
[numthreads(1,1,1)] void main(uint i : SV_GroupID) { pl.x = i; DispatchMesh(1, 1, 1, pl); }
