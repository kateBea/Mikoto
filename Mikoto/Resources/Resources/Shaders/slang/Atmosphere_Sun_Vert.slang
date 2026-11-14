#include "base/Base.slang"

struct VSOutput {
    float2 UV : TEXCOORD0;
    float4 Position : SV_Position;
};

[shader( "vertex" )]
VSOutput main( uint vertexID : SV_VertexID ) {
    VSOutput output;
    float2 uv = float2( ( vertexID << 1 ) & 2, vertexID & 2 );
    output.UV = uv;
    // The sun lies at the far plane. LEQUAL against the opaque/masked
    // prepass allows only background samples; never write scene depth.
    output.Position = float4( uv * 2 - 1, 1, 1 );
#if defined( MKT_SUPPORT_VULKAN ) || defined( MKT_SUPPORT_D3D12 )
    output.Position.y *= -1;
#endif
    return output;
}
