Texture2D<float> t : register(t0); SamplerComparisonState c : register(s0);
float4 main(float4 p : SV_Position) : SV_Target { return t.SampleCmpBias(c, p.xy, 0.5, 1) + t.SampleCmpGrad(c, p.xy, 0.5, 0.1, 0.1); }
