float4 main(float4 p : SV_Position) : SV_Target { return QuadAny(p.x > 10) && !QuadAll(p.y > 10) ? 1 : 0; }
