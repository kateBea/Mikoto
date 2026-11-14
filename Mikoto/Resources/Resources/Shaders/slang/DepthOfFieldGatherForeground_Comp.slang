// Copyright 2026 ケイト. Licensed under the Apache License, Version 2.0.

#include "base/DepthOfField_Helpers.slang"

MKT_DECLARE_PUSH_CONSTANT( DepthOfFieldParameters, parameters );

[shader( "compute" )]
[numthreads( 8, 8, 1 )]
void main( uint3 thread : SV_DispatchThreadID ) {
    if ( any( thread.xy >= uint2( parameters.mTargetWidth, parameters.mTargetHeight ) ) ) { return; }
    float4 bounds = DoFLoad( parameters.mInputs[4], int2( thread.xy / 8 ) );
    float radius = max( -bounds.x, 0.0 );
    if ( radius < 0.25 ) {
        dofOutputs[parameters.mOutputs[0]][thread.xy] = 0.0;
        return;
    }

    uint mip = DoFGatherMip( radius, bounds, parameters );
    uint rings = max( parameters.mRingCount, 1u );
    float3 color = 0.0;
    float coverage = 0.0;
    float weightSum = 0.0;
    for ( uint ring = 0; ring < rings; ++ring ) {
        uint count = 6 * ( ring + 1 );
        float inner = float( ring ) / float( rings );
        float outer = float( ring + 1 ) / float( rings );
        // Equal-area radial midpoint; angles remain fixed because the renderer has no TAA.
        float radial = sqrt( ( inner * inner + outer * outer ) * 0.5 );
        float sampleArea = radius * radius * ( outer * outer - inner * inner ) / float( count );
        for ( uint i = 0; i < count; ++i ) {
            float angle = 2.0 * kDoFPi * ( ( i + 0.5 ) / float( count ) + ( ring & 1 ) * 0.5 / float( count ) );
            float2 disk = float2( cos( angle ), sin( angle ) ) * radial;
            float2 offset = DoFApertureSample( disk, parameters ) * radius;
            float4 sample = DoFLoadMip( float2( thread.xy ) + offset, mip, parameters );
            if ( sample.a >= -0.01 ) { continue; }
            float oppositeCoC = DoFLoadMip( float2( thread.xy ) - offset, mip, parameters ).a;
            // Foreground CoC is negative. The opposed sample widens the intersection
            // footprint without importing background RGB into this layer.
            float mirroredRadius = max( -min( sample.a, oppositeCoC ), 0.5 );
            float intersection = saturate( mirroredRadius - radial * radius + 0.5 );
            float weight = intersection * sampleArea / max( sample.a * sample.a, 0.25 );
            color += sample.rgb * weight;
            weightSum += weight;
            coverage += weight;
        }
    }
    float opacity = saturate( coverage );
    float3 normalized = color / max( weightSum, 1e-5 );
    // Premultiplied RGB is required when this coverage later blends over the background.
    dofOutputs[parameters.mOutputs[0]][thread.xy] = float4( DoFClampColor( normalized ) * opacity, opacity );
}
