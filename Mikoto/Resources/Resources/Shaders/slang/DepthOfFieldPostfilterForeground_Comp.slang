// Copyright 2026 ケイト. Licensed under the Apache License, Version 2.0.

#include "base/DepthOfField_Helpers.slang"

MKT_DECLARE_PUSH_CONSTANT( DepthOfFieldParameters, parameters );

[shader( "compute" )]
[numthreads( 8, 8, 1 )]
void main( uint3 thread : SV_DispatchThreadID ) {
    if ( any( thread.xy >= uint2( parameters.mTargetWidth, parameters.mTargetHeight ) ) ) { return; }
    float4 values[9];
    uint index = 0;
    for ( int y = -1; y <= 1; ++y ) {
        for ( int x = -1; x <= 1; ++x ) {
            values[index++] = DoFLoad( parameters.mInputs[0], int2( thread.xy ) + int2( x, y ) );
        }
    }
    dofOutputs[parameters.mOutputs[0]][thread.xy] = DoFMedian9( values );
}
