// Copyright 2026 ケイト. Licensed under the Apache License, Version 2.0.

#include "base/DepthOfField_Helpers.slang"

MKT_DECLARE_PUSH_CONSTANT( DepthOfFieldParameters, parameters );

[shader( "compute" )]
[numthreads( 8, 8, 1 )]
void main( uint3 thread : SV_DispatchThreadID ) {
    if ( any( thread.xy >= uint2( parameters.mTargetWidth, parameters.mTargetHeight ) ) ) { return; }
    int2 tile = int2( thread.xy );
    float4 bounds = DoFLoad( parameters.mInputs[1], tile );
    float kernel = max( -bounds.x, bounds.w );
    int radius = int( ceil( kernel / 8.0 ) ) + 1;
    float foregroundMax = -65504.0;
    float backgroundMin = 65504.0;
    for ( int y = -radius; y <= radius; ++y ) {
        for ( int x = -radius; x <= radius; ++x ) {
            int2 source = tile + int2( x, y );
            if ( any( source < 0 ) || any( source >= int2( parameters.mTargetWidth, parameters.mTargetHeight ) ) ) { continue; }
            float separation = length( max( abs( float2( x, y ) ) - 1.0, 0.0 ) * 8.0 );
            if ( separation > kernel + 1.0 ) { continue; }
            float4 candidate = DoFLoad( parameters.mInputs[0], source );
            if ( candidate.x < 0.0 ) { foregroundMax = max( foregroundMax, candidate.y ); }
            if ( candidate.w > 0.0 ) { backgroundMin = min( backgroundMin, candidate.z ); }
        }
    }
    bounds.y = foregroundMax == -65504.0 ? 0.0 : foregroundMax;
    bounds.z = backgroundMin == 65504.0 ? 0.0 : backgroundMin;
    dofOutputs[parameters.mOutputs[0]][thread.xy] = bounds;
}
