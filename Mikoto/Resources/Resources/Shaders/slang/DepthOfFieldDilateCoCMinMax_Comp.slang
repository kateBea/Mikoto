// Copyright 2026 ケイト. Licensed under the Apache License, Version 2.0.

#include "base/DepthOfField_Helpers.slang"

MKT_DECLARE_PUSH_CONSTANT( DepthOfFieldParameters, parameters );

[shader( "compute" )]
[numthreads( 8, 8, 1 )]
void main( uint3 thread : SV_DispatchThreadID ) {
    int2 tile = int2( thread.xy );
    if ( any( thread.xy >= uint2( parameters.mTargetWidth, parameters.mTargetHeight ) ) ) { return; }
    float4 bounds = DoFLoad( parameters.mInputs[0], tile );
    int radius = int( ceil( parameters.mMaxRadius * 0.5 / 8.0 ) ) + 1;
    for ( int y = -radius; y <= radius; ++y ) {
        for ( int x = -radius; x <= radius; ++x ) {
            int2 source = tile + int2( x, y );
            if ( any( source < 0 ) || any( source >= int2( parameters.mTargetWidth, parameters.mTargetHeight ) ) ) { continue; }
            float4 candidate = DoFLoad( parameters.mInputs[0], source );
            // Closest separation of two 8x8 tiles. Include a pixel for reconstruction filtering.
            float separation = length( max( abs( float2( x, y ) ) - 1.0, 0.0 ) * 8.0 );
            if ( separation <= -candidate.x + 1.0 ) { bounds.x = min( bounds.x, candidate.x ); }
            if ( separation <= candidate.w + 1.0 ) { bounds.w = max( bounds.w, candidate.w ); }
        }
    }
    dofOutputs[parameters.mOutputs[0]][thread.xy] = bounds;
}
