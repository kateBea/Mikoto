//    Copyright 2026 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Reconstruct reflection radiance from nearby stochastic hits. Surface agreement and BRDF/PDF
// weights reject unrelated samples while reducing trace noise.

#include "base/SSR_Common.slang"

struct Parameters {
    Pointer mFrame;
    Pointer mCamera;
    uint mColorMips[SsrMaxMipCount];
    uint mMipCount;
    uint mPosition;
    uint mNormal;
    uint mSurface;
    uint mHits;
    uint mDirectionPdf;
    uint mOutput;
    uint mReflectionDistance;
    uint mSampler;
};

MKT_DECLARE_STRUCTURED_SRV buffers;
MKT_DECLARE_TEXTURE_SRV( Texture2D ) textures;
MKT_DECLARE_TEXTURE_UAV( RWTexture2D<float4> ) images;
MKT_DECLARE_SAMPLERS samplers;
MKT_DECLARE_PUSH_CONSTANT( Parameters, parameters );

// Choose a reflected-radiance mip from the hit footprint and roughness. The color pyramid
// supplies prefiltered samples rather than a single sharp texel.
float3 FilteredRadiance( float2 uv, float mip ) {
    mip = clamp( mip, 0, float( parameters.mMipCount - 1 ) );
    uint low = uint( floor( mip ) );
    uint high = min( low + 1, parameters.mMipCount - 1 );
    float3 a = textures[NonUniformResourceIndex( parameters.mColorMips[low] )].SampleLevel( samplers[parameters.mSampler], uv, 0 ).rgb;
    float3 b = textures[NonUniformResourceIndex( parameters.mColorMips[high] )].SampleLevel( samplers[parameters.mSampler], uv, 0 ).rgb;
    return lerp( a, b, frac( mip ) );
}

[shader( "compute" )]
[numthreads( kImageGroupWidth, kImageGroupHeight, 1 )]
void main( uint3 thread : SV_DispatchThreadID ) {
    SsrFrame frame = ( ( BufferReference<SsrFrame> )parameters.mFrame )[0];
    uint2 pixel = thread.xy;
    if ( any( pixel >= uint2( frame.mScreen.xy ) ) ) {
        return;
    }

    images[parameters.mOutput][pixel] = 0;
    images[parameters.mReflectionDistance][pixel] = 0;
    float4 position = textures[parameters.mPosition].Load( int3( pixel, 0 ) );
    float4 surface = textures[parameters.mSurface].Load( int3( pixel, 0 ) );
    if ( position.w <= 0 || surface.w > frame.mTrace.z || !any( surface.rgb > 0 ) ) {
        return;
    }

    CameraInfo camera = ( ( BufferReference<CameraInfo> )parameters.mCamera )[0];
    float3 world = SsrWorldPosition( position.xyz, frame );
    float3 view = camera.mProjection[3][3] != 0 ? normalize( mul( frame.mInverseView, float4( 0, 0, 1, 0 ) ).xyz )
                                               : normalize( camera.mViewPosition.xyz - world );
    float3 normal = normalize( textures[parameters.mNormal].Load( int3( pixel, 0 ) ).xyz * 2 - 1 );
    float3 color = 0;
    float weightSum = 0;
    float hitWeight = 0;
    float reflectedDistance = 0;
    float2 tracePosition = ( float2( pixel ) + 0.5 ) * frame.mScreen.zw / frame.mScreen.xy;
    float radius = lerp( 0.75, 3.0, surface.w );
    float angle = SsrSequence( pixel, frame.mControl.x ).x * 6.28318530718;
    uint count = uint( frame.mFilter.y );

    for ( uint index = 0; index < count; ++index ) {
        float sampleRadius = index == 0 ? 0 : sqrt( float( index ) / max( float( count - 1 ), 1 ) ) * radius;
        float phi = angle + float( index ) * 2.39996323;
        int2 neighbor = clamp( int2( floor( tracePosition + sampleRadius * float2( cos( phi ), sin( phi ) ) ) ),
                               0, int2( frame.mScreen.zw ) - 1 );
        float4 ray = textures[parameters.mDirectionPdf].Load( int3( neighbor, 0 ) );
        if ( ray.w <= 0 ) {
            continue;
        }

        uint2 sourcePixel = uint2( neighbor );
        if ( any( frame.mScreen.xy != frame.mScreen.zw ) ) {
            sourcePixel = min( sourcePixel * 2 + uint2( frame.mControl.x & 1u, ( frame.mControl.x >> 1u ) & 1u ), uint2( frame.mScreen.xy ) - 1 );
        }
        float4 sourcePosition = textures[parameters.mPosition].Load( int3( sourcePixel, 0 ) );
        float3 sourceNormal = normalize( textures[parameters.mNormal].Load( int3( sourcePixel, 0 ) ).xyz * 2 - 1 );
        float3 sourceWorld = SsrWorldPosition( sourcePosition.xyz, frame );
        if ( sourcePosition.w <= 0 || dot( sourceNormal, normal ) < 0.5 ||
             abs( dot( sourceWorld - world, normal ) ) > max( frame.mTrace.y * 2, position.w * 0.005 ) ) {
            continue;
        }

        float4 hit = textures[parameters.mHits].Load( int3( neighbor, 0 ) );
        float3 light = ray.xyz;
        float3 hitPosition = 0;
        if ( hit.w > 0 ) {
            int2 hitPixel = clamp( int2( hit.xy * frame.mScreen.xy ), 0, int2( frame.mScreen.xy ) - 1 );
            hitPosition = textures[parameters.mPosition].Load( int3( hitPixel, 0 ) ).xyz;
            light = normalize( SsrWorldPosition( hitPosition, frame ) - world );
        }

        float weight = SsrRayWeight( normal, view, light, surface.w, ray.w );
        weightSum += weight;
        if ( hit.w <= 0 || weight <= 0 ) {
            continue;
        }

        // Cone footprint grows with travel distance and shrinks at contacts.
        // Roughness changes the directions as well as this prefilter footprint.
        float distance = length( hitPosition - position.xyz );
        float projectedScale = abs( camera.mProjection[1][1] ) * frame.mScreen.y * 0.5;
        float footprint = surface.w * surface.w * distance * projectedScale /
                          max( -hitPosition.z * sqrt( float( count ) ), 0.01 );
        footprint *= lerp( 0.5, 1.0, frame.mTrace.w );
        float mip = log2( max( footprint, 1.0 ) );
        float confidence = hit.w;
        color += FilteredRadiance( hit.xy, mip ) * weight * confidence;
        hitWeight += weight * confidence;
        reflectedDistance += distance * weight * confidence;
    }

    if ( hitWeight > 1e-8 && weightSum > 1e-8 ) {
        // Frostbite ratio estimator: normalize by the local BRDF/PDF weights,
        // then apply PBR's own pre-integrated response captured at full resolution.
        images[parameters.mOutput][pixel] = float4( color / hitWeight * surface.rgb, saturate( hitWeight / weightSum ) );
        images[parameters.mReflectionDistance][pixel] = float4( reflectedDistance / hitWeight, 0, 0, 0 );
    }
}
