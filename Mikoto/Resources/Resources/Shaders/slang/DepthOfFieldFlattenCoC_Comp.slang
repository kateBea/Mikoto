// Copyright 2026 ケイト. Licensed under the Apache License, Version 2.0.

#include "base/DepthOfField_Helpers.slang"

MKT_DECLARE_PUSH_CONSTANT( DepthOfFieldParameters, parameters );
groupshared float4 tileBounds[64];

[shader( "compute" )]
[numthreads( 8, 8, 1 )]
void main( uint3 group : SV_GroupID, uint3 local : SV_GroupThreadID ) {
    uint index = local.y * 8 + local.x;
    float coc = DoFLoad( parameters.mInputs[0], int2( group.xy * 8 + local.xy ) ).a;
    // x/y: signed foreground min/max; z/w: background min/max.
    // Include zero for pixels outside a layer. Otherwise a foreground/in-focus
    // edge could be mistaken for uniform foreground and use an unsafe coarse mip.
    tileBounds[index] = float4( min( coc, 0.0 ), min( coc, 0.0 ), max( coc, 0.0 ), max( coc, 0.0 ) );
    GroupMemoryBarrierWithGroupSync();
    for ( uint stride = 32; stride > 0; stride >>= 1 ) {
        if ( index < stride ) {
            float4 a = tileBounds[index];
            float4 b = tileBounds[index + stride];
            tileBounds[index] = float4( min( a.x, b.x ), max( a.y, b.y ), min( a.z, b.z ), max( a.w, b.w ) );
        }
        GroupMemoryBarrierWithGroupSync();
    }
    if ( index == 0 ) {
        dofOutputs[parameters.mOutputs[0]][group.xy] = tileBounds[0];
    }
}
