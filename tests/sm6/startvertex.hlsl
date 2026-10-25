float4 main(uint sv : SV_StartVertexLocation, uint si : SV_StartInstanceLocation) : SV_Position { return float4(sv, si, 0, 1); }
