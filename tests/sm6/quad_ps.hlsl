float4 main(float4 p : SV_Position) : SV_Target { float x = p.x; return float4(QuadReadAcrossX(x), QuadReadAcrossY(x), QuadReadAcrossDiagonal(x), QuadReadLaneAt(x, 2)); }
