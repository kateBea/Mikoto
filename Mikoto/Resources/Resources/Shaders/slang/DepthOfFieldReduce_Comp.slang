// Copyright 2026 ケイト. Licensed under the Apache License, Version 2.0.

#include "base/DepthOfField_Helpers.slang"

MKT_DECLARE_PUSH_CONSTANT( DepthOfFieldParameters, parameters );

[shader( "compute" )]
[numthreads( 8, 8, 1 )]
void main( uint3 thread : SV_DispatchThreadID ) {
    if ( any( thread.xy >= uint2( parameters.mTargetWidth, parameters.mTargetHeight ) ) ) { return; }
    float4 samples[4];
    float minimum = 65504.0;
    float maximum = -65504.0;
    for ( uint i = 0; i < 4; ++i ) {
        samples[i] = DoFLoad( parameters.mInputs[0], int2( thread.xy * 2 + uint2( i & 1, i >> 1 ) ) );
        minimum = min( minimum, samples[i].a );
        maximum = max( maximum, samples[i].a );
    }
    float referenceCoC = minimum < -0.25 ? minimum : maximum;
    uint quarterWidth, quarterHeight;
    dofImages[parameters.mInputs[1]].GetDimensions( quarterWidth, quarterHeight );
    float2 uv = ( float2( thread.xy ) + 0.5 ) * float( 1u << ( parameters.mMipLevel - 1 ) ) / float2( quarterWidth, quarterHeight );
    float3 referenceColor = DoFSample( parameters.mInputs[1], parameters.mSampler, uv ).rgb;
    float referenceLuminance = DoFLuminance( referenceColor );
    float3 color = 0.0;
    float sum = 0.0;
    for ( uint i = 0; i < 4; ++i ) {
        float bilateral = saturate( 1.0 - abs( samples[i].a - referenceCoC ) / max( abs( referenceCoC ), 0.5 ) );
        float hdrWeight = ( 1.0 + referenceLuminance ) / ( 1.0 + max( referenceLuminance, DoFLuminance( samples[i].rgb ) ) );
        float weight = bilateral * hdrWeight;
        color += samples[i].rgb * weight;
        sum += weight;
    }
    // TODO: extract sprites once the foreground and background scatter draws are implemented.
    // Keep every highlight in the gather mips; blacking them out here would lose their energy.
    dofOutputs[parameters.mOutputs[0]][thread.xy] = float4( color / max( sum, 1e-5 ), referenceCoC );
}
