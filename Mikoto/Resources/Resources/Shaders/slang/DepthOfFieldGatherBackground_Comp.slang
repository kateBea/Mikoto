// Copyright 2026 ケイト. Licensed under the Apache License, Version 2.0.

#include "base/DepthOfField_Helpers.slang"

MKT_DECLARE_PUSH_CONSTANT( DepthOfFieldParameters, parameters );

struct BackgroundBucket {
    float3 color;
    float weight;
    float coc;
    float found;
};

void AccumulateBackground( inout BackgroundBucket bucket, float4 sample, float weight, float intersection ) {
    bucket.color += sample.rgb * weight;
    bucket.weight += weight;
    bucket.coc += sample.a * weight;
    bucket.found += intersection;
}

[shader( "compute" )]
[numthreads( 8, 8, 1 )]
void main( uint3 thread : SV_DispatchThreadID ) {
    if ( any( thread.xy >= uint2( parameters.mTargetWidth, parameters.mTargetHeight ) ) ) { return; }
    float4 bounds = DoFLoad( parameters.mInputs[4], int2( thread.xy / 8 ) );
    float radius = max( bounds.w, 0.0 );
    if ( radius < 0.25 ) {
        dofOutputs[parameters.mOutputs[0]][thread.xy] = 0.0;
        return;
    }

    uint rings = max( parameters.mRingCount, 1u );
    uint mip = DoFGatherMip( radius, bounds, parameters );
    BackgroundBucket previous = ( BackgroundBucket )0;
    // Larger CoCs describe farther surfaces. Traversing outer rings first
    // approximately sorts gathered background surfaces from far to near.
    for ( int ring = int( rings ) - 1; ring >= 0; --ring ) {
        uint count = 6 * ( uint( ring ) + 1 );
        float inner = float( ring ) / float( rings );
        float outer = float( ring + 1 ) / float( rings );
        float radial = sqrt( ( inner * inner + outer * outer ) * 0.5 );
        float sampleArea = radius * radius * ( outer * outer - inner * inner ) / float( count );
        float borderingRadius = ( float( ring ) + 2.5 ) * radius / ( float( rings ) + 0.5 );
        BackgroundBucket current = ( BackgroundBucket )0;
        for ( uint i = 0; i < count; ++i ) {
            float angle = 2.0 * kDoFPi * ( ( i + 0.5 ) / float( count ) + ( ring & 1 ) * 0.5 / float( count ) );
            float2 offset = DoFApertureSample( float2( cos( angle ), sin( angle ) ) * radial, parameters ) * radius;
            float4 sample = DoFLoadMip( float2( thread.xy ) + offset, mip, parameters );
            if ( sample.a <= 0.01 ) { continue; }
            float intersection = saturate( sample.a - radial * radius + 0.5 );
            float weight = intersection * sampleArea / max( sample.a * sample.a, 0.25 );
            if ( sample.a <= borderingRadius ) {
                AccumulateBackground( current, sample, weight, intersection );
            } else {
                AccumulateBackground( previous, sample, weight, intersection );
            }
        }

        // A nearer ring can attenuate previously accumulated, more distant geometry.
        // This is the two-bucket merge described in slides 37-40.
        float currentAverage = current.coc / max( current.weight, 1e-5 );
        float previousAverage = previous.coc / max( previous.weight, 1e-5 );
        float opacity = saturate( current.found / float( count ) );
        float factor = previous.weight > 0.0 ? 1.0 - opacity * saturate( previousAverage - currentAverage ) : 0.0;
        previous.color = previous.color * factor + current.color;
        previous.coc = previous.coc * factor + current.coc;
        previous.weight = previous.weight * factor + current.weight;
    }

    float alpha = saturate( previous.weight );
    float3 color = previous.color / max( previous.weight, 1e-5 );
    dofOutputs[parameters.mOutputs[0]][thread.xy] = float4( DoFClampColor( color ) * alpha, alpha );
    // TODO: publish CoC mean/variance for highlight visibility once additive scatter is available.
}
