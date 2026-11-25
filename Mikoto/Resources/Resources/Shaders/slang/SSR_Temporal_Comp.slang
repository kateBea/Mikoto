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

#include "base/SSR_Common.slang"

struct Parameters {
    Pointer Frame;
    Pointer Camera;
    uint Current;
    uint ReflectionDistance;
    uint Position;
    uint Normal;
    uint Surface;
    uint History;
    uint HistoryMetadata;
    uint Output;
    uint MetadataOutput;
    uint Sampler;
};

MKT_DECLARE_STRUCTURED_SRV buffers;
MKT_DECLARE_TEXTURE_SRV( Texture2D ) textures;
MKT_DECLARE_TEXTURE_UAV( RWTexture2D<float4> ) images;
MKT_DECLARE_SAMPLERS samplers;
MKT_DECLARE_PUSH_CONSTANT( Parameters, parameters );

float2 PreviousUv( float3 world, SsrFrame frame ) {
    float4 clip = mul( frame.PreviousViewProjection, float4( world, 1 ) );
    if ( clip.w <= 0 ) {
        return -1;
    }
    return float2( clip.x / clip.w * 0.5 + 0.5, 0.5 - clip.y / clip.w * 0.5 );
}

[shader( "compute" )]
[numthreads( 8, 8, 1 )]
void main( uint3 thread : SV_DispatchThreadID ) {
    SsrFrame frame = ( ( BufferReference<SsrFrame> )parameters.Frame )[0];
    uint2 pixel = thread.xy;
    if ( any( pixel >= uint2( frame.Screen.xy ) ) ) {
        return;
    }

    float4 current = textures[parameters.Current].Load( int3( pixel, 0 ) );
    float4 position = textures[parameters.Position].Load( int3( pixel, 0 ) );
    float4 surface = textures[parameters.Surface].Load( int3( pixel, 0 ) );
    float3 normal = normalize( textures[parameters.Normal].Load( int3( pixel, 0 ) ).xyz * 2 - 1 );
    images[parameters.Output][pixel] = current;
    images[parameters.MetadataOutput][pixel] = position.w > 0 && any( surface.rgb > 0 )
        ? float4( SsrEncodeNormal( normal ), surface.w, position.w ) : float4( 0 );

    // The first frame and misses must never sample undefined or stale history.
    if ( frame.Control.y == 0 || current.w <= 0 || position.w <= 0 ) {
        return;
    }

    CameraInfo camera = ( ( BufferReference<CameraInfo> )parameters.Camera )[0];
    float3 world = SsrWorldPosition( position.xyz, frame );
    float2 surfaceUv = PreviousUv( world, frame );
    if ( !SsrInside( surfaceUv ) ) {
        return;
    }
    int2 previousPixel = clamp( int2( surfaceUv * frame.Screen.xy ), 0, int2( frame.Screen.xy ) - 1 );
    float4 surfaceHistory = textures[parameters.HistoryMetadata].Load( int3( previousPixel, 0 ) );
    float expectedDepth = -mul( frame.PreviousView, float4( world, 1 ) ).z;
    if ( surfaceHistory.w <= 0 || abs( surfaceHistory.w - expectedDepth ) > max( frame.Trace.y * 2, expectedDepth * 0.01 ) ||
         dot( SsrDecodeNormal( surfaceHistory.xy ), normal ) < 0.85 ) {
        return;
    }

    // Reproject a virtual point behind the reflector using the mean reflection
    // distance. Receiver-depth-only reprojection smears reflections under camera translation.
    // Frostbite slide 67; also Wicked's reflection-depth temporal reprojection:
    // https://github.com/turanszkij/WickedEngine/blob/master/WickedEngine/shaders/ssr_temporalCS.hlsl
    float distance = textures[parameters.ReflectionDistance].Load( int3( pixel, 0 ) ).r;
    float3 away = camera.mProjection[3][3] != 0 ? normalize( mul( frame.InverseView, float4( 0, 0, -1, 0 ) ).xyz )
                                               : normalize( world - camera.mViewPosition.xyz );
    float2 historyUv = PreviousUv( world + away * distance, frame );
    if ( !SsrInside( historyUv ) ) {
        return;
    }
    int2 reflectedPixel = clamp( int2( historyUv * frame.Screen.xy ), 0, int2( frame.Screen.xy ) - 1 );
    float4 reflectedMetadata = textures[parameters.HistoryMetadata].Load( int3( reflectedPixel, 0 ) );
    if ( reflectedMetadata.w <= 0 || abs( reflectedMetadata.z - surface.w ) > 0.1 ||
         dot( SsrDecodeNormal( reflectedMetadata.xy ), normal ) < 0.85 ) {
        return;
    }

    float4 history = textures[parameters.History].SampleLevel( samplers[parameters.Sampler], historyUv, 0 );
    if ( history.w <= 0 ) {
        return;
    }
    float3 minimum = current.rgb;
    float3 maximum = current.rgb;
    for ( int y = -1; y <= 1; ++y ) {
        for ( int x = -1; x <= 1; ++x ) {
            int2 neighbor = clamp( int2( pixel ) + int2( x, y ), 0, int2( frame.Screen.xy ) - 1 );
            float4 sample = textures[parameters.Current].Load( int3( neighbor, 0 ) );
            if ( sample.w > 0 ) {
                minimum = min( minimum, sample.rgb );
                maximum = max( maximum, sample.rgb );
            }
        }
    }

    // No object-motion buffer is available here. Depth/normal rejection and a
    // current-neighborhood clamp limit trails from animated meshes and changing video pixels.
    history.rgb = clamp( history.rgb, minimum, maximum );
    float weight = frame.Filter.x * min( current.w, history.w );
    images[parameters.Output][pixel] = float4( lerp( current.rgb, history.rgb, weight ), current.w );
}
