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

// Reproject resolved reflections and reject history across geometry changes. Neighborhood
// clipping and confidence control how much previous radiance survives motion.

#include "base/SSR_Common.slang"

struct Parameters {
    Pointer mFrame;
    Pointer mCamera;
    uint mCurrent;
    uint mReflectionDistance;
    uint mPosition;
    uint mNormal;
    uint mSurface;
    uint mHistory;
    uint mHistoryMetadata;
    uint mOutput;
    uint mMetadataOutput;
    uint mSampler;
};

MKT_DECLARE_STRUCTURED_SRV buffers;
MKT_DECLARE_TEXTURE_SRV( Texture2D ) textures;
MKT_DECLARE_TEXTURE_UAV( RWTexture2D<float4> ) images;
MKT_DECLARE_SAMPLERS samplers;
MKT_DECLARE_PUSH_CONSTANT( Parameters, parameters );

// Project a current world point into the previous view and return its history coordinate. This
// provides the camera-motion mapping used before history rejection.
float2 PreviousUv( float3 world, SsrFrame frame ) {
    float4 clip = mul( frame.mPreviousViewProjection, float4( world, 1 ) );
    if ( clip.w <= 0 ) {
        return -1;
    }
    return float2( clip.x / clip.w * 0.5 + 0.5, 0.5 - clip.y / clip.w * 0.5 );
}

[shader( "compute" )]
[numthreads( kImageGroupWidth, kImageGroupHeight, 1 )]
void main( uint3 thread : SV_DispatchThreadID ) {
    SsrFrame frame = ( ( BufferReference<SsrFrame> )parameters.mFrame )[0];
    uint2 pixel = thread.xy;
    if ( any( pixel >= uint2( frame.mScreen.xy ) ) ) {
        return;
    }

    float4 current = textures[parameters.mCurrent].Load( int3( pixel, 0 ) );
    float4 position = textures[parameters.mPosition].Load( int3( pixel, 0 ) );
    float4 surface = textures[parameters.mSurface].Load( int3( pixel, 0 ) );
    float3 normal = normalize( textures[parameters.mNormal].Load( int3( pixel, 0 ) ).xyz * 2 - 1 );
    images[parameters.mOutput][pixel] = current;
    images[parameters.mMetadataOutput][pixel] = position.w > 0 && any( surface.rgb > 0 )
        ? float4( SsrEncodeNormal( normal ), surface.w, position.w ) : float4( 0 );

    // Reset frames and unsupported receivers must never sample history. A
    // stochastic miss on a valid receiver is a zero-coverage sample, not a cut.
    if ( frame.mControl.y == 0 || position.w <= 0 || surface.w > frame.mTrace.z || !any( surface.rgb > 0 ) ) {
        return;
    }

    CameraInfo camera = ( ( BufferReference<CameraInfo> )parameters.mCamera )[0];
    float3 world = SsrWorldPosition( position.xyz, frame );
    float2 surfaceUv = PreviousUv( world, frame );
    if ( !SsrInside( surfaceUv ) ) {
        return;
    }
    int2 previousPixel = clamp( int2( surfaceUv * frame.mScreen.xy ), 0, int2( frame.mScreen.xy ) - 1 );
    float4 surfaceHistory = textures[parameters.mHistoryMetadata].Load( int3( previousPixel, 0 ) );
    float expectedDepth = -mul( frame.mPreviousView, float4( world, 1 ) ).z;
    if ( surfaceHistory.w <= 0 || abs( surfaceHistory.w - expectedDepth ) > max( frame.mTrace.y * 2, expectedDepth * 0.01 ) ||
         dot( SsrDecodeNormal( surfaceHistory.xy ), normal ) < 0.85 ) {
        return;
    }

    // Reproject a virtual point behind the reflector using the mean reflection
    // distance. Receiver-depth-only reprojection smears reflections under camera translation.
    // Frostbite slide 67; also Wicked's reflection-depth temporal reprojection:
    // https://github.com/turanszkij/WickedEngine/blob/master/WickedEngine/shaders/ssr_temporalCS.hlsl
    float distance = textures[parameters.mReflectionDistance].Load( int3( pixel, 0 ) ).r;
    float3 away = camera.mProjection[3][3] != 0 ? normalize( mul( frame.mInverseView, float4( 0, 0, -1, 0 ) ).xyz )
                                               : normalize( world - camera.mViewPosition.xyz );
    float2 historyUv = PreviousUv( world + away * distance, frame );
    if ( !SsrInside( historyUv ) ) {
        return;
    }
    int2 reflectedPixel = clamp( int2( historyUv * frame.mScreen.xy ), 0, int2( frame.mScreen.xy ) - 1 );
    float4 reflectedMetadata = textures[parameters.mHistoryMetadata].Load( int3( reflectedPixel, 0 ) );
    if ( reflectedMetadata.w <= 0 || abs( reflectedMetadata.z - surface.w ) > 0.1 ||
         dot( SsrDecodeNormal( reflectedMetadata.xy ), normal ) < 0.85 ) {
        return;
    }

    float4 history = textures[parameters.mHistory].SampleLevel( samplers[parameters.mSampler], historyUv, 0 );
    if ( history.w <= 0 ) {
        return;
    }
    float3 minimum = current.w > 0 ? current.rgb : float3( 1e30 );
    float3 maximum = current.w > 0 ? current.rgb : float3( 0 );
    float minimumCoverage = saturate( current.w );
    float maximumCoverage = minimumCoverage;
    for ( int y = -kSsrHistoryClampRadius; y <= kSsrHistoryClampRadius; ++y ) {
        for ( int x = -kSsrHistoryClampRadius; x <= kSsrHistoryClampRadius; ++x ) {
            int2 neighbor = clamp( int2( pixel ) + int2( x, y ), 0, int2( frame.mScreen.xy ) - 1 );
            float4 samplePosition = textures[parameters.mPosition].Load( int3( neighbor, 0 ) );
            float4 sampleSurface = textures[parameters.mSurface].Load( int3( neighbor, 0 ) );
            float3 sampleNormal = normalize( textures[parameters.mNormal].Load( int3( neighbor, 0 ) ).xyz * 2 - 1 );
            float3 separation = SsrWorldPosition( samplePosition.xyz, frame ) - world;
            if ( samplePosition.w <= 0 || abs( sampleSurface.w - surface.w ) > 0.1 ||
                 dot( sampleNormal, normal ) < 0.85 ||
                 abs( dot( separation, normal ) ) > max( frame.mTrace.y * 2, position.w * 0.005 ) ) {
                continue;
            }
            float4 sample = textures[parameters.mCurrent].Load( int3( neighbor, 0 ) );
            minimumCoverage = min( minimumCoverage, saturate( sample.w ) );
            maximumCoverage = max( maximumCoverage, saturate( sample.w ) );
            if ( sample.w > 0 ) {
                minimum = min( minimum, sample.rgb );
                maximum = max( maximum, sample.rgb );
            }
        }
    }

    // A fully missing neighborhood immediately falls back to environment
    // lighting. Otherwise, isolated misses can reuse compatible history.
    if ( maximumCoverage <= 0 ) {
        return;
    }

    // The compositor multiplies radiance by coverage. Accumulating only RGB
    // left that multiplier noisy, especially at a reflection's fading edge.
    // Filter their product and coverage together, then unpremultiply once.
    // https://github.com/turanszkij/WickedEngine/blob/master/WickedEngine/shaders/ssr_temporalCS.hlsl
    history.rgb = clamp( history.rgb, minimum, maximum );
    history.w = clamp( history.w, minimumCoverage, maximumCoverage );
    float weight = saturate( frame.mFilter.x );
    float coverage = lerp( saturate( current.w ), history.w, weight );
    float3 radiance = lerp( current.rgb * saturate( current.w ), history.rgb * history.w, weight );
    images[parameters.mOutput][pixel] = coverage > 1e-6 ? float4( radiance / coverage, coverage ) : float4( 0 );
}
