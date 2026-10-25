Texture2D t : register(t0); SamplerState s : register(s0);
float4 main(float4 p : SV_Position, float2 uv : TEXCOORD0) : SV_Target { float4 c = t.Sample(s, uv); if (c.a < 0.1) discard; return c * (1 + ddx(uv.x) + ddy(uv.y)); }
