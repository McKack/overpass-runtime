float4 main(float4 p : SV_Position, uint v : SV_ViewID) : SV_Target { return float4(v, 0, 0, 1); }
