Texture2D t : register(t0); SamplerState s : register(s0); cbuffer C : register(b0) { int2 off; };
float4 main(float4 p : SV_Position) : SV_Target { return t.Sample(s, p.xy, off); }
