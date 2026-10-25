Texture2D<float> t : register(t0); Texture2D<uint> u : register(t1); SamplerComparisonState c : register(s0); SamplerState s : register(s1);
float4 main(float4 p : SV_Position) : SV_Target { return float4(t.SampleCmpLevel(c, p.xy, 0.5, 1), (float)u.GatherRaw(s, p.xy).x, 0, 1); }
