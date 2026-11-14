// Copyright 2026 ケイト. Licensed under the Apache License, Version 2.0.

#include "base/DepthOfField_Helpers.slang"

MKT_DECLARE_PUSH_CONSTANT( DepthOfFieldParameters, parameters );

// Background upsampling is CoC-aware; foreground coverage must be allowed to cross silhouettes.
float4 UpsampleBackground( float2 halfPixel, float fullCoC ) {
    int2 origin = int2( floor( halfPixel ) );
    float2 fraction = frac( halfPixel );
    float4 sum = 0.0;
    float weightSum = 0.0;
    for ( uint i = 0; i < 4; ++i ) {
        int2 delta = int2( i & 1, i >> 1 );
        float2 axisWeight = lerp( 1.0 - fraction, fraction, float2( delta ) );
        float coc = DoFLoad( parameters.mInputs[7], origin + delta ).a * 2.0;
        float bilateral = rcp( 1.0 + abs( max( coc, 0.0 ) - max( fullCoC, 0.0 ) ) );
        float weight = axisWeight.x * axisWeight.y * bilateral;
        sum += DoFLoad( parameters.mInputs[3], origin + delta ) * weight;
        weightSum += weight;
    }
    return sum / max( weightSum, 1e-5 );
}

float3 GatherFullResolutionFocus( int2 pixel, float4 original, float centerCoC ) {
    if ( abs( centerCoC ) < 0.5 || abs( centerCoC ) >= 4.0 ) { return original.rgb; }

    float3 color = 0.0;
    float sum = 0.0;
    // Fixed samples replace the presentation's stochastic sampling plus main TAA.
    // Pixel centers and the soft aperture intersection retain subpixel blur radii.
    for ( int y = -4; y <= 4; ++y ) {
        for ( int x = -4; x <= 4; ++x ) {
            float2 offset = float2( x, y );
            if ( dot( offset, offset ) > 16.0 ) { continue; }
            float sampleCoC = DoFLoad( parameters.mInputs[1], pixel + int2( x, y ) ).x;
            float radius = abs( sampleCoC );
            if ( radius >= 4.0 ) { continue; }
            float2 lutUV = offset / 8.0 + 0.5;
            // The LUT stores normalized aperture distance. Multiplication by four restores full pixels.
            float distance = DoFSample( parameters.mInputs[6], parameters.mSampler, lutUV ).x * 4.0;
            float intersection = saturate( max( radius, 0.5 ) - distance + 0.5 );
            float depthWeight = saturate( 1.0 - max( sampleCoC - centerCoC, 0.0 ) / max( abs( centerCoC ), 1.0 ) );
            float weight = intersection * depthWeight / max( radius * radius, 0.25 );
            color += DoFLoad( parameters.mInputs[0], pixel + int2( x, y ) ).rgb * weight;
            sum += weight;
        }
    }
    float3 result = sum > 1e-5 ? color / sum : original.rgb;

    // Keep this precise gather inside the stable half-resolution neighborhood's bounds.
    float2 halfPixel = ( float2( pixel ) + 0.5 ) * 0.5 - 0.5;
    int2 origin = int2( floor( halfPixel ) );
    float3 low = original.rgb;
    float3 high = original.rgb;
    for ( uint i = 0; i < 4; ++i ) {
        float4 focus = DoFLoad( parameters.mInputs[5], origin + int2( i & 1, i >> 1 ) );
        if ( focus.a > 0.0 ) {
            low = min( low, focus.rgb );
            high = max( high, focus.rgb );
        }
    }
    return clamp( result, low, high );
}

[shader( "compute" )]
[numthreads( 8, 8, 1 )]
void main( uint3 thread : SV_DispatchThreadID ) {
    uint2 pixel = thread.xy;
    if ( any( pixel >= uint2( parameters.mWidth, parameters.mHeight ) ) ) { return; }
    float4 original = DoFLoad( parameters.mInputs[0], int2( pixel ) );
    if ( parameters.mEnabled == 0 ) {
        dofOutputs[parameters.mOutputs[0]][pixel] = original;
        return;
    }
    float coc = DoFLoad( parameters.mInputs[1], int2( pixel ) ).x;
    if ( parameters.mVisualize != 0 ) {
        float amount = saturate( abs( coc ) / max( parameters.mMaxRadius, 1.0 ) );
        float3 layerColor = coc < 0.0 ? float3( 0.05, 1.0, 0.15 ) : float3( 0.05, 0.3, 1.0 );
        dofOutputs[parameters.mOutputs[0]][pixel] = float4( lerp( float3( 0.03 ), layerColor, amount ), 1 );
        return;
    }

    // Physical 2:1 coordinates also handle odd viewport dimensions without stretching the half-resolution grid.
    float2 halfPixel = ( float2( pixel ) + 0.5 ) * 0.5 - 0.5;
    float2 halfUV = ( halfPixel + 0.5 ) / float2( DoFHalfSize( parameters ) );
    float4 foreground = DoFSample( parameters.mInputs[2], parameters.mSampler, halfUV );
    float4 background = UpsampleBackground( halfPixel, coc );
    float4 fill = DoFSample( parameters.mInputs[4], parameters.mSampler, halfUV );
    float3 focus = GatherFullResolutionFocus( int2( pixel ), original, coc );

    float backgroundOpacity = saturate( background.a );
    float3 backgroundColor = background.rgb / max( backgroundOpacity, 1e-5 );
    float farBlur = smoothstep( 1.0, 4.0, max( coc, 0.0 ) );
    float3 color = lerp( focus, backgroundColor, farBlur * backgroundOpacity );
    color = lerp( color, fill.rgb, saturate( fill.a ) );
    color = foreground.rgb + color * ( 1.0 - saturate( foreground.a ) );
    dofOutputs[parameters.mOutputs[0]][pixel] = float4( DoFClampColor( color ), original.a );
}
