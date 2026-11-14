// Copyright 2026 ケイト. Licensed under the Apache License, Version 2.0.
#pragma once

// PBRT PowerLightSampler's discrete PMF is part of BOTH light proposals and MIS.
// Mikoto uses a binary-search CDF and a small uniform mixture rather than an alias table.
// https://pbr-book.org/4ed/Light_Sources/Light_Sampling#PowerLightSampler
float2 LightDistribution( uint index ) {
    return LoadBufferValue<float2>( Parameters.Data, Parameters.LightDistribution + index * 8 );
}

float LightProbability( uint index, uint count ) {
    if ( count == 0 ) {
        return 0.0;
    }
    if ( Parameters.LightDistribution == 0 ) {
        return 1.0 / float( count );
    }
    // Hybrid's first indirect vertex excludes analytical lights already shaded by raster.
    uint first = count == LightCount() ? 0u : SceneData().LightCount;
    if ( index < first || index >= first + count ) {
        return 0.0;
    }
    float lower = first > 0 ? LightDistribution( first - 1 ).x : 0.0;
    float upper = LightDistribution( first + count - 1 ).x;
    return LightDistribution( index ).y / ( upper - lower );
}

uint SampleLight( float sample, bool analytical, out float probability ) {
    uint count = LightCount( analytical );
    uint first = analytical ? 0u : SceneData().LightCount;
    if ( Parameters.LightDistribution == 0 ) {
        probability = 1.0 / float( count );
        return first + min( uint( sample * count ), count - 1 );
    }
    float lower = first > 0 ? LightDistribution( first - 1 ).x : 0.0;
    float upper = LightDistribution( first + count - 1 ).x;
    float target = min( lerp( lower, upper, sample ), NextFloat( upper, false ) );
    uint low = first, high = first + count - 1;
    while ( low < high ) {
        uint middle = low + ( high - low ) / 2;
        if ( target >= LightDistribution( middle ).x ) {
            low = middle + 1;
        }
        else {
            high = middle;
        }
    }
    probability = LightProbability( low, count );
    return low;
}
