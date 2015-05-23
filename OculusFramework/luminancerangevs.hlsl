void main(in uint vertIdx : SV_VertexID, out float4 position : SV_POSITION, out float2 uv : TEXCOORD0) { 
    // 0 -> -1.0f, 1.0f, 0.0f
    // 1 -> 3.0f, 1.0f, 0.0f
    // 2 -> -1.0f, -3.0f, 0.0f
    position = float4(-1.0f + 4.0f * (vertIdx % 2), 1.0f - 4.0f * (vertIdx / 2), 0.0f, 1.0f);
    // 0 -> 0.0f, 0.0f
    // 1 -> 2.0f, 0.0f
    // 2 -> 0.0f, 2.0f
    uv = float2(2.0f * (vertIdx % 2), 2.0f * (vertIdx / 2));
}