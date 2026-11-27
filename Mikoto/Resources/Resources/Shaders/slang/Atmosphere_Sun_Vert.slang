// Generate the fullscreen primitive for the sun composition pass. The fragment shader
// reconstructs the camera ray so the solar disk follows the same projection as the sky.

#include "base/Base.slang"

struct VSOutput {
    float2 mUV : TEXCOORD0;
    float4 mPosition : SV_Position;
};

[shader( "vertex" )]
VSOutput main( uint vertexID : SV_VertexID ) {
    VSOutput output;
    float2 uv = float2( ( vertexID << 1 ) & 2, vertexID & 2 );
    output.mUV = uv;
    // The sun lies at the far plane. LEQUAL against the opaque/masked
    // prepass allows only background samples; never write scene depth.
    output.mPosition = float4( uv * 2 - 1, 1, 1 );
#if defined( MKT_SUPPORT_VULKAN ) || defined( MKT_SUPPORT_D3D12 )
    output.mPosition.y *= -1;
#endif
    return output;
}
